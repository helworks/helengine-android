#pragma once
#include <cstdint>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

/// Test asset manager stores bytes and independently injected read/seek failures.
struct AAssetManager {
    /// Cooked paths visible to the fake APK namespace.
    std::map<std::string,std::vector<uint8_t>> Files;
    /// Live handle count detects failed-open and deserializer leaks.
    int Handles=0;
    /// Native read and seek counts verify bounded buffering rather than per-byte SDK calls.
    int Reads=0,Seeks=0;
    /// Maximum successful native read size simulates short reads.
    size_t ReadLimit=static_cast<size_t>(-1);
    /// Missing trailing bytes simulate a corrupt asset whose declared length is unchanged.
    size_t TruncatedBytes=0;
    /// Negative native return values simulate SDK failures.
    bool FailRead=false,FailSeek=false;
};
/// Independent handle into one immutable fake APK file.
struct AAsset {
    /// Owner observes open/close and read/seek activity.
    AAssetManager* Manager;
    /// Immutable asset bytes shared without sharing reader cursors.
    const std::vector<uint8_t>* Bytes;
    /// Native position can precede or follow the adapter's consumer position.
    size_t Cursor=0;
};
constexpr int AASSET_MODE_RANDOM=1;
/// Opens one independent read handle or reports a missing packaged asset.
AAsset* AAssetManager_open(AAssetManager* manager,const char* path,int mode);
/// Releases one read handle and updates leak observations.
void AAsset_close(AAsset* asset);
/// Reports immutable uncompressed length through the 64-bit SDK shape.
int64_t AAsset_getLength64(AAsset* asset);
/// Returns bytes, short reads, real EOF or an injected native failure.
int AAsset_read(AAsset* asset,void* buffer,size_t count);
/// Performs a native absolute seek or an injected failure.
int64_t AAsset_seek64(AAsset* asset,int64_t offset,int whence);
