#include "AndroidProcessMemoryReader.hpp"
#include "../diagnostics/AndroidTraceSection.hpp"
#include <array>
#include <cerrno>
#include <charconv>
#include <fcntl.h>
#include <limits>
#include <malloc.h>
#include <stdexcept>
#include <sys/resource.h>
#include <unistd.h>

namespace helengine::android {
    AndroidProcessMemoryReader::AndroidProcessMemoryReader() {
        const long size=sysconf(_SC_PAGESIZE);
        if (size<=0) throw std::runtime_error("Android memory diagnostics could not read the system page size.");
        PageSize=static_cast<uint64_t>(size);
    }
    uint64_t AndroidProcessMemoryReader::Multiply(uint64_t count,uint64_t unit) {
        if (count>std::numeric_limits<uint64_t>::max()/unit) throw std::overflow_error("Android memory diagnostic byte count overflowed.");
        return count*unit;
    }
    void AndroidProcessMemoryReader::ParseStatm(const char* data,size_t length,uint64_t* values) {
        const char* cursor=data; const char* end=data+length;
        for (size_t index=0;index<7;++index) {
            while (cursor<end && (*cursor==' ' || *cursor=='\t' || *cursor=='\n' || *cursor=='\r')) ++cursor;
            const auto result=std::from_chars(cursor,end,values[index]);
            if (result.ec!=std::errc{} || result.ptr==cursor) throw std::runtime_error("Android process memory counters are malformed.");
            cursor=result.ptr;
            if (cursor<end && *cursor!=' ' && *cursor!='\t' && *cursor!='\n' && *cursor!='\r') throw std::runtime_error("Android process memory counter has an invalid suffix.");
        }
        while (cursor<end && (*cursor==' ' || *cursor=='\t' || *cursor=='\n' || *cursor=='\r')) ++cursor;
        if (cursor!=end) throw std::runtime_error("Android process memory counters have unexpected trailing data.");
    }
    AndroidProcessMemorySample AndroidProcessMemoryReader::Capture(bool includeHeap) const {
        const AndroidTraceSection trace("HelEngine.Memory.Capture");
        int descriptor;
        do { descriptor=open("/proc/self/statm",O_RDONLY|O_CLOEXEC); } while (descriptor<0 && errno==EINTR);
        if (descriptor<0) throw std::runtime_error("Android memory diagnostics could not open /proc/self/statm.");
        std::array<char,256> buffer{}; size_t length=0; bool failed=false;
        while (length<buffer.size()) {
            const auto count=read(descriptor,buffer.data()+length,buffer.size()-length);
            if (count<0) { if (errno==EINTR) continue; failed=true; break; }
            if (count==0) break;
            length+=static_cast<size_t>(count);
        }
        // Linux closes the descriptor even if close reports EINTR; retrying can close an unrelated descriptor.
        const int closeResult=close(descriptor);
        if (failed || length==0 || length==buffer.size() || (closeResult<0 && errno!=EINTR)) throw std::runtime_error("Android process memory counter read failed.");
        std::array<uint64_t,7> pages{}; ParseStatm(buffer.data(),length,pages.data());
        struct rusage usage{};
        if (getrusage(RUSAGE_SELF,&usage)!=0 || usage.ru_maxrss<0 || usage.ru_minflt<0 || usage.ru_majflt<0) throw std::runtime_error("Android process resource counters are unavailable or invalid.");
        AndroidProcessMemorySample sample{};
        sample.VirtualBytes=Multiply(pages[0],PageSize); sample.ResidentBytes=Multiply(pages[1],PageSize);
        sample.PeakResidentBytes=Multiply(static_cast<uint64_t>(usage.ru_maxrss),1024);
        const auto minor=static_cast<uint64_t>(usage.ru_minflt),major=static_cast<uint64_t>(usage.ru_majflt);
        if (minor>std::numeric_limits<uint64_t>::max()-major) throw std::overflow_error("Android process page-fault count overflowed.");
        sample.PageFaultCount=minor+major;
        if (includeHeap) {
            const auto heap=mallinfo();
            sample.NativeHeapAllocatedBytes=heap.uordblks; sample.NativeHeapFreeBytes=heap.fordblks;
            sample.NativeHeapArenaBytes=heap.arena; sample.NativeHeapMappedBytes=heap.hblkhd;
        }
        return sample;
    }
}
