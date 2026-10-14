#pragma once
#include "IPlatformSettingsBackend.hpp"
#include "PlatformSettingsConfiguration.hpp"
#include "../rendering/AndroidVulkanRenderer.hpp"

namespace helengine::android {
    /// Applies Android's optional runtime contracts through the existing engine settings service.
    class AndroidPlatformSettingsBackend final : public ::IPlatformSettingsBackend {
    public:
        /// Binds settings to the persistent Vulkan renderer owned by the host.
        explicit AndroidPlatformSettingsBackend(AndroidVulkanRenderer& renderer);
        /// Borrows the exported catalog until the core and backend finish using it.
        void SetConfiguration(::PlatformSettingsConfiguration* configuration);
        /// Rejects unknown Android contracts, invalid render scales and invalid FPS requests before changing runtime state.
        void Validate(::PlatformSettingsState* candidate) override;
        /// Recreates quality-dependent resources synchronously, retaining prior resources on failure.
        void Apply(::PlatformSettingsState* candidate) override;
    private:
        /// Renderer outlives settings application and native surface replacement.
        AndroidVulkanRenderer& Renderer;
        /// Host-owned original catalog describes every platform option the backend must support.
        ::PlatformSettingsConfiguration* Configuration=nullptr;
        /// Stable identifier shared with the builder's AndroidRuntimeSettingIds contract.
        static constexpr const char* RenderScaleId="render-scale";
        /// Explicit FPS contract shared with the Android builder; zero follows the display rate.
        static constexpr const char* TargetFrameRateId="target-frame-rate";
    };
}
