#pragma once
#include "../rendering/AndroidVulkanRenderer.hpp"
#include "AndroidRenderManager3D.hpp"
#include "AndroidRenderManager2D.hpp"
#include "AndroidInputBackend.hpp"
#include "AndroidAudioBackend.hpp"
#include "Core.hpp"
#include <android/native_activity.h>
#include <chrono>

namespace helengine::android {
    /// Owns a generated HelEngine runtime across surface replacement and drives authored scene scripts each frame.
    class AndroidGameHost {
    public:
        /// Binds runtime ownership to the activity and its persistent Vulkan renderer.
        AndroidGameHost(ANativeActivity* activity, AndroidVulkanRenderer& renderer);
        /// Disposes the scene and engine-owned content before releasing platform adapters.
        ~AndroidGameHost();
        /// Extracts cooked assets into private app storage and boots the embedded startup scene once.
        void Initialize();
        /// Updates engine layout after a surface or content-rectangle resize.
        void Resize();
        /// Restarts the frame clock without counting time spent paused as a single long update.
        void Resume();
        /// Pauses platform audio and clears held input while the activity is in the background.
        void Suspend();
        /// Advances authored scripts, gathers render queues, presents Vulkan, and commits frame-boundary operations.
        void Frame();
        /// Returns the portable input adapter used by the native event loop.
        AndroidInputBackend& GetInput() { return Input; }
    private:
        /// Reads a complete packaged asset without depending on APK compression or file descriptors.
        std::vector<std::uint8_t> ReadPackagedFile(const std::string& path);
        /// Copies the explicit cooked-file inventory into the application's private content directory.
        void ExtractContent();
        /// Reports Android's density to the shared viewport system without converting any UI coordinates.
        void ReportDisplayDensity();
        /// Activity retained for access to assets and private storage while the host lives.
        ANativeActivity* Activity;
        /// Vulkan host retained across Android window lifetimes.
        AndroidVulkanRenderer& Renderer;
        /// Ordered overlay backend for the generated core.
        AndroidRenderManager2D Overlay;
        /// Mesh and camera backend for the generated core.
        AndroidRenderManager3D Meshes;
        /// Native pointer input backend owned by the host.
        AndroidInputBackend Input;
        /// Native PCM output and worker lifetime retained across scene and surface replacement.
        AndroidAudioBackend Audio;
        /// Engine created only after a drawable native window exists.
        std::unique_ptr<::Core> Engine;
        /// Monotonic timestamp of the preceding active frame.
        std::chrono::steady_clock::time_point PreviousFrame;
        /// Successfully processed engine frames used to limit diagnostic output.
        std::uint64_t Frames = 0;
        /// Last published scene-id sequence, used to log real scene transitions once.
        std::string ReportedScenes;
    };
}
