#include "AndroidDisplayDensity.hpp"
#include <stdexcept>

namespace helengine::android {
    int ReadAndroidDisplayDensity(ANativeActivity* activity) {
        JNIEnv* env=nullptr;
        const int status=activity->vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6);
        const bool attach=status==JNI_EDETACHED;
        if ((attach && activity->vm->AttachCurrentThread(&env,nullptr)!=JNI_OK) || (!attach && status!=JNI_OK)) {
            throw std::runtime_error("Could not access Android display metrics from the native host thread.");
        }
        if (env->PushLocalFrame(8)<0) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (attach) activity->vm->DetachCurrentThread();
            throw std::runtime_error("Could not allocate Android display-metrics JNI references.");
        }
        try {
            auto activityClass=env->GetObjectClass(activity->clazz);
            auto getResources=env->GetMethodID(activityClass,"getResources","()Landroid/content/res/Resources;");
            if (!getResources) throw std::runtime_error("Android activity has no resource accessor.");
            auto resources=env->CallObjectMethod(activity->clazz,getResources);
            if (env->ExceptionCheck() || !resources) throw std::runtime_error("Could not read current Android activity resources.");
            auto resourcesClass=env->GetObjectClass(resources);
            auto getMetrics=env->GetMethodID(resourcesClass,"getDisplayMetrics","()Landroid/util/DisplayMetrics;");
            if (!getMetrics) throw std::runtime_error("Android resources have no display metrics accessor.");
            auto metrics=env->CallObjectMethod(resources,getMetrics);
            if (env->ExceptionCheck() || !metrics) throw std::runtime_error("Could not read current Android display metrics.");
            auto metricsClass=env->GetObjectClass(metrics);
            auto densityField=env->GetFieldID(metricsClass,"densityDpi","I");
            if (!densityField) throw std::runtime_error("Android display metrics have no density measurement.");
            const int dpi=env->GetIntField(metrics,densityField);
            if (env->ExceptionCheck() || dpi<=0) throw std::runtime_error("Android display density is unavailable.");
            env->PopLocalFrame(nullptr);
            if (attach) activity->vm->DetachCurrentThread();
            return dpi;
        } catch (...) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->PopLocalFrame(nullptr);
            if (attach) activity->vm->DetachCurrentThread();
            throw;
        }
    }
}
