#include "AndroidPlatformSettingsBackend.hpp"
#include "CommonPlatformRuntimeSettings.hpp"
#include "PlatformRuntimeSettingDefinition.hpp"
#include "PlatformRuntimeSettingKind.hpp"
#include "PlatformSettingsState.hpp"
#include "PlatformSettingValue.hpp"
#include "../rendering/FrameRateMath.hpp"

namespace helengine::android {
    AndroidPlatformSettingsBackend::AndroidPlatformSettingsBackend(AndroidVulkanRenderer& renderer) : Renderer(renderer) {}
    void AndroidPlatformSettingsBackend::SetConfiguration(::PlatformSettingsConfiguration* configuration) { Configuration=configuration; }
    void AndroidPlatformSettingsBackend::Validate(::PlatformSettingsState* candidate) {
        if (!candidate || !Configuration) throw std::invalid_argument("Android settings require an exported catalog and candidate values.");
        for (int32_t index=0;index<Configuration->get_SettingCount();++index) {
            const auto id=Configuration->GetDefinitionAt(index)->get_Id();
            if (!::CommonPlatformRuntimeSettings::IsCommon(id) && id!=RenderScaleId && id!=TargetFrameRateId) {
                throw std::runtime_error(std::string("Unsupported Android runtime setting: ")+id.c_str());
            }
        }
        if (candidate->Contains(RenderScaleId)) {
            const auto scale=candidate->GetValue(RenderScaleId);
            if (scale.Kind!=::PlatformRuntimeSettingKind::Number) throw std::invalid_argument("Android render scale requires a numeric value.");
            Renderer.ValidateSceneRenderScale(scale.Number);
        }
        if (candidate->Contains(TargetFrameRateId)) {
            const auto rate=candidate->GetValue(TargetFrameRateId);
            if (rate.Kind!=::PlatformRuntimeSettingKind::Integer) throw std::invalid_argument("Android target frame rate requires an integer value.");
            FrameRateMath::Validate(rate.Number);
        }
    }
    void AndroidPlatformSettingsBackend::Apply(::PlatformSettingsState* candidate) {
        if (candidate->Contains(RenderScaleId)) Renderer.SetSceneRenderScale(candidate->GetValue(RenderScaleId).Number);
        // Fallible GPU allocation completes before the non-allocating FPS update publishes any new pacing request.
        if (candidate->Contains(TargetFrameRateId)) Renderer.SetTargetFrameRate(candidate->GetValue(TargetFrameRateId).Number);
    }
}
