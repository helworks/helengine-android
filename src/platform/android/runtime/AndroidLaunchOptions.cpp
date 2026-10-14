#include "AndroidLaunchOptions.hpp"
#include <stdexcept>

namespace helengine::android {
    std::string ReadAndroidStartupSceneOverride(ANativeActivity* activity) {
        JNIEnv* env=nullptr;
        const int status=activity->vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6);
        const bool attach=status==JNI_EDETACHED;
        if ((attach && activity->vm->AttachCurrentThread(&env,nullptr)!=JNI_OK) || (!attach && status!=JNI_OK)) throw std::runtime_error("Could not access Android launch intent.");
        if (env->PushLocalFrame(8)<0) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (attach) activity->vm->DetachCurrentThread();
            throw std::runtime_error("Could not allocate Android launch-option JNI references.");
        }
        try {
            auto activityClass=env->GetObjectClass(activity->clazz);
            auto getIntent=env->GetMethodID(activityClass,"getIntent","()Landroid/content/Intent;");
            if (!getIntent) throw std::runtime_error("Android activity has no intent accessor.");
            auto intent=env->CallObjectMethod(activity->clazz,getIntent);
            if (env->ExceptionCheck() || !intent) throw std::runtime_error("Could not read the Android launch intent.");
            auto intentClass=env->GetObjectClass(intent);
            auto getExtra=env->GetMethodID(intentClass,"getStringExtra","(Ljava/lang/String;)Ljava/lang/String;");
            if (!getExtra) throw std::runtime_error("Android launch intent has no string extra accessor.");
            auto key=env->NewStringUTF("helengine.startup_scene");
            auto value=static_cast<jstring>(env->CallObjectMethod(intent,getExtra,key));
            if (env->ExceptionCheck()) throw std::runtime_error("Could not read the Android startup scene override.");
            std::string result;
            if (value) {
                const auto* characters=env->GetStringUTFChars(value,nullptr);
                if (!characters) throw std::runtime_error("Could not decode the Android startup scene override.");
                result=characters; env->ReleaseStringUTFChars(value,characters);
            }
            env->PopLocalFrame(nullptr); if (attach) activity->vm->DetachCurrentThread();
            return result;
        } catch (...) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->PopLocalFrame(nullptr); if (attach) activity->vm->DetachCurrentThread();
            throw;
        }
    }
}
