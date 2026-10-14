#include "../src/platform/android/rendering/SceneResolutionMath.hpp"
#include "../src/platform/android/rendering/SceneFramePlanner.hpp"
#include "ScenePlanTestBatch.hpp"
#include <iostream>
#include <limits>

/// Exercises the production resolution math and pass planner without an emulator or graphics host.
class SceneResolutionTests {
public:
    /// Runs numerical, camera-order, native-overlay and reusable-storage checks.
    static void Run() {
        ResolutionDimensions();
        RoundedEdges();
        InvalidQuality();
        EmptyAndOverlayOnlyFrames();
        OrderedCameraOverlays();
        CamerasWithoutColorClears();
        DepthOnlyCameraAndStorageReuse();
        std::cout<<"7 native scene-resolution tests passed.\n";
    }
private:
    /// Throws with the behavioral expectation rather than depending on debug-only assertions.
    static void Check(bool valid,const char* message) { if (!valid) throw std::runtime_error(message); }
    /// Standard landscape scales keep native display dimensions independent from target dimensions.
    static void ResolutionDimensions() {
        using helengine::android::SceneResolutionMath;
        Check(SceneResolutionMath::ScaleDimension(1920,0.5)==960,"Half-resolution width must be 960.");
        Check(SceneResolutionMath::ScaleDimension(1080,0.5)==540,"Half-resolution height must be 540.");
        Check(SceneResolutionMath::ScaleDimension(1920,0.75)==1440,"75 percent width must be 1440.");
        Check(SceneResolutionMath::ScaleDimension(1080,0.75)==810,"75 percent height must be 810.");
        Check(SceneResolutionMath::ScaleDimension(1920,1)==1920,"Full-quality width must match the display.");
        Check(SceneResolutionMath::ScaleDimension(1,0.25)==1,"A small window must retain one target pixel.");
        Check(SceneResolutionMath::ScaleDimension(1921,0.5)==961,"Odd widths must round consistently.");
    }
    /// Shared camera boundaries are mapped through actual target dimensions, preventing independently rounded seams.
    static void RoundedEdges() {
        using helengine::android::SceneResolutionMath;
        const auto target=SceneResolutionMath::ScaleDimension(1921,0.75);
        const auto first=SceneResolutionMath::MapEdge(0,1921,target);
        const auto shared=SceneResolutionMath::MapEdge(960,1921,target);
        const auto end=SceneResolutionMath::MapEdge(1921,1921,target);
        Check(first==0 && end==target,"The entire physical viewport must cover the entire internal surface.");
        Check((shared-first)+(end-shared)==target,"Adjacent camera regions must share one mapped edge.");
        for (std::uint32_t edge=1;edge<=1921;++edge) {
            Check(SceneResolutionMath::MapEdge(edge,1921,target)>=SceneResolutionMath::MapEdge(edge-1,1921,target),
                "Mapped edges must remain monotonic.");
        }
    }
    /// Rejects non-finite and unsupported quality before resources can change.
    static void InvalidQuality() {
        const double invalid[]{0,0.249,1.001,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()};
        for (double value:invalid) {
            bool rejected=false;
            try { helengine::android::SceneResolutionMath::ValidateScale(value); }
            catch (const std::invalid_argument&) { rejected=true; }
            Check(rejected,"Invalid render scales must fail validation.");
        }
    }
    /// Checks every production operation runs inside its required native or world pass.
    static void VerifyScopes(const std::vector<helengine::android::SceneFrameCommand>& plan) {
        using helengine::android::SceneFrameOperation;
        bool native=false,world=false;
        for (const auto& command:plan) {
            switch (command.Operation) {
                case SceneFrameOperation::BeginPresentation:
                case SceneFrameOperation::ResumePresentation: Check(!native && !world,"Presentation passes cannot nest."); native=true; break;
                case SceneFrameOperation::EndPresentation: Check(native && !world,"Only an active presentation pass can end."); native=false; break;
                case SceneFrameOperation::BeginWorld: Check(!native && !world,"World work must be outside the presentation pass."); world=true; break;
                case SceneFrameOperation::EndWorld: Check(world && !native,"Only an active world pass can end."); world=false; break;
                case SceneFrameOperation::DrawWorld: Check(world && !native,"Meshes and world depth clears must use the internal target."); break;
                case SceneFrameOperation::ClearCameraColor:
                case SceneFrameOperation::CompositeWorld:
                case SceneFrameOperation::DrawOverlay: Check(native && !world,"UI, backdrop clears and composites must remain native-resolution."); break;
            }
        }
        Check(!native && !world,"The frame must end every open pass before presentation.");
    }
    /// Finds a required behavior in the plan while preserving its original batch identity.
    static std::size_t Find(const std::vector<helengine::android::SceneFrameCommand>& plan,
        helengine::android::SceneFrameOperation operation,std::size_t batch) {
        for (std::size_t index=0;index<plan.size();++index) {
            if (plan[index].Operation==operation && plan[index].BatchIndex==batch) return index;
        }
        throw std::runtime_error("Required scene pass operation is absent.");
    }
    /// Empty and 2D-only frames cannot accidentally downsample text or sprites.
    static void EmptyAndOverlayOnlyFrames() {
        using namespace helengine::android;
        std::vector<SceneFrameCommand> plan;
        SceneFramePlanner::Build(std::vector<ScenePlanTestBatch>{},plan);
        Check(plan.size()==2,"An empty frame only opens and closes presentation.");
        VerifyScopes(plan);
        SceneFramePlanner::Build(std::vector<ScenePlanTestBatch>{{false,false,false,false},{false,false,false,false}},plan);
        VerifyScopes(plan);
        Check(plan.size()==4 && plan[1].Operation==SceneFrameOperation::DrawOverlay && plan[2].BatchIndex==1,
            "2D-only frames must remain in the native pass in original order.");
    }
    /// Each camera composites before its UI, and a later camera's clear cannot move before earlier UI.
    static void OrderedCameraOverlays() {
        using namespace helengine::android;
        const std::vector<ScenePlanTestBatch> batches{{true,true,true,true},{true,false,false,false},{false,false,false,false},
            {true,true,true,true},{true,false,false,false},{false,false,false,false}};
        std::vector<SceneFrameCommand> plan;
        SceneFramePlanner::Build(batches,plan); VerifyScopes(plan);
        Check(Find(plan,SceneFrameOperation::DrawWorld,1)<Find(plan,SceneFrameOperation::CompositeWorld,0),"First camera meshes precede their composite.");
        Check(Find(plan,SceneFrameOperation::CompositeWorld,0)<Find(plan,SceneFrameOperation::DrawOverlay,2),"First camera UI must be drawn after its composite.");
        Check(Find(plan,SceneFrameOperation::DrawOverlay,2)<Find(plan,SceneFrameOperation::ClearCameraColor,3),"Later camera clears must preserve authored order.");
        Check(Find(plan,SceneFrameOperation::CompositeWorld,3)<Find(plan,SceneFrameOperation::DrawOverlay,5),"Second camera UI must remain native and above its world.");
    }
    /// Camera markers delimit compositing even if clear policies and overlay queues are both empty.
    static void CamerasWithoutColorClears() {
        using namespace helengine::android;
        const std::vector<ScenePlanTestBatch> batches{{true,true,false,false},{true,false,false,false},
            {true,true,false,false},{true,false,false,false}};
        std::vector<SceneFrameCommand> plan;
        SceneFramePlanner::Build(batches,plan); VerifyScopes(plan);
        Check(Find(plan,SceneFrameOperation::CompositeWorld,0)<Find(plan,SceneFrameOperation::BeginWorld,2),"Adjacent cameras must composite separately.");
        for (const auto& command:plan) Check(command.Operation!=SceneFrameOperation::ClearCameraColor,"A disabled camera clear must not be synthesized.");
        Check(Find(plan,SceneFrameOperation::DrawWorld,3)<Find(plan,SceneFrameOperation::CompositeWorld,2),"The final camera must also composite.");
    }
    /// A camera's depth-only marker survives planning, and warm frames retain their command buffer allocation.
    static void DepthOnlyCameraAndStorageReuse() {
        using namespace helengine::android;
        const std::vector<ScenePlanTestBatch> batches{{true,true,false,true},{false,false,false,false}};
        std::vector<SceneFrameCommand> plan;
        SceneFramePlanner::Build(batches,plan); VerifyScopes(plan);
        Check(Find(plan,SceneFrameOperation::BeginWorld,0)<Find(plan,SceneFrameOperation::DrawWorld,0),"Depth clears require an active world pass even without meshes.");
        const auto storage=plan.data();
        for (int index=0;index<100;++index) {
            SceneFramePlanner::Build(batches,plan);
            Check(plan.data()==storage,"Warm scene planning must reuse its command storage.");
        }
    }
};

/// Runs independent CPU checks and reports failures through a nonzero exit code.
int main() {
    try { SceneResolutionTests::Run(); return 0; }
    catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
