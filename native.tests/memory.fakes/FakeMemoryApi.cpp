#include "FakeMemoryApi.hpp"
#include "fcntl.h"
#include "unistd.h"
#include <algorithm>
#include <cerrno>
#include <cstring>

long FakeMemoryApi::PageSize=4096;
std::string FakeMemoryApi::Statm;
size_t FakeMemoryApi::Cursor=0,FakeMemoryApi::ReadLimit=256;
int FakeMemoryApi::Handles=0,FakeMemoryApi::Closes=0,FakeMemoryApi::HeapCalls=0;
bool FakeMemoryApi::OpenFails=false,FakeMemoryApi::ReadFails=false,FakeMemoryApi::UsageFails=false,FakeMemoryApi::CloseFails=false;
bool FakeMemoryApi::InterruptOpen=false,FakeMemoryApi::InterruptRead=false,FakeMemoryApi::InterruptClose=false;
struct rusage FakeMemoryApi::Usage{};
struct mallinfo FakeMemoryApi::Heap{};
void FakeMemoryApi::Reset() {
    PageSize=4096; Statm="1000 200 30 40 0 500 0\n"; Cursor=0; ReadLimit=256; Handles=Closes=HeapCalls=0;
    OpenFails=ReadFails=UsageFails=CloseFails=InterruptOpen=InterruptRead=InterruptClose=false;
    Usage={1000,12,3}; Heap={80000,12000,50000,30000}; errno=0;
}
long sysconf(int name) { return name==_SC_PAGESIZE?FakeMemoryApi::PageSize:-1; }
int open(const char* path,int flags) {
    if (FakeMemoryApi::InterruptOpen) { FakeMemoryApi::InterruptOpen=false; errno=EINTR; return -1; }
    if (FakeMemoryApi::OpenFails || std::strcmp(path,"/proc/self/statm")!=0 || flags!=(O_RDONLY|O_CLOEXEC)) { errno=EACCES; return -1; }
    ++FakeMemoryApi::Handles; FakeMemoryApi::Cursor=0; return 7;
}
int64_t read(int descriptor,void* destination,size_t count) {
    if (FakeMemoryApi::InterruptRead) { FakeMemoryApi::InterruptRead=false; errno=EINTR; return -1; }
    if (FakeMemoryApi::ReadFails || descriptor!=7) { errno=EIO; return -1; }
    const auto available=FakeMemoryApi::Statm.size()-FakeMemoryApi::Cursor;
    const auto copied=std::min({count,available,FakeMemoryApi::ReadLimit});
    if (copied>0) std::memcpy(destination,FakeMemoryApi::Statm.data()+FakeMemoryApi::Cursor,copied);
    FakeMemoryApi::Cursor+=copied; return static_cast<int64_t>(copied);
}
int close(int descriptor) {
    ++FakeMemoryApi::Closes;
    if (descriptor!=7 || FakeMemoryApi::Handles!=1) { errno=EBADF; return -1; }
    --FakeMemoryApi::Handles;
    if (FakeMemoryApi::InterruptClose) { FakeMemoryApi::InterruptClose=false; errno=EINTR; return -1; }
    if (FakeMemoryApi::CloseFails) { errno=EIO; return -1; }
    return 0;
}
int getrusage(int who,struct rusage* usage) {
    if (FakeMemoryApi::UsageFails || who!=RUSAGE_SELF) { errno=EIO; return -1; }
    *usage=FakeMemoryApi::Usage; return 0;
}
struct mallinfo mallinfo() { ++FakeMemoryApi::HeapCalls; return FakeMemoryApi::Heap; }
