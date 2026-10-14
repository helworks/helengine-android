#include "android/asset_manager.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

AAsset* AAssetManager_open(AAssetManager* manager,const char* path,int mode) {
    const auto file=manager->Files.find(path);
    if (file==manager->Files.end() || mode!=AASSET_MODE_RANDOM) return nullptr;
    auto* asset=new AAsset{manager,&file->second,0}; ++manager->Handles; return asset;
}
void AAsset_close(AAsset* asset) { --asset->Manager->Handles; delete asset; }
int64_t AAsset_getLength64(AAsset* asset) { return static_cast<int64_t>(asset->Bytes->size()); }
int AAsset_read(AAsset* asset,void* buffer,size_t count) {
    auto& manager=*asset->Manager; ++manager.Reads;
    if (manager.FailRead) return -1;
    const auto length=asset->Bytes->size()-std::min(manager.TruncatedBytes,asset->Bytes->size());
    const auto available=asset->Cursor>=length?0:length-asset->Cursor;
    const auto copied=std::min({count,available,manager.ReadLimit});
    if (copied>0) std::memcpy(buffer,asset->Bytes->data()+asset->Cursor,copied);
    asset->Cursor+=copied; return static_cast<int>(copied);
}
int64_t AAsset_seek64(AAsset* asset,int64_t offset,int whence) {
    ++asset->Manager->Seeks;
    if (asset->Manager->FailSeek || whence!=SEEK_SET || offset<0 || static_cast<uint64_t>(offset)>asset->Bytes->size()) return -1;
    asset->Cursor=static_cast<size_t>(offset); return offset;
}
