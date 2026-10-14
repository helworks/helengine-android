#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
/// Generated engine seek ABI mirrored for isolated CPU tests.
enum SeekOrigin : uint8_t { Begin,Current,End };
/// Native Stream interface mirrored exactly for the operations implemented by the production asset adapter.
class Stream {
public:
    /// Allows caller ownership through the existing content-source interface.
    virtual ~Stream()=default;
    /// Reads a destination byte range.
    virtual size_t Read(uint8_t* buffer,size_t offset,size_t count)=0;
    /// Writes a source byte range.
    virtual void Write(const uint8_t* buffer,size_t offset,size_t count)=0;
    /// Seeks from one supported origin.
    virtual size_t Seek(int64_t offset,SeekOrigin origin)=0;
    /// Resizes a writable stream.
    virtual void SetLength(size_t length)=0;
    /// Reports current read capability.
    virtual bool CanRead() const=0;
    /// Reports current write capability.
    virtual bool CanWrite() const=0;
    /// Reports current seek capability.
    virtual bool CanSeek() const=0;
    /// Returns stream byte length.
    virtual size_t Length() const=0;
    /// Returns logical stream position.
    virtual size_t Position() const=0;
    /// Sets logical stream position.
    virtual void SetPosition(size_t value)=0;
    /// Reserves writable capacity.
    virtual void InternalReserve(size_t count)=0;
    /// Writes one byte for binary serializers.
    virtual void InternalWriteByte(uint8_t value)=0;
    /// Reads one byte or returns -1 at EOF.
    virtual int InternalReadByte()=0;
    /// Explicitly releases resources.
    virtual void Dispose() {}
    /// Closes an underlying handle.
    virtual void Close() {}
    /// Flushes pending writes.
    virtual void Flush() {}
};
