#include "AndroidAssetStream.hpp"
#include "../diagnostics/AndroidTraceSection.hpp"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace helengine::android {
    AndroidAssetStream::AndroidAssetStream(AAssetManager* manager,const std::string& path) {
        const AndroidTraceSection trace("HelEngine.Content.Open");
        if (!manager || path.empty() || path.find('\0')!=std::string::npos) throw std::invalid_argument("Android asset stream requires a manager and valid asset path.");
        Asset.reset(AAssetManager_open(manager,path.c_str(),AASSET_MODE_RANDOM));
        if (!Asset) throw std::runtime_error("Missing packaged Android asset: "+path);
        const auto length=AAsset_getLength64(Asset.get());
        if (length<0 || static_cast<uint64_t>(length)>std::numeric_limits<size_t>::max()) throw std::runtime_error("Android asset length exceeds the stream address range.");
        AssetLength=static_cast<size_t>(length);
    }
    void AndroidAssetStream::RequireOpen() const {
        if (!Asset) throw std::logic_error("Android asset stream is closed.");
    }
    void AndroidAssetStream::RejectWrite() { throw std::logic_error("Packaged Android content is read-only."); }
    size_t AndroidAssetStream::Length() const { RequireOpen(); return AssetLength; }
    size_t AndroidAssetStream::Position() const { RequireOpen(); return Cursor; }
    size_t AndroidAssetStream::ReadNative(void* destination,size_t count) {
        const AndroidTraceSection trace("HelEngine.Content.ReadNative");
        const auto request=std::min(count,static_cast<size_t>(std::numeric_limits<int>::max()));
        const int result=AAsset_read(Asset.get(),destination,request);
        if (result<0 || static_cast<size_t>(result)>request) throw std::runtime_error("Android packaged asset read failed.");
        if (result==0 && Cursor<AssetLength) throw std::runtime_error("Android packaged asset ended before its declared length.");
        return static_cast<size_t>(result);
    }
    size_t AndroidAssetStream::Read(uint8_t* buffer,size_t offset,size_t count) {
        RequireOpen();
        if (offset>std::numeric_limits<size_t>::max()-count || (!buffer && count!=0)) throw std::invalid_argument("Android asset read requires a valid destination range.");
        const size_t available=std::min(count,AssetLength-Cursor);
        size_t copied=0;
        while (copied<available) {
            if (Cursor>=BufferStart && Cursor-BufferStart<BufferLength) {
                const auto bufferedOffset=Cursor-BufferStart;
                const auto chunk=std::min(available-copied,BufferLength-bufferedOffset);
                std::memcpy(buffer+offset+copied,Buffer.data()+bufferedOffset,chunk); Cursor+=chunk; copied+=chunk;
            } else if (available-copied>=Buffer.size()) {
                BufferLength=0;
                const auto chunk=ReadNative(buffer+offset+copied,available-copied); Cursor+=chunk; copied+=chunk;
            } else {
                BufferStart=Cursor; BufferLength=ReadNative(Buffer.data(),std::min(Buffer.size(),AssetLength-Cursor));
            }
        }
        return copied;
    }
    size_t AndroidAssetStream::Seek(int64_t offset,::SeekOrigin origin) {
        RequireOpen();
        int64_t base=0;
        if (origin==::SeekOrigin::Current) base=static_cast<int64_t>(Cursor);
        else if (origin==::SeekOrigin::End) base=static_cast<int64_t>(AssetLength);
        else if (origin!=::SeekOrigin::Begin) throw std::invalid_argument("Invalid Android asset seek origin.");
        if (offset<-base || offset>static_cast<int64_t>(AssetLength)-base) throw std::out_of_range("Android asset seek is outside the packaged file.");
        const auto target=static_cast<size_t>(base+offset);
        if (BufferLength!=0 && target>=BufferStart && target-BufferStart<=BufferLength) { Cursor=target; return Cursor; }
        const AndroidTraceSection trace("HelEngine.Content.SeekNative");
        const auto result=AAsset_seek64(Asset.get(),static_cast<int64_t>(target),SEEK_SET);
        if (result<0 || static_cast<uint64_t>(result)!=target) throw std::runtime_error("Android packaged asset seek failed.");
        Cursor=target; BufferStart=target; BufferLength=0; return Cursor;
    }
    void AndroidAssetStream::SetPosition(size_t value) {
        if (value>static_cast<size_t>(std::numeric_limits<int64_t>::max())) throw std::out_of_range("Android asset position exceeds the seek range.");
        Seek(static_cast<int64_t>(value),::SeekOrigin::Begin);
    }
    int AndroidAssetStream::InternalReadByte() { uint8_t value=0; return Read(&value,0,1)==0?-1:value; }
    void AndroidAssetStream::Write(const uint8_t*,size_t,size_t) { RejectWrite(); }
    void AndroidAssetStream::SetLength(size_t) { RejectWrite(); }
    void AndroidAssetStream::InternalReserve(size_t) { RejectWrite(); }
    void AndroidAssetStream::InternalWriteByte(uint8_t) { RejectWrite(); }
    void AndroidAssetStream::Close() { Asset.reset(); BufferLength=0; }
}
