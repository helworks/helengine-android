#include "AndroidAssetContentStreamSource.hpp"
#include "AndroidAssetStream.hpp"
#include <stdexcept>

namespace helengine::android {
    AndroidAssetContentStreamSource::AndroidAssetContentStreamSource(AAssetManager* manager) : Manager(manager) {
        if (!Manager) throw std::invalid_argument("Android packaged content requires a live asset manager.");
    }
    std::string AndroidAssetContentStreamSource::NormalizePath(const std::string& path) {
        if (path.empty() || path.front()=='/' || path.find_first_of("\\:")!=std::string::npos || path.find('\0')!=std::string::npos) {
            throw std::invalid_argument("Android content paths must be relative APK paths.");
        }
        std::string normalized;
        for (size_t begin=0;begin<path.size();) {
            const auto end=path.find('/',begin);
            const auto part=path.substr(begin,end==std::string::npos?path.size()-begin:end-begin);
            if (part=="..") throw std::invalid_argument("Android content paths cannot traverse parent directories.");
            if (!part.empty() && part!=".") { if (!normalized.empty()) normalized+='/'; normalized+=part; }
            if (end==std::string::npos) break;
            begin=end+1;
        }
        if (normalized.rfind("cooked/",0)!=0 || path.back()=='/' || normalized=="cooked/") {
            throw std::invalid_argument("Android runtime content must name a file inside cooked/.");
        }
        return normalized;
    }
    ::Stream* AndroidAssetContentStreamSource::OpenRead(HeCppString assetPath) { return new AndroidAssetStream(Manager,NormalizePath(assetPath.raw())); }
}
