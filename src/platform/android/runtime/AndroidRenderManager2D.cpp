#include "AndroidRenderManager2D.hpp"
#include "AndroidRuntimeTexture.hpp"
#include "android_asset_reader.hpp"
#include "TextureAsset.hpp"
#include "TextureAssetPixelCodec.hpp"
#include "ICamera.hpp"
#include "IRenderQueue2D.hpp"
#include "IDrawable2D.hpp"
#include "ISpriteDrawable2D.hpp"
#include "ITextDrawable2D.hpp"
#include "IRoundedRectDrawable2D.hpp"
#include "Entity.hpp"
#include "Batch2DGeometry.hpp"
#include "SpriteQuad2D.hpp"
#include "FontAsset.hpp"
#include "FontInfo.hpp"
#include "TextLayoutUtils.hpp"
#include "TextLayoutAlignmentUtils.hpp"
#include "runtime/native_cast.hpp"
#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>

namespace helengine::android {
    AndroidRenderManager2D::AndroidRenderManager2D(AndroidVulkanRenderer& renderer) : Renderer(renderer) {}
    ::RuntimeTexture* AndroidRenderManager2D::BuildTextureFromRaw(::TextureAsset* data) {
        if (!data) throw std::invalid_argument("Android texture source is required.");
        std::unique_ptr<Array<uint8_t>> decoded(::TextureAssetPixelCodec::DecodeToRgba32(data));
        auto result = std::make_unique<AndroidRuntimeTexture>();
        result->set_Id(data->get_Id()); result->set_Width(data->Width); result->set_Height(data->Height);
        result->Image = std::make_shared<SceneTexture>();
        result->Image->Width=data->Width; result->Image->Height=data->Height;
        result->Image->Pixels.reserve(decoded->Length);
        for (int32_t i=0; i<decoded->Length; ++i) result->Image->Pixels.push_back((*decoded)[i]);
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Authored texture loaded: %ux%u id=%s.",data->Width,data->Height,data->get_Id().c_str());
        return result.release();
    }
    ::RuntimeTexture* AndroidRenderManager2D::BuildTextureFromCooked(HeCppString path, ::IContentStreamSource* source) {
        auto asset=ReadAsset(path,source); auto* texture=he_cpp_try_cast<::TextureAsset>(asset.get());
        if (!texture) throw std::runtime_error("Android cooked texture must deserialize as TextureAsset.");
        auto* result=BuildTextureFromRaw(texture); texture->Dispose(); return result;
    }
    void AndroidRenderManager2D::UpdateTextureRegionCore(::RuntimeTexture* texture, int32_t x, int32_t y, int32_t width, int32_t height, Array<uint8_t>* rgba8, int32_t sourceRowPitch) {
        auto* target=dynamic_cast<AndroidRuntimeTexture*>(texture);
        if (!target || !target->Image) throw std::runtime_error("Android atlas update requires a live Android runtime texture.");
        for (int32_t row=0; row<height; ++row) {
            for (int32_t byte=0; byte<width*4; ++byte) {
                target->Image->Pixels[(static_cast<std::size_t>(y+row)*target->Image->Width+x)*4+byte]=(*rgba8)[row*sourceRowPitch+byte];
            }
        }
        ++target->Image->Revision;
    }
    void AndroidRenderManager2D::DrawCamera(::ICamera* camera, VkRect2D viewport) {
        Viewport=viewport; Scissor=viewport;
        if (camera->get_RenderQueue2D()) camera->get_RenderQueue2D()->VisitOrdered(this);
    }
    void AndroidRenderManager2D::Visit(::IDrawable2D* drawable) {
        if (!drawable || !drawable->get_Parent() || !drawable->get_Parent()->get_IsHierarchyEnabled()) return;
        ClipBuilder.BuildClipChain(drawable,&ClipChain);
        double left=0,top=0,right=Viewport.extent.width,bottom=Viewport.extent.height;
        for (int32_t index=0;index<ClipChain.get_Count();++index) {
            const auto rectangle=ClipChain[index]->GetClipRect();
            left=std::max(left,static_cast<double>(rectangle.X)); top=std::max(top,static_cast<double>(rectangle.Y));
            right=std::min(right,static_cast<double>(rectangle.X+rectangle.Z)); bottom=std::min(bottom,static_cast<double>(rectangle.Y+rectangle.W));
        }
        if (right<=left || bottom<=top) return;
        const auto x=static_cast<int32_t>(std::floor(left)),y=static_cast<int32_t>(std::floor(top));
        Scissor={{Viewport.offset.x+x,Viewport.offset.y+y},{static_cast<uint32_t>(std::ceil(right)-x),static_cast<uint32_t>(std::ceil(bottom)-y)}};
        drawable->Draw();
    }
    void AndroidRenderManager2D::Submit(SceneBatch batch) { batch.HasScissor=true; batch.Scissor=Scissor; Renderer.Submit(std::move(batch)); }
    SceneVertex AndroidRenderManager2D::Vertex(::Entity* parent, ::RealEvaluation x, ::RealEvaluation y, ::RealEvaluation u, ::RealEvaluation v, ::byte4 color) {
        const auto matrix=parent->get_WorldTransformMatrix();
        auto screenX=x*matrix.M11+y*matrix.M21+matrix.M41;
        auto screenY=x*matrix.M12+y*matrix.M22+matrix.M42;
        // SceneVertex is a fixed float32 Vulkan vertex layout; keep arithmetic in the selected Real policy until packing.
        return {{static_cast<float>(2*screenX/Viewport.extent.width-1),static_cast<float>(2*screenY/Viewport.extent.height-1),0,1},
            {color.X/255.0f,color.Y/255.0f,color.Z/255.0f,color.W/255.0f},{static_cast<float>(u),static_cast<float>(v)}};
    }
    void AndroidRenderManager2D::AppendQuad(SceneBatch& batch, ::Entity* parent, ::RealEvaluation x, ::RealEvaluation y, ::RealEvaluation width, ::RealEvaluation height, ::float4 source, ::byte4 color) {
        SceneVertex vertices[]{Vertex(parent,x,y,source.X,source.Y,color),Vertex(parent,x+width,y,source.X+source.Z,source.Y,color),
            Vertex(parent,x,y+height,source.X,source.Y+source.W,color),Vertex(parent,x+width,y+height,source.X+source.Z,source.Y+source.W,color)};
        for (int i : {0,1,2,2,1,3}) batch.Vertices.push_back(vertices[i]);
    }
    void AndroidRenderManager2D::DrawSprite(::ISpriteDrawable2D* sprite) {
        if (!sprite || !sprite->get_Parent() || !sprite->get_Texture()) return;
        auto* texture=dynamic_cast<AndroidRuntimeTexture*>(sprite->get_Texture());
        if (!texture || !texture->Image) throw std::runtime_error("Android sprite requires a live Android texture.");
        const auto quad=::Batch2DGeometry::ResolveSpriteQuad(sprite);
        const auto source=sprite->get_SourceRect(); const auto color=sprite->get_Color();
        SceneBatch batch{}; batch.Viewport=Viewport; batch.Texture=texture->Image;
        const ::float2 points[]{quad.TopLeft,quad.TopRight,quad.BottomLeft,quad.BottomRight};
        const float u[]{static_cast<float>(source.X),static_cast<float>(source.X+source.Z),static_cast<float>(source.X),static_cast<float>(source.X+source.Z)};
        const float v[]{static_cast<float>(source.Y),static_cast<float>(source.Y),static_cast<float>(source.Y+source.W),static_cast<float>(source.Y+source.W)};
        SceneVertex vertices[4];
        for (int i=0;i<4;++i) {
            vertices[i]={{static_cast<float>(2*points[i].X/Viewport.extent.width-1),static_cast<float>(2*points[i].Y/Viewport.extent.height-1),0,1},
                {color.X/255.0f,color.Y/255.0f,color.Z/255.0f,color.W/255.0f},{u[i],v[i]}};
        }
        for (int i : {0,1,2,2,1,3}) batch.Vertices.push_back(vertices[i]);
        Submit(std::move(batch));
    }
    void AndroidRenderManager2D::DrawText(::ITextDrawable2D* text) {
        if (!text || !text->get_Parent() || !text->get_Font()) return;
        auto* font=text->get_Font(); auto* texture=dynamic_cast<AndroidRuntimeTexture*>(font->get_Texture());
        if (!texture || !texture->Image) throw std::runtime_error("Android text requires a loaded runtime font atlas.");
        const ::RealEvaluation scale=text->get_FontScale();
        if (scale<=0) throw std::runtime_error("Authored text font scale must be positive.");
        auto content=text->get_Text(); auto size=text->get_Size();
        if (text->get_WrapText()) content=::TextLayoutUtils::WrapText(content,font,std::max(1,static_cast<int>(size.X/scale)));
        std::istringstream lines(content.raw()); std::string line; ::RealEvaluation y=0;
        SceneBatch batch{}; batch.Viewport=Viewport; batch.Texture=texture->Image;
        while (std::getline(lines,line)) {
            line.erase(std::remove(line.begin(),line.end(),'\r'),line.end());
            auto measured=::TextLayoutAlignmentUtils::MeasureVisibleLineWidth(line,font,scale,font->get_AtlasWidth());
            auto x=::TextLayoutAlignmentUtils::ResolveHorizontalOffset(text->get_Alignment(),size.X,measured);
            for (char character : line) {
                if (character==' ') { x+=font->get_FontInfo()->get_SpaceWidth()*scale; continue; }
                ::FontChar glyph;
                if (!font->get_Characters()->TryGetValue(character,glyph)) continue;
                const auto width=glyph.SourceRect.Z*font->get_AtlasWidth()*scale;
                const auto height=glyph.SourceRect.W*font->get_AtlasHeight()*scale;
                const auto top=y+glyph.OffsetY*scale;
                const auto glyphX=x+glyph.BearingX*scale;
                auto shadow=text->get_ShadowOffset();
                if (shadow.X!=0 || shadow.Y!=0) AppendQuad(batch,text->get_Parent(),glyphX+shadow.X,top+shadow.Y,width,height,glyph.SourceRect,text->get_ShadowColor());
                auto outline=text->get_OutlineScale();
                if (outline>0) {
                    AppendQuad(batch,text->get_Parent(),glyphX-outline,top,width,height,glyph.SourceRect,text->get_OutlineColor());
                    AppendQuad(batch,text->get_Parent(),glyphX+outline,top,width,height,glyph.SourceRect,text->get_OutlineColor());
                    AppendQuad(batch,text->get_Parent(),glyphX,top-outline,width,height,glyph.SourceRect,text->get_OutlineColor());
                    AppendQuad(batch,text->get_Parent(),glyphX,top+outline,width,height,glyph.SourceRect,text->get_OutlineColor());
                }
                AppendQuad(batch,text->get_Parent(),glyphX,top,width,height,glyph.SourceRect,text->get_Color());
                x+=glyph.AdvanceWidth>0 ? glyph.AdvanceWidth*scale : width;
            }
            y+=font->get_LineHeight()*scale;
        }
        Submit(std::move(batch));
    }
    void AndroidRenderManager2D::SubmitRoundedPolygon(::IRoundedRectDrawable2D* shape, ::RealEvaluation inset, ::byte4 color) {
        auto size=shape->get_Size(); const auto width=size.X-2*inset,height=size.Y-2*inset;
        if (width<=0 || height<=0 || color.W==0) return;
        const ::RealEvaluation radius=::RealMath::Clamp(shape->get_Radius()-inset,0.0f,::RealMath::Min(width,height)/2);
        SceneBatch batch{}; batch.Viewport=Viewport;
        std::vector<SceneVertex> perimeter;
        const auto corners=static_cast<int>(shape->get_Corners());
        const auto angle=shape->get_Rotation(); const auto cosine=::RealMath::Cos(angle),sine=::RealMath::Sin(angle);
        for (int corner=0; corner<4; ++corner) {
            const int flags[]{2,8,4,1};
            const bool rounded=(corners&flags[corner])!=0 && radius>0;
            const auto cx=inset+(corner==0 || corner==1 ? width : 0)-(corner==0 || corner==1 ? radius : -radius);
            const auto cy=inset+(corner==1 || corner==2 ? height : 0)-(corner==1 || corner==2 ? radius : -radius);
            for (int segment=0; segment<=(rounded ? 8 : 0); ++segment) {
                ::RealEvaluation theta=(-0.5+corner*0.5+(rounded ? segment/16.0 : 0.25))*3.141592653589793;
                ::RealEvaluation x=rounded ? cx+::RealMath::Cos(theta)*radius : ::RealEvaluation(inset+(corner==0 || corner==1 ? width : 0));
                ::RealEvaluation y=rounded ? cy+::RealMath::Sin(theta)*radius : ::RealEvaluation(inset+(corner==1 || corner==2 ? height : 0));
                auto dx=x-size.X/2.0f,dy=y-size.Y/2.0f;
                perimeter.push_back(Vertex(shape->get_Parent(),size.X/2.0f+dx*cosine-dy*sine,size.Y/2.0f+dx*sine+dy*cosine,0,0,color));
            }
        }
        auto center=Vertex(shape->get_Parent(),size.X/2.0f,size.Y/2.0f,0,0,color);
        for (std::size_t i=0; i<perimeter.size(); ++i) {
            batch.Vertices.push_back(center); batch.Vertices.push_back(perimeter[i]); batch.Vertices.push_back(perimeter[(i+1)%perimeter.size()]);
        }
        Submit(std::move(batch));
    }
    void AndroidRenderManager2D::DrawRoundedRect(::IRoundedRectDrawable2D* shape) {
        if (!shape || !shape->get_Parent()) return;
        const auto size=shape->get_Size();
        if (!ReportedPanelBounds && size.X>200 && size.Y>100) {
            const auto matrix=shape->get_Parent()->get_WorldTransformMatrix();
            __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Authored UI panel bounds: x=%.1f y=%.1f width=%d height=%d viewport=%ux%u.",
                static_cast<double>(matrix.M41),static_cast<double>(matrix.M42),size.X,size.Y,Viewport.extent.width,Viewport.extent.height);
            ReportedPanelBounds=true;
        }
        const auto border=shape->get_BorderThickness();
        if (border>0) SubmitRoundedPolygon(shape,0,shape->get_BorderColor());
        SubmitRoundedPolygon(shape,::RealMath::Max(0.0f,border),shape->get_FillColor());
    }
}
