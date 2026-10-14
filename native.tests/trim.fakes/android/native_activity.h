#pragma once
#include <jni.h>
/// Minimal activity boundary keeps the same object-reference field consumed by the real trim registration.
struct ANativeActivity {
    /// Retained Java activity reference used by JNI identity checks.
    jobject clazz=nullptr;
};
