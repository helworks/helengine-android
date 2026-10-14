#pragma once
#include <android/native_activity.h>
#include <string>

namespace helengine::android {
    /// Reads an explicitly requested debug startup scene from the activity intent; an absent extra keeps the generated project startup.
    std::string ReadAndroidStartupSceneOverride(ANativeActivity* activity);
}
