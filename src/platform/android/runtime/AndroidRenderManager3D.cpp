#include "AndroidRenderManager3D.hpp"
#include "AndroidRenderManager2D.hpp"
#include "AndroidRuntimeModel.hpp"
#include "AndroidRuntimeMaterial.hpp"
#include "android_asset_reader.hpp"
#include "ModelAsset.hpp"
#include "ModelAssetIndexData.hpp"
#include "ModelSubmeshAsset.hpp"
#include "ModelSubmeshResolver.hpp"
#include "RuntimeSubmesh.hpp"
#include "PlatformMaterialAsset.hpp"
#include "Core.hpp"
#include "ObjectManager.hpp"
#include "MeshComponent.hpp"
#include "ICamera.hpp"
#include "IRenderQueue3D.hpp"
#include "Entity.hpp"
#include "DirectionalLightComponent.hpp"
#include "AmbientLightComponent.hpp"
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
        double length = std::sqrt(n.X*n.X+n.Y*n.Y+n.Z*n.Z);
        if (length <= 0) throw std::runtime_error("Authored model contains a zero-length transformed normal.");
        return ::float3(n.X/length,n.Y/length,n.Z/length);
    }
    AndroidRenderManager3D::AndroidRenderManager3D(AndroidVulkanRenderer& renderer, AndroidRenderManager2D& overlay) : Renderer(renderer), Overlay(overlay) {}
    ::RuntimeModel* AndroidRenderManager3D::BuildModelFromRaw(::ModelAsset* data) {
        if (!data || !data->Positions || !data->Normals || data->Normals->Length != data->Positions->Length) {
            throw std::runtime_error("Android meshes require authored positions and one normal per vertex.");
        }
        auto model = std::make_unique<AndroidRuntimeModel>();
        model->set_Id(data->get_Id());
        for (int32_t i=0; i<data->Positions->Length; ++i) { model->Positions.push_back((*data->Positions)[i]); model->Normals.push_back((*data->Normals)[i]); }
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
    ::RuntimeModel* AndroidRenderManager3D::BuildModelFromCooked(std::string path, ::IContentStreamSource* source) {
        auto asset = ReadAsset(path,source);
        auto* model = he_cpp_try_cast<::ModelAsset>(asset.get());
        if (!model) throw std::runtime_error("Android cooked model must deserialize as ModelAsset.");
        auto* result = BuildModelFromRaw(model); model->Dispose(); return result;
    }
    ::RuntimeMaterial* AndroidRenderManager3D::BuildMaterialFromCooked(std::string path, ::IContentStreamSource* source) {
        auto asset = ReadAsset(path,source);
        auto* material = he_cpp_try_cast<::PlatformMaterialAsset>(asset.get());
        if (!material) throw std::runtime_error("Android cooked material must deserialize as PlatformMaterialAsset.");
        return BuildMaterialFromCooked(material);
    }
    ::RuntimeMaterial* AndroidRenderManager3D::BuildMaterialFromCooked(::PlatformMaterialAsset* material) {
        if (!material || material->RendererFamilyId != "android-native" || !material->TextureRelativePath.empty()) throw std::runtime_error("Android requires an untextured android-native standard material.");
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
                {static_cast<uint32_t>(normalized.Z*size.width),static_cast<uint32_t>(normalized.W*size.height)}};
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
            auto clear = Camera->get_ClearSettings();
            if (clear.ClearColorEnabled) Renderer.SetClearColor(clear.ClearColor.X,clear.ClearColor.Y,clear.ClearColor.Z,clear.ClearColor.W);
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
        if (DrawCalls==0 && (FrameSequence%120)==1) {
            auto rotation=mesh->get_Parent()->get_LocalOrientation();
            __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","Authored mesh transform: model=%s quaternion=(%.4f,%.4f,%.4f,%.4f).",
                model->get_Id().c_str(),rotation.X,rotation.Y,rotation.Z,rotation.W);
        }
        if (!::float4x4::TryInverseTranspose__ref0_out1(world,normal)) throw std::runtime_error("Authored mesh transform is not invertible.");
        const double near = Camera->get_NearPlaneDistance(), far = Camera->get_FarPlaneDistance();
        if (near <= 0 || far <= near) throw std::runtime_error("Authored camera clipping planes are invalid.");
        const double f = 1/std::tan(Camera->get_FieldOfView()/2.0);
        const double aspect = static_cast<double>(Viewport.extent.width)/Viewport.extent.height;
        auto* manager = get_OwnerCore()->get_ObjectManager();
        auto* lights = manager->get_DirectionalLights();
        auto* ambientLights = manager->get_AmbientLights();
        auto* ranges = model->get_Submeshes(); auto* materials = mesh->get_Materials();
        for (int32_t rangeIndex=0; rangeIndex<ranges->Length; ++rangeIndex) {
            if (!materials || rangeIndex>=materials->Length) throw std::runtime_error("Authored mesh is missing a submesh material.");
            auto* material = dynamic_cast<AndroidRuntimeMaterial*>((*materials)[rangeIndex]);
            if (!material) throw std::runtime_error("Authored mesh uses an unsupported Android material.");
            SceneBatch batch{}; batch.Viewport = Viewport; batch.DepthTest = true;
            auto* range = (*ranges)[rangeIndex];
            for (int32_t i=range->get_IndexStart(); i<range->get_IndexStart()+range->get_IndexCount(); ++i) {
                auto index = model->Indices[i]; auto position = TransformPoint(TransformPoint(model->Positions[index],world),View);
                auto n = TransformNormal(model->Normals[index],normal);
                ::float3 light(material->Lit ? 0.0f : 1.0f,material->Lit ? 0.0f : 1.0f,material->Lit ? 0.0f : 1.0f);
                if (material->Lit) {
                    for (int32_t a=0; a<ambientLights->get_Count(); ++a) {
                        auto* l = (*ambientLights)[a]; auto c=l->get_Color(); float intensity=l->get_Intensity();
                        light.X+=c.X*intensity; light.Y+=c.Y*intensity; light.Z+=c.Z*intensity;
                    }
                    for (int32_t l=0; l<lights->get_Count(); ++l) {
                        auto* sun = (*lights)[l]; if (!sun->get_Parent()->get_IsHierarchyEnabled()) continue;
                        auto direction = ::float4::RotateVector(::float3(0,0,-1),sun->get_Parent()->get_Orientation());
                        float intensity=std::max(0.0f,-(n.X*direction.X+n.Y*direction.Y+n.Z*direction.Z))*sun->get_Intensity();
                        auto color=sun->get_Color(); light.X+=color.X*intensity; light.Y+=color.Y*intensity; light.Z+=color.Z*intensity;
                    }
                }
                const auto color=material->Color;
                SceneVertex vertex{{static_cast<float>(position.X*f/aspect),static_cast<float>(-position.Y*f),
                    static_cast<float>((far/(near-far))*position.Z+near*far/(near-far)),-position.Z},
                    {color.X*light.X,color.Y*light.Y,color.Z*light.Z,color.W},{0,0}};
                batch.Vertices.push_back(vertex);
            }
            Renderer.Submit(std::move(batch)); ++DrawCalls;
        }
    }
}
