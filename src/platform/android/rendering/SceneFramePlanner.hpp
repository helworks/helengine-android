#pragma once
#include "SceneFrameCommand.hpp"
#include <vector>

namespace helengine::android {
    /// Plans each camera's scaled world pass before its native-resolution overlays, including cameras without clear commands.
    class SceneFramePlanner {
    public:
        /// Reuses command storage and preserves original batch indices for vertex and lighting offsets.
        template<typename Batch>
        static void Build(const std::vector<Batch>& batches,std::vector<SceneFrameCommand>& commands) {
            commands.clear();
            commands.reserve(batches.size()*5+2);
            commands.push_back({SceneFrameOperation::BeginPresentation,0});
            bool world=false;
            std::size_t camera=0;
            for (std::size_t index=0;index<batches.size();++index) {
                const auto& batch=batches[index];
                if (batch.WorldSpace) {
                    if (world && batch.BeginCamera) { FinishWorld(commands,camera); world=false; }
                    if (!world) {
                        if (batch.ClearColor) commands.push_back({SceneFrameOperation::ClearCameraColor,index});
                        commands.push_back({SceneFrameOperation::EndPresentation,index});
                        commands.push_back({SceneFrameOperation::BeginWorld,index});
                        camera=index;
                        world=true;
                    }
                    commands.push_back({SceneFrameOperation::DrawWorld,index});
                } else {
                    if (world) { FinishWorld(commands,camera); world=false; }
                    commands.push_back({SceneFrameOperation::DrawOverlay,index});
                }
            }
            if (world) FinishWorld(commands,camera);
            commands.push_back({SceneFrameOperation::EndPresentation,0});
        }
    private:
        /// Returns to the preserved presentation image and composites only this camera's world region.
        static void FinishWorld(std::vector<SceneFrameCommand>& commands,std::size_t camera) {
            commands.push_back({SceneFrameOperation::EndWorld,camera});
            commands.push_back({SceneFrameOperation::ResumePresentation,camera});
            commands.push_back({SceneFrameOperation::CompositeWorld,camera});
        }
    };
}
