#pragma once
#include <android/native_activity.h>

namespace helengine::android {
    /// Reads the activity's current logical display DPI without applying viewport scale or converting coordinates.
    int ReadAndroidDisplayDensity(ANativeActivity* activity);
}
