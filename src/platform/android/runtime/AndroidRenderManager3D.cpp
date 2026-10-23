#include "AndroidRenderManager3D.hpp"
#include "AndroidRenderManager2D.hpp"
#include "AndroidRuntimeModel.hpp"
#include "AndroidRuntimeMaterial.hpp"
#include "AndroidRuntimeTexture.hpp"
#include "android_asset_reader.hpp"
#include "ModelAsset.hpp"
#include "ModelAssetIndexData.hpp"
#include "ModelSubmeshAsset.hpp"
#include "ModelSubmeshResolver.hpp"
#include "RuntimeSubmesh.hpp"
#include "PlatformMaterialAsset.hpp"
#include "AssetSerializer.hpp"
#include "EngineBinaryReader.hpp"
#include "Core.hpp"
#include "ObjectManager.hpp"
#include "MeshComponent.hpp"
#include "ICamera.hpp"
#include "IRenderQueue3D.hpp"
#include "Entity.hpp"
#include "DirectionalLightComponent.hpp"
#include "AmbientLightComponent.hpp"
#include "PointLightComponent.hpp"
#include "SpotLightComponent.hpp"
#include "runtime/native_cast.hpp"
#include <android/log.h>
#include <algorithm>
#include <cmath>

namespace helengine::android {
    /// Applies an engine matrix to a position without changing its row-vector storage convention.
    static ::float3 TransformPoint(const ::float3& p, const ::float4x4& m) {
        return ::float3(p.X*m.M11+p.Y*m.M21+p.Z*m.M31+m.M41,
            p.X*m.M12+p.Y*m.M22+p.Z*m.M32+m.M42,p.X*m.M13+p.Y*m.M23+p.Z*m.M33+m.M43);
    }
    /// Applies and normalizes an inverse-transpose normal transform.
    static ::float3 TransformNormal(const ::float3& p, const ::float4x4& m) {
        ::float3 n(p.X*m.M11+p.Y*m.M21+p.Z*m.M31,p.X*m.M12+p.Y*m.M22+p.Z*m.M32,p.X*m.M13+p.Y*m.M23+p.Z*m.M33);
        auto length = ::RealMath::Sqrt(n.X*n.X+n.Y*n.Y+n.Z*n.Z);
        if (length <= 0) throw std::runtime_error("Authored model contains a zero-length transformed normal.");
        return ::float3(n.X/length,n.Y/length,n.Z/length);
    }
    AndroidRenderManager3D::AndroidRenderManager3D(AndroidVulkanRenderer& renderer, AndroidRenderManager2D& overlay) : Renderer(renderer), Overlay(overlay) {
        UnboundMaterial.Color=::float4(1,1,1,1); UnboundMaterial.Lit=true; UnboundMaterial.ReceiveShadows=false;
    }
    ::RuntimeModel* AndroidRenderManager3D::BuildModelFromRaw(::ModelAsset* data) {
        if (!data || !data->Positions || !data->Normals || data->Normals->Length != data->Positions->Length) {
            throw std::runtime_error("Android meshes require authored positions and one normal per vertex.");
        }
        auto model = std::make_unique<AndroidRuntimeModel>();
        model->set_Id(data->get_Id());
        for (int32_t i=0; i<data->Positions->Length; ++i) { model->Positions.push_back((*data->Positions)[i]); model->Normals.push_back((*data->Normals)[i]); }
        if (data->TexCoords) {
            if (data->TexCoords->Length != data->Positions->Length) throw std::runtime_error("Android model texture coordinates must match its vertex count.");
            for (int32_t i=0; i<data->TexCoords->Length; ++i) model->TexCoords.push_back((*data->TexCoords)[i]);
        }
        std::unique_ptr<::ModelAssetIndexData> indices(::ModelAssetIndexData::Resolve(data));
        for (int32_t i=0; i<indices->IndexCount; ++i) {
            auto index = indices->Uses32BitIndices ? (*indices->Indices32)[i] : (*indices->Indices16)[i];
            if (index >= model->Positions.size()) throw std::runtime_error("Authored model index exceeds its vertex count.");
            model->Indices.push_back(index);
        }
        if (model->Indices.empty()) {
            for (std::size_t i=0; i<model->Positions.size(); ++i) model->Indices.push_back(i);
        }
        std::unique_ptr<Array<::RuntimeSubmesh*>> ranges(::ModelSubmeshResolver::BuildRuntimeSubmeshes(data));
        for (int32_t i=0; i<ranges->Length; ++i) {
            auto* range=(*ranges)[i];
            if (range->get_IndexCount()%3) throw std::runtime_error("Android supports triangle-list submesh ranges only.");
        }
        model->SetSubmeshes(ranges.get()); model->SetBounds(data->BoundsMin,data->BoundsMax);
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Authored model loaded: id=%s vertices=%zu indices=%zu.",data->get_Id().c_str(),model->Positions.size(),model->Indices.size());
        return model.release();
    }
    ::RuntimeModel* AndroidRenderManager3D::BuildModelFromCooked(HeCppString path, ::IContentStreamSource* source) {
        auto asset = ReadAsset(path,source);
        auto* model = he_cpp_try_cast<::ModelAsset>(asset.get());
        if (!model) throw std::runtime_error("Android cooked model must deserialize as ModelAsset.");
        auto* result = BuildModelFromRaw(model); model->Dispose(); return result;
    }
    ::RuntimeMaterial* AndroidRenderManager3D::BuildMaterialFromCooked(HeCppString path, ::IContentStreamSource* source) {
        if (!source || path.empty()) throw std::invalid_argument("Android material loading requires a content source and path.");
        std::unique_ptr<::Stream> stream(source->OpenRead(path));
        std::unique_ptr<::Asset> asset(::AssetSerializer::Deserialize(stream.get()));
        auto* material = he_cpp_try_cast<::PlatformMaterialAsset>(asset.get());
        if (!material) throw std::runtime_error("Android cooked material must deserialize as PlatformMaterialAsset.");
        auto result=std::unique_ptr<AndroidRuntimeMaterial>(static_cast<AndroidRuntimeMaterial*>(BuildMaterialFromCooked(material)));
        if (!material->TextureRelativePath.empty()) {
            std::unique_ptr<::RuntimeTexture> texture(Overlay.BuildTextureFromCooked(material->TextureRelativePath,source));
            auto* androidTexture=dynamic_cast<AndroidRuntimeTexture*>(texture.get());
            if (!androidTexture || !androidTexture->Image) throw std::runtime_error("Android material texture did not produce resident RGBA pixels.");
            result->Texture=androidTexture->Image;
            result->Texture->Srgb=true; result->Texture->Repeat=true;
        }
        if (stream->Position()<stream->Length()) {
            std::unique_ptr<::EngineBinaryReader> reader(::EngineBinaryReader::Create(stream.get(),::EngineBinaryEndianness::LittleEndian,true));
            if (reader->ReadUInt32()!=0x414d4548 || reader->ReadByte()!=1) throw std::runtime_error("Unsupported Android cooked material parameter version.");
            result->Metallic=reader->ReadSingle(); result->Roughness=reader->ReadSingle(); result->Specular=reader->ReadSingle();
            result->CastShadows=reader->ReadByte()!=0; result->ReceiveShadows=reader->ReadByte()!=0; result->AlphaMode=reader->ReadByte();
            const auto roughnessPath=reader->ReadString();
            if (!std::isfinite(result->Metallic) || result->Metallic<0 || result->Metallic>1 || !std::isfinite(result->Roughness) || result->Roughness<0 || result->Roughness>1
                || !std::isfinite(result->Specular) || result->Specular<0 || result->Specular>1 || result->AlphaMode>2 || stream->Position()!=stream->Length()) {
                throw std::runtime_error("Android material contains invalid shading parameters.");
            }
            if (!roughnessPath.empty()) {
                std::unique_ptr<::RuntimeTexture> texture(Overlay.BuildTextureFromCooked(roughnessPath,source));
                result->RoughnessTexture=static_cast<AndroidRuntimeTexture*>(texture.get())->Image;
                result->RoughnessTexture->Repeat=true;
            }
        }
        return result.release();
    }
    ::RuntimeMaterial* AndroidRenderManager3D::BuildMaterialFromCooked(::PlatformMaterialAsset* material) {
        if (!material || material->RendererFamilyId != "android-native") throw std::runtime_error("Android requires an android-native standard material.");
        auto* result = new AndroidRuntimeMaterial();
        result->set_Id(material->get_Id()); result->Lit = material->Lit;
        result->Color = ::float4(material->BaseColorR/255.0f,material->BaseColorG/255.0f,material->BaseColorB/255.0f,material->BaseColorA/255.0f);
        return result;
    }
    void AndroidRenderManager3D::Draw() {
        DrawCalls = 0;
        ++FrameSequence;
        auto* cameras = get_OwnerCore()->get_ObjectManager()->get_Cameras();
        for (int32_t i=0; i<cameras->get_Count(); ++i) {
            Camera = (*cameras)[i];
            if (!Camera || !Camera->get_Parent()->get_IsHierarchyEnabled()) continue;
            const auto size = Renderer.GetExtent(); const auto normalized = Camera->get_Viewport();
            Viewport = {{static_cast<int32_t>(normalized.X*size.width),static_cast<int32_t>(normalized.Y*size.height)},
                {static_cast<uint32_t>(static_cast<int32_t>(normalized.Z*size.width)),static_cast<uint32_t>(static_cast<int32_t>(normalized.W*size.height))}};
            if (!Viewport.extent.width || !Viewport.extent.height) continue;
            if (ReportedViewport.width!=Viewport.extent.width || ReportedViewport.height!=Viewport.extent.height) {
                ReportedViewport=Viewport.extent;
                __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Authored camera viewport: %ux%u aspect=%.6f.",
                    Viewport.extent.width,Viewport.extent.height,static_cast<double>(Viewport.extent.width)/Viewport.extent.height);
            }
            if (Viewport.offset.x < 0 || Viewport.offset.y < 0 || Viewport.offset.x+Viewport.extent.width > size.width || Viewport.offset.y+Viewport.extent.height > size.height) throw std::runtime_error("Android camera viewport exceeds its render surface.");
            auto world = Camera->get_Parent()->get_WorldTransformMatrix();
            ::float4x4 inverseTranspose;
            if (!::float4x4::TryInverseTranspose__ref0_out1(world,inverseTranspose)) throw std::runtime_error("Authored camera transform is not invertible.");
            ::float4x4::Transpose__ref0_out1(inverseTranspose,View);
            CaptureCameraLighting();
            auto clear = Camera->get_ClearSettings();
            {
                SceneBatch batch{}; batch.WorldSpace=true; batch.BeginCamera=true;
                batch.Viewport=Viewport; batch.ClearColor=clear.ClearColorEnabled; batch.ClearDepth=clear.ClearDepthEnabled;
                batch.ClearValue[0]=static_cast<float>(clear.ClearColor.X); batch.ClearValue[1]=static_cast<float>(clear.ClearColor.Y); batch.ClearValue[2]=static_cast<float>(clear.ClearColor.Z); batch.ClearValue[3]=static_cast<float>(clear.ClearColor.W);
                batch.ClearDepthValue=clear.ClearDepth; Renderer.Submit(std::move(batch));
            }
            if (Camera->get_RenderQueue3D()) Camera->get_RenderQueue3D()->VisitOrdered(this);
            Overlay.DrawCamera(Camera,Viewport);
        }
        Camera = nullptr;
    }
    void AndroidRenderManager3D::Visit(::IDrawable3D* drawable) {
        auto* mesh = he_cpp_try_cast<::MeshComponent>(drawable);
        if (!mesh || !mesh->get_Parent()->get_IsHierarchyEnabled()) return;
        auto* model = dynamic_cast<AndroidRuntimeModel*>(mesh->get_Model());
        if (!model) throw std::runtime_error("Android received a mesh without an Android runtime model.");
        auto world = mesh->get_Parent()->get_WorldTransformMatrix(); ::float4x4 normal;
        if (DrawCalls<4 && (FrameSequence%120)==1) {
            auto rotation=mesh->get_Parent()->get_LocalOrientation();
            auto position=mesh->get_Parent()->get_Position();
            __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Authored mesh transform: draw=%d model=%s position=(%.4f,%.4f,%.4f) quaternion=(%.4f,%.4f,%.4f,%.4f).",
                DrawCalls,model->get_Id().c_str(),static_cast<double>(position.X),static_cast<double>(position.Y),static_cast<double>(position.Z),static_cast<double>(rotation.X),static_cast<double>(rotation.Y),static_cast<double>(rotation.Z),static_cast<double>(rotation.W));
        }
        if (!::float4x4::TryInverseTranspose__ref0_out1(world,normal)) throw std::runtime_error("Authored mesh transform is not invertible.");
        const auto near = Camera->get_NearPlaneDistance(), far = Camera->get_FarPlaneDistance();
        if (near <= 0 || far <= near) throw std::runtime_error("Authored camera clipping planes are invalid.");
        const auto f = 1/::RealMath::Tan(Camera->get_FieldOfView()/2.0);
        const ::RealEvaluation aspect = ::RealEvaluation(Viewport.extent.width)/Viewport.extent.height;
        auto* ranges = model->get_Submeshes(); auto* materials = mesh->get_Materials();
        for (int32_t rangeIndex=0; rangeIndex<ranges->Length; ++rangeIndex) {
            if (!materials) throw std::runtime_error("Authored mesh material slots must be initialized.");
            // Match the shared player contract: imported meshes may have no bindings, and additional submeshes reuse slot zero.
            auto* material = materials->Length==0?&UnboundMaterial:dynamic_cast<AndroidRuntimeMaterial*>((*materials)[rangeIndex<materials->Length?rangeIndex:0]);
            if (!material) throw std::runtime_error("Authored mesh uses an unsupported Android material.");
            SceneBatch batch{}; batch.WorldSpace=true; batch.Viewport = Viewport; batch.DepthTest = true;
            batch.Texture=material->Texture;
            batch.RoughnessTexture=material->RoughnessTexture; batch.Transparent=material->AlphaMode==2;
            batch.Lighting=CameraLighting;
            batch.Lighting.Surface[0]=material->Metallic; batch.Lighting.Surface[1]=material->Roughness;
            batch.Lighting.Surface[2]=material->Specular; batch.Lighting.Surface[3]=material->Lit?1.0f:0.0f;
            batch.Lighting.Options[0]=material->AlphaMode; batch.Lighting.Options[1]=material->RoughnessTexture?1.0f:0.0f;
            batch.Lighting.ShadowParameters[2]=material->ReceiveShadows?1.0f:0.0f;
            batch.CastShadow=get_ShadowsEnabled() && material->CastShadows && material->AlphaMode!=2;
            if (material->Texture && model->TexCoords.size()!=model->Positions.size()) throw std::runtime_error("Textured Android meshes require authored texture coordinates.");
            auto* range = (*ranges)[rangeIndex];
            for (int32_t i=range->get_IndexStart(); i<range->get_IndexStart()+range->get_IndexCount(); ++i) {
                auto index = model->Indices[i]; auto worldPosition=TransformPoint(model->Positions[index],world); auto position = TransformPoint(worldPosition,View);
                auto n = TransformNormal(model->Normals[index],normal);
                const auto color=material->Color;
                const auto uv=model->TexCoords.empty() ? ::float2(0,0) : model->TexCoords[index];
                SceneVertex vertex{{static_cast<float>(position.X*f/aspect),static_cast<float>(-position.Y*f),
                    static_cast<float>((far/(near-far))*position.Z+near*far/(near-far)),static_cast<float>(-position.Z)},
                    {static_cast<float>(color.X),static_cast<float>(color.Y),static_cast<float>(color.Z),static_cast<float>(color.W)},
                    {static_cast<float>(uv.X),static_cast<float>(uv.Y)},
                    {static_cast<float>(worldPosition.X),static_cast<float>(worldPosition.Y),static_cast<float>(worldPosition.Z)},
                    {static_cast<float>(n.X),static_cast<float>(n.Y),static_cast<float>(n.Z)}};
                batch.Vertices.push_back(vertex);
            }
            Renderer.Submit(std::move(batch)); ++DrawCalls;
        }
    }
    void AndroidRenderManager3D::CaptureCameraLighting() {
        CameraLighting={};
        const auto position=Camera->get_Parent()->get_Position();
        CameraLighting.Camera[0]=static_cast<float>(position.X); CameraLighting.Camera[1]=static_cast<float>(position.Y); CameraLighting.Camera[2]=static_cast<float>(position.Z);
        auto* manager=get_OwnerCore()->get_ObjectManager();
        auto* ambient=manager->get_AmbientLights();
        for (int32_t index=0;index<ambient->get_Count();++index) {
            auto* light=(*ambient)[index];
            if (!light->get_Parent()->get_IsHierarchyEnabled()) continue;
            auto color=light->get_Color(); const auto intensity=light->get_Intensity();
            // SceneLighting is the fixed float32 Vulkan uniform block.
            CameraLighting.Ambient[0]+=static_cast<float>(color.X*intensity); CameraLighting.Ambient[1]+=static_cast<float>(color.Y*intensity); CameraLighting.Ambient[2]+=static_cast<float>(color.Z*intensity);
        }
        int count=0;
        const int lightBudget=std::clamp(get_MaximumVisibleLights(),0,8);
        auto* directional=manager->get_DirectionalLights();
        for (int32_t index=0;index<directional->get_Count();++index) {
            auto* light=(*directional)[index];
            if (!light->get_Parent()->get_IsHierarchyEnabled()) continue;
            if (count>=lightBudget) break;
            const auto direction=::float4::RotateVector(::float3(0,0,-1),light->get_Parent()->get_Orientation());
            const auto color=light->get_Color(); const auto intensity=light->get_Intensity();
            CameraLighting.LightPositions[count][0]=static_cast<float>(direction.X); CameraLighting.LightPositions[count][1]=static_cast<float>(direction.Y); CameraLighting.LightPositions[count][2]=static_cast<float>(direction.Z);
            CameraLighting.LightColors[count][0]=static_cast<float>(color.X*intensity); CameraLighting.LightColors[count][1]=static_cast<float>(color.Y*intensity); CameraLighting.LightColors[count][2]=static_cast<float>(color.Z*intensity);
            if (count==0) {
                CameraLighting.ShadowParameters[0]=static_cast<float>(get_ShadowsEnabled() && light->get_ShadowsEnabled()?1.0f:0.0f);
                CameraLighting.ShadowParameters[1]=0.0005f; CameraLighting.ShadowParameters[3]=static_cast<float>(light->get_ShadowDistance());
            }
            ++count;
        }
        auto* points=manager->get_PointLights();
        for (int32_t index=0;index<points->get_Count();++index) {
            auto* light=(*points)[index];
            if (!light->get_Parent()->get_IsHierarchyEnabled()) continue;
            if (count>=lightBudget) break;
            const auto location=light->get_Parent()->get_Position(); const auto color=light->get_Color(); const auto intensity=light->get_Intensity();
            CameraLighting.LightPositions[count][0]=static_cast<float>(location.X); CameraLighting.LightPositions[count][1]=static_cast<float>(location.Y); CameraLighting.LightPositions[count][2]=static_cast<float>(location.Z); CameraLighting.LightPositions[count][3]=1;
            CameraLighting.LightColors[count][0]=static_cast<float>(color.X*intensity); CameraLighting.LightColors[count][1]=static_cast<float>(color.Y*intensity); CameraLighting.LightColors[count][2]=static_cast<float>(color.Z*intensity);
            CameraLighting.LightParameters[count][0]=static_cast<float>(light->get_Range()); ++count;
        }
        auto* spots=manager->get_SpotLights();
        for (int32_t index=0;index<spots->get_Count();++index) {
            auto* light=(*spots)[index];
            if (!light->get_Parent()->get_IsHierarchyEnabled()) continue;
            if (count>=lightBudget) break;
            const auto location=light->get_Parent()->get_Position(); const auto color=light->get_Color(); const auto intensity=light->get_Intensity();
            const auto direction=::float4::RotateVector(::float3(0,0,-1),light->get_Parent()->get_Orientation());
            CameraLighting.LightPositions[count][0]=static_cast<float>(location.X); CameraLighting.LightPositions[count][1]=static_cast<float>(location.Y); CameraLighting.LightPositions[count][2]=static_cast<float>(location.Z); CameraLighting.LightPositions[count][3]=2;
            CameraLighting.LightColors[count][0]=static_cast<float>(color.X*intensity); CameraLighting.LightColors[count][1]=static_cast<float>(color.Y*intensity); CameraLighting.LightColors[count][2]=static_cast<float>(color.Z*intensity);
            CameraLighting.LightDirections[count][0]=static_cast<float>(direction.X); CameraLighting.LightDirections[count][1]=static_cast<float>(direction.Y); CameraLighting.LightDirections[count][2]=static_cast<float>(direction.Z);
            CameraLighting.LightParameters[count][0]=static_cast<float>(light->get_Range());
            CameraLighting.LightParameters[count][1]=static_cast<float>(::RealMath::Cos(light->get_InnerConeAngleDegrees()*3.141592653589793/180.0));
            CameraLighting.LightParameters[count][2]=static_cast<float>(::RealMath::Cos(light->get_OuterConeAngleDegrees()*3.141592653589793/180.0)); ++count;
        }
        CameraLighting.Options[2]=count;
    }
}
