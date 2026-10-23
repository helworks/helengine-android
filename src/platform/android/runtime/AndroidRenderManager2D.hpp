#pragma once
#include "RenderManager2D.hpp"
#include "IRenderVisitor2D.hpp"
#include "float4.hpp"
#include "byte4.hpp"
#include "ClipRegionStackBuilder2D.hpp"
#include "IClipRegion2D.hpp"
#include "../rendering/AndroidVulkanRenderer.hpp"

class Entity;
class ICamera;

namespace helengine::android {
    /// Draws the engine's sprites, rounded panels, and authored font glyphs through Vulkan overlay batches.
    class AndroidRenderManager2D final : public ::RenderManager2D, public ::IRenderVisitor2D {
    public:
        /// Binds overlay drawing to the same Vulkan frame as 3D meshes.
        explicit AndroidRenderManager2D(AndroidVulkanRenderer& renderer);
        /// Decodes an engine texture into independently owned RGBA8 storage.
        ::RuntimeTexture* BuildTextureFromRaw(::TextureAsset* data) override;
        /// Deserializes a packaged texture through the configured content source.
        ::RuntimeTexture* BuildTextureFromCooked(HeCppString path, ::IContentStreamSource* source) override;
        /// Visits one camera's ordered 2D queue within its pixel viewport.
        void DrawCamera(::ICamera* camera, VkRect2D viewport);
        /// Dispatches each generated drawable to its engine-owned drawing entry point.
        void Visit(::IDrawable2D* drawable) override;
        /// Emits a transformed sprite quad with its authored source rectangle and tint.
        void DrawSprite(::ISpriteDrawable2D* sprite) override;
        /// Lays out glyphs with the shared engine font metrics, alignment, wrapping, outline, and shadow settings.
        void DrawText(::ITextDrawable2D* text) override;
        /// Emits rounded border and fill polygons using the authored size, radius, and transform.
        void DrawRoundedRect(::IRoundedRectDrawable2D* shape) override;
    protected:
        /// Applies a validated dynamic atlas update and increments the GPU upload revision.
        void UpdateTextureRegionCore(::RuntimeTexture* texture, int32_t x, int32_t y, int32_t width, int32_t height,
            Array<uint8_t>* rgba8, int32_t sourceRowPitch) override;
    private:
        /// Appends a transformed textured quad to an existing batch.
        void AppendQuad(SceneBatch& batch, ::Entity* parent, ::RealEvaluation x, ::RealEvaluation y, ::RealEvaluation width, ::RealEvaluation height, ::float4 source, ::byte4 color);
        /// Converts a local UI coordinate to clip space using the parent transform and camera viewport.
        SceneVertex Vertex(::Entity* parent, ::RealEvaluation x, ::RealEvaluation y, ::RealEvaluation u, ::RealEvaluation v, ::byte4 color);
        /// Attaches the current drawable's authored clipping intersection before submitting its geometry.
        void Submit(SceneBatch batch);
        /// Emits one convex rounded rectangle polygon, preserving per-corner radius selection.
        void SubmitRoundedPolygon(::IRoundedRectDrawable2D* shape, ::RealEvaluation inset, ::byte4 color);
        /// Vulkan batch sink shared with the 3D render manager.
        AndroidVulkanRenderer& Renderer;
        /// Active camera's pixel viewport for clip-space conversion and scissoring.
        VkRect2D Viewport{};
        /// Physical clip intersection resolved for the drawable currently visited.
        VkRect2D Scissor{};
        /// Shared clip-chain resolver preserves the engine's ancestor ordering.
        ::ClipRegionStackBuilder2D ClipBuilder;
        /// Reusable borrowed ancestor clip references.
        ::List<::IClipRegion2D*> ClipChain;
        /// Limits the large authored panel's physical bounds diagnostic to the first rendered frame.
        bool ReportedPanelBounds=false;
    };
}
