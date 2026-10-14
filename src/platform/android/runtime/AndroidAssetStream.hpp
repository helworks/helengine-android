#pragma once
#include "system/io/stream.hpp"
#include <android/asset_manager.h>
#include <array>
#include <memory>
#include <string>

namespace helengine::android {
    /// Reads one APK asset through a buffered, seekable engine stream without extracting it to disk.
    class AndroidAssetStream final : public ::Stream {
    public:
        /// Opens an independent asset handle; the activity retains the asset manager for this stream's lifetime.
        AndroidAssetStream(AAssetManager* manager,const std::string& path);
        /// Releases the asset handle even when a deserializer exits with an exception.
        ~AndroidAssetStream() override=default;
        /// Preserves the base stream overloads for array and span callers.
        using ::Stream::Read;
        /// Preserves the base stream overloads while rejecting writes through the native overload.
        using ::Stream::Write;
        /// Reads available bytes into the caller's buffer; zero denotes the real end of the asset.
        size_t Read(uint8_t* buffer,size_t offset,size_t count) override;
        /// Rejects modifications to packaged content.
        void Write(const uint8_t* buffer,size_t offset,size_t count) override;
        /// Seeks within the asset, using cached bytes when the target is still buffered.
        size_t Seek(int64_t offset,::SeekOrigin origin) override;
        /// Rejects resizing a packaged asset.
        void SetLength(size_t length) override;
        /// Reports whether the read handle is still open.
        bool CanRead() const override { return Asset!=nullptr; }
        /// Packaged assets cannot be written.
        bool CanWrite() const override { return false; }
        /// Both compressed and uncompressed assets support seeking while open.
        bool CanSeek() const override { return Asset!=nullptr; }
        /// Returns the asset's immutable uncompressed byte length.
        size_t Length() const override;
        /// Returns the consumer position, which can precede the SDK handle because of read buffering.
        size_t Position() const override;
        /// Moves the consumer position using the same bounds as Seek.
        void SetPosition(size_t value) override;
        /// Rejects capacity reservation for a read-only stream.
        void InternalReserve(size_t count) override;
        /// Rejects byte writes through binary serializers.
        void InternalWriteByte(uint8_t byte) override;
        /// Reads one buffered byte or returns -1 at the real end of the asset.
        int InternalReadByte() override;
        /// Releases the handle; repeated disposal is harmless.
        void Dispose() override { Close(); }
        /// Releases the handle and its buffered bytes immediately.
        void Close() override;
        /// Validates the open read handle; no writable data needs flushing.
        void Flush() override { RequireOpen(); }
    private:
        /// Rejects access to metadata and content after disposal.
        void RequireOpen() const;
        /// Gives all mutating stream operations the same explicit failure.
        [[noreturn]] static void RejectWrite();
        /// Reads a bounded SDK chunk, reporting native errors and unexpectedly truncated assets.
        size_t ReadNative(void* destination,size_t count);
        /// Owning handle is never shared between independent readers or scene-loading threads.
        std::unique_ptr<AAsset,decltype(&AAsset_close)> Asset{nullptr,AAsset_close};
        /// Immutable length is read with the SDK's 64-bit API.
        size_t AssetLength=0;
        /// Current consumer byte offset in the uncompressed asset.
        size_t Cursor=0;
        /// Small reads from binary serializers reuse one fixed buffer instead of making a native call per byte.
        std::array<uint8_t,16384> Buffer{};
        /// Asset offset represented by the first buffered byte.
        size_t BufferStart=0;
        /// Number of valid bytes in the current buffer.
        size_t BufferLength=0;
    };
}
