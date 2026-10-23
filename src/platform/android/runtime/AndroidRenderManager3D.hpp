#pragma once
#include "RenderManager3D.hpp"
#include "IRenderVisitor3D.hpp"
#include "float4x4.hpp"
#include "AndroidRuntimeMaterial.hpp"
#include "../rendering/AndroidVulkanRenderer.hpp"

namespace helengine::android {
    class AndroidRenderManager2D;
    /// Traverses generated camera queues and converts authored meshes into depth-tested Vulkan batches.
    class AndroidRenderManager3D final : public ::RenderManager3D, public ::IRenderVisitor3D {
    public:
        /// Binds the engine manager to its Vulkan host and ordered overlay renderer.
        AndroidRenderManager3D(AndroidVulkanRenderer& renderer, AndroidRenderManager2D& overlay);
        /// Copies positions, normals, index ranges, and bounds from an authored model.
        ::RuntimeModel* BuildModelFromRaw(::ModelAsset* data) override;
        /// Loads and validates an engine-owned cooked model payload.
        ::RuntimeModel* BuildModelFromCooked(HeCppString path, ::IContentStreamSource* source) override;
        /// Loads the builder-owned standard material payload.
        ::RuntimeMaterial* BuildMaterialFromCooked(HeCppString path, ::IContentStreamSource* source) override;
        /// Converts one standard material's color and lighting settings into a runtime resource.
        ::RuntimeMaterial* BuildMaterialFromCooked(::PlatformMaterialAsset* material) override;
        /// Collects 3D geometry followed by each camera's ordered overlay queue.
        void Draw() override;
        /// Converts one visible mesh into clip-space triangles using the active camera and scene lights.
        void Visit(::IDrawable3D* drawable) override;
        /// Reports the number of mesh submesh batches emitted for the latest frame.
        int32_t get_LastDrawCallCount() override { return DrawCalls; }
    private:
        /// Captures the active camera's authored ambient, directional, point and spot lighting once before its queue traversal.
        void CaptureCameraLighting();
        /// Vulkan presentation and batch queue shared with the 2D manager.
        AndroidVulkanRenderer& Renderer;
        /// Overlay visitor invoked after each camera's 3D queue.
        AndroidRenderManager2D& Overlay;
        /// Built-in white standard surface used by the engine's valid materialless imported meshes.
        AndroidRuntimeMaterial UnboundMaterial;
        /// Camera currently visiting the render queue.
        ::ICamera* Camera = nullptr;
        /// Inverse camera world transform, expressed in the engine's matrix layout.
        ::float4x4 View;
        /// Camera/light portion copied into each material batch's std140 block.
        SceneLighting CameraLighting{};
        /// Pixel viewport associated with the active camera.
        VkRect2D Viewport{};
        /// Number of emitted 3D batches in the current frame.
        int32_t DrawCalls = 0;
        /// Frame ordinal used to sample authored transform diagnostics without flooding logcat.
        std::uint64_t FrameSequence = 0;
        /// Most recently reported camera dimensions, used to record aspect changes once per resize.
        VkExtent2D ReportedViewport{};
    };
}
