#include "AndroidGameHost.hpp"
#include "../diagnostics/AndroidTraceSection.hpp"
#include "AndroidDisplayDensity.hpp"
#include "AndroidLaunchOptions.hpp"
#include "CoreInitializationOptions.hpp"
#include "GeneratedRuntimeModuleRegistration.hpp"
#include "RuntimeSceneCatalog.hpp"
#include "RuntimeSceneCatalogEntry.hpp"
#include "SceneManager.hpp"
#include "LoadedSceneRecord.hpp"
#include "PlatformInfo.hpp"
#include "ObjectManager.hpp"
#include "ViewportComponent.hpp"
#include "ICamera.hpp"
#include "StandardPlatformInputConfiguration.hpp"
#include "StandardPlatformActionBinding.hpp"
#include "InputControlId.hpp"
#include "runtime/runtime_standard_platform_input_manifest.hpp"
#include "runtime/runtime_scene_catalog_manifest.hpp"
#include "runtime/runtime_startup_manifest.hpp"
#include <android/log.h>

namespace helengine::android {
    AndroidGameHost::AndroidGameHost(ANativeActivity* activity, AndroidVulkanRenderer& renderer)
        : Activity(activity), Renderer(renderer), Overlay(renderer), Meshes(renderer,Overlay), Settings(renderer), Content(activity->assetManager), Input(activity), AudioFocus(activity,Audio), MemoryTrim(activity,ALooper_forThread()) {}
    AndroidGameHost::~AndroidGameHost() { if (Engine) Engine->Dispose(); if (RuntimeSettings) RuntimeSettings->Dispose(); }
    void AndroidGameHost::Initialize() {
        const AndroidTraceSection trace("HelEngine.Initialize");
        if (Engine) { Resize(); return; }
        Engine=std::make_unique<::Core>();
        Meshes.set_ShadowsEnabled(he_get_runtime_shadows_enabled());
        auto* options=Engine->get_InitializationOptions();
        RuntimeSettings.reset(he_create_runtime_platform_settings());
        options->set_PlatformSettingsConfiguration(RuntimeSettings.get());
        Settings.SetConfiguration(RuntimeSettings.get());
        options->set_PlatformSettingsBackend(&Settings);
        options->set_ContentStreamSource(&Content);
        options->set_RuntimeDiagnosticsProvider(&Diagnostics);
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","APK authored content source ready: direct asset streams.");
        options->set_UpdateOrderLayers(8); options->set_RenderOrderLayers3D(8);
        options->set_UpdateListInitialCapacity(128); options->set_RenderList2DInitialCapacity(128); options->set_RenderList3DInitialCapacity(64);
        options->set_CommitPendingSceneOperationsDuringDraw(false);
        options->set_SceneLoadingBudgetMilliseconds(he_get_runtime_scene_loading_budget_milliseconds());
        std::size_t bindingCount=0;
        const auto* inputBindings=he_runtime_standard_platform_action_entries(&bindingCount);
        if (bindingCount && !inputBindings) throw std::runtime_error("Android input manifest declared bindings without their data.");
        auto* bindings=new List<::StandardPlatformActionBinding*>(bindingCount);
        for (std::size_t index=0;index<bindingCount;++index) {
            const auto& binding=inputBindings[index];
            bindings->AddOwned(new ::StandardPlatformActionBinding(static_cast<::StandardPlatformAction>(binding.ActionId),
                ::InputControlId(static_cast<::InputDeviceKind>(binding.DeviceKind),static_cast<::InputControlKind>(binding.ControlKind),binding.DeviceIndex,binding.ControlIndex)));
        }
        options->set_StandardPlatformInputConfiguration(new ::StandardPlatformInputConfiguration(bindings));
        std::size_t count=0; auto* entries=he_runtime_scene_catalog_entries(&count);
        if (!entries || !count) throw std::runtime_error("Android startup requires a generated scene catalog.");
        auto* catalog=new Array<::RuntimeSceneCatalogEntry*>(count);
        std::string startup;
        for (std::size_t i=0; i<count; ++i) {
            (*catalog)[i]=new ::RuntimeSceneCatalogEntry(entries[i].SceneId,entries[i].CookedRelativePath);
            if (std::string(entries[i].CookedRelativePath)==he_get_runtime_startup_scene_relative_path()) startup=entries[i].SceneId;
        }
        if (startup.empty()) throw std::runtime_error("Android startup scene is missing from its embedded scene catalog.");
        const auto requestedStartup=ReadAndroidStartupSceneOverride(Activity);
        if (!requestedStartup.empty()) {
            bool found=false;
            for (std::size_t index=0;index<count;++index) if (requestedStartup==entries[index].SceneId) found=true;
            if (!found) throw std::runtime_error("Requested Android startup scene is absent from the generated catalog: "+requestedStartup);
            startup=requestedStartup;
        }
        options->set_SceneCatalog(new ::RuntimeSceneCatalog(catalog));
        auto size=Renderer.GetExtent(); Meshes.AddWindow(0,size.width,size.height);
        ReportDisplayDensity();
        Engine->Initialize(&Meshes,&Overlay,&Input,new ::PlatformInfo("Android","Vulkan"),options);
        Audio.Initialize(); Engine->SetAudioBackend(&Audio);
        RegisterGeneratedRuntimeModules(Engine.get());
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Generated HelEngine runtime initialized; startup=%s path=%s.",startup.c_str(),he_runtime_scene_cooked_relative_path(startup.c_str()));
        const std::string loading=he_get_runtime_loading_screen_scene_id();
        if (loading.empty() || loading==startup) {
            Engine->get_SceneManager()->LoadScene(startup,::SceneLoadMode::Single);
            Engine->CompleteFrameBoundary();
        } else {
            bool found=false;
            for (std::size_t index=0;index<count;++index) if (loading==entries[index].SceneId) found=true;
            if (!found) throw std::runtime_error("Android configured loading screen is absent from the packaged scene catalog.");
            Engine->get_SceneManager()->LoadScene(loading,::SceneLoadMode::Additive);
            Engine->CompleteFrameBoundary();
            Engine->get_SceneManager()->RequestSceneTransition(startup);
        }
    }
    void AndroidGameHost::Resize() {
        if (Engine) {
            auto size=Renderer.GetExtent();
            Meshes.OnWindowResize(0,size.width,size.height);
            ReportDisplayDensity();
        }
    }
    void AndroidGameHost::ReportDisplayDensity() {
        const int dpi=ReadAndroidDisplayDensity(Activity);
        Meshes.SetWindowDensityDpi(0,dpi);
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Android display density reported to engine viewport system: %d dpi.",dpi);
    }
    void AndroidGameHost::Resume() {
        PreviousFrame=std::chrono::steady_clock::now(); Input.Reset();
        AudioFocus.SetActive(true); Audio.SetHostPaused(false);
    }
    void AndroidGameHost::Suspend() {
        Input.Reset(); Audio.SetHostPaused(true); AudioFocus.SetActive(false);
    }
    void AndroidGameHost::TrimPendingMemory() {
        const int level=MemoryTrim.TakePendingLevel();
        if (!level) return;
        const auto result=Renderer.TrimUnusedResources();
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Renderer memory trimmed: level=%d buffers=%llu expiredTextures=%llu vertexCapacity=%llu lightingCapacity=%llu.",level,
            static_cast<unsigned long long>(result.FrameBufferCount),static_cast<unsigned long long>(result.ExpiredTextureCount),
            static_cast<unsigned long long>(result.VertexCapacityBytes),static_cast<unsigned long long>(result.LightingCapacityBytes));
    }
    void AndroidGameHost::Frame() {
        const AndroidTraceSection trace("HelEngine.Frame");
        if (!Engine) throw std::runtime_error("Cannot update Android before engine initialization.");
        const auto now=std::chrono::steady_clock::now();
        const double elapsed=std::chrono::duration<double>(now-PreviousFrame).count(); PreviousFrame=now;
        Renderer.BeginSceneFrame();
        { const AndroidTraceSection phase("HelEngine.Core.Update"); Engine->Update(elapsed); }
        { const AndroidTraceSection phase("HelEngine.Core.Draw"); Engine->Draw(); }
        { const AndroidTraceSection phase("HelEngine.Overlay.Build"); Overlay.Draw(); }
        Renderer.DrawFrame();
        { const AndroidTraceSection phase("HelEngine.Scene.Commit"); Engine->CompleteFrameBoundary(); }
        std::string activeScenes;
        auto* scenes=Engine->get_SceneManager()->get_LoadedScenes();
        for (int32_t index=0;index<scenes->get_Count();++index) {
            if (index) activeScenes+=",";
            activeScenes+=scenes->get_Item(index)->get_SceneId().raw();
        }
        if (activeScenes!=ReportedScenes) {
            ReportedScenes=activeScenes;
            __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Authored active scenes: %s.",activeScenes.c_str());
        }
        if ((Frames++%120)==0) {
            auto* updateables=Engine->get_ObjectManager()->get_Updateables();
            __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Authored runtime update: elapsed=%.3f cameras=%d updateables=%d meshDraws=%d.",
                Engine->get_TotalElapsedSeconds(),Engine->get_ObjectManager()->get_Cameras()->get_Count(),
                updateables->get_Count(),Meshes.get_LastDrawCallCount());
            for (int32_t index=0; index<updateables->get_Count(); ++index) {
                auto* viewport=dynamic_cast<::ViewportComponent*>((*updateables)[index]);
                if (viewport && viewport->get_ScalingMode()==::ViewportComponent::DensityScalingMode) {
                    auto bounds=viewport->get_ResolvedViewportSize();
                    __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Shared viewport density: scale=%.3f authoredPixelsPerCm=%.3f uiScale=%.3f physical=%dx%d.",
                        static_cast<double>(viewport->get_DensityScale()),static_cast<double>(viewport->get_PixelsPerCentimeter()),static_cast<double>(viewport->get_UiScale()),bounds.X,bounds.Y);
                    break;
                }
            }
        }
    }
}
