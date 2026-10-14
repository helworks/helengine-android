#include "AndroidGameHost.hpp"
#include "AndroidDisplayDensity.hpp"
#include "AndroidLaunchOptions.hpp"
#include "CoreInitializationOptions.hpp"
#include "HostFileSystemContentStreamSource.hpp"
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
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace helengine::android {
    AndroidGameHost::AndroidGameHost(ANativeActivity* activity, AndroidVulkanRenderer& renderer)
        : Activity(activity), Renderer(renderer), Overlay(renderer), Meshes(renderer,Overlay) {}
    AndroidGameHost::~AndroidGameHost() { if (Engine) Engine->Dispose(); }
    std::vector<std::uint8_t> AndroidGameHost::ReadPackagedFile(const std::string& path) {
        std::unique_ptr<AAsset,decltype(&AAsset_close)> asset(AAssetManager_open(Activity->assetManager,path.c_str(),AASSET_MODE_STREAMING),AAsset_close);
        if (!asset) throw std::runtime_error("Missing cooked APK asset: "+path);
        std::vector<std::uint8_t> data(AAsset_getLength64(asset.get()));
        std::size_t offset=0;
        while (offset<data.size()) {
            int count=AAsset_read(asset.get(),data.data()+offset,data.size()-offset);
            if (count<=0) throw std::runtime_error("Incomplete cooked APK asset: "+path);
            offset+=count;
        }
        return data;
    }
    void AndroidGameHost::ExtractContent() {
        auto inventory=ReadPackagedFile("android-content-files.txt");
        std::istringstream files(std::string(inventory.begin(),inventory.end())); std::string relative;
        const std::filesystem::path root=std::filesystem::path(Activity->internalDataPath)/"content";
        std::filesystem::create_directories(root);
        std::size_t count=0;
        while (std::getline(files,relative)) {
            if (!relative.empty() && relative.back()=='\r') relative.pop_back();
            if (relative.empty()) continue;
            if (relative.rfind("cooked/",0)!=0 || relative.find("..")!=std::string::npos || relative.find('\\')!=std::string::npos) throw std::runtime_error("Invalid packaged content inventory path.");
            auto bytes=ReadPackagedFile(relative); auto destination=root/relative;
            std::filesystem::create_directories(destination.parent_path());
            std::ofstream output(destination,std::ios::binary|std::ios::trunc);
            output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
            if (!output) throw std::runtime_error("Could not extract cooked asset into private app storage.");
            ++count;
        }
        if (!count || chdir(root.c_str())!=0) throw std::runtime_error("Could not select the Android packaged-content directory.");
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","APK authored content extracted: files=%zu.",count);
    }
    void AndroidGameHost::Initialize() {
        if (Engine) { Resize(); Resume(); return; }
        ExtractContent(); Engine=std::make_unique<::Core>();
        auto* options=Engine->get_InitializationOptions();
        options->set_ContentStreamSource(new ::HostFileSystemContentStreamSource("./"));
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
        Resume();
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
    void AndroidGameHost::Resume() { PreviousFrame=std::chrono::steady_clock::now(); Input.Reset(); Audio.SetHostPaused(false); }
    void AndroidGameHost::Suspend() { Input.Reset(); Audio.SetHostPaused(true); }
    void AndroidGameHost::Frame() {
        if (!Engine) throw std::runtime_error("Cannot update Android before engine initialization.");
        const auto now=std::chrono::steady_clock::now();
        const double elapsed=std::chrono::duration<double>(now-PreviousFrame).count(); PreviousFrame=now;
        Renderer.BeginSceneFrame();
        Engine->Update(elapsed); Engine->Draw(); Overlay.Draw(); Renderer.DrawFrame(); Engine->CompleteFrameBoundary();
        std::string activeScenes;
        auto* scenes=Engine->get_SceneManager()->get_LoadedScenes();
        for (int32_t index=0;index<scenes->get_Count();++index) {
            if (index) activeScenes+=",";
            activeScenes+=scenes->get_Item(index)->get_SceneId();
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
                        viewport->get_DensityScale(),viewport->get_PixelsPerCentimeter(),viewport->get_UiScale(),bounds.X,bounds.Y);
                    break;
                }
            }
        }
    }
}
