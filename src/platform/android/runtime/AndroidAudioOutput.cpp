#include "AndroidAudioOutput.hpp"
#include <android/log.h>
#include <android/api-level.h>
#include <dlfcn.h>
#include <stdexcept>
#include <string>

namespace helengine::android {
    void AndroidAudioOutput::Check(aaudio_result_t result,const char* operation) {
        if (result<AAUDIO_OK) throw std::runtime_error(std::string(operation)+": "+AAudio_convertResultToText(result));
    }
    AndroidAudioOutput::~AndroidAudioOutput() {
        if (Stream) { AAudioStream_requestStop(Stream); AAudioStream_close(Stream); }
    }
    void AndroidAudioOutput::Initialize(AAudioStream_dataCallback callback,void* userData) {
        if (Initialized || !callback || !userData) throw std::invalid_argument("Android audio output requires one live mixer callback and context.");
        Callback=callback; CallbackContext=userData;
        if (!Paused) OpenStream();
        Initialized=true;
    }
    void AndroidAudioOutput::SetPaused(bool paused) {
        Paused=paused;
        if (!Initialized) return;
        if (Paused) CloseStream(); else Update();
    }
    void AndroidAudioOutput::CloseStream() {
        if (!Stream) return;
        auto* retired=Stream;
        Stream=nullptr;
        const auto stopped=AAudioStream_requestStop(retired);
        const auto closed=AAudioStream_close(retired);
        // close() finishes callbacks before their context can be reused by the next stream.
        const auto pending=Error.exchange(AAUDIO_OK,std::memory_order_acq_rel);
        if (stopped!=AAUDIO_ERROR_DISCONNECTED) Check(stopped,"AAudioStream_requestStop");
        Check(closed,"AAudioStream_close");
        if (pending!=AAUDIO_ERROR_DISCONNECTED) Check(pending,"AAudio asynchronous output failure during close");
    }
    void AndroidAudioOutput::OpenStream() {
        // Usage/content setters were introduced in API 28; API 26/27 retain the matching Java request's MEDIA attributes.
        using SetUsageFunction=void (*)(AAudioStreamBuilder*,aaudio_usage_t);
        using SetContentFunction=void (*)(AAudioStreamBuilder*,aaudio_content_type_t);
        SetUsageFunction setUsage=nullptr;
        SetContentFunction setContent=nullptr;
        if (android_get_device_api_level()>=28) {
            setUsage=reinterpret_cast<SetUsageFunction>(dlsym(RTLD_DEFAULT,"AAudioStreamBuilder_setUsage"));
            setContent=reinterpret_cast<SetContentFunction>(dlsym(RTLD_DEFAULT,"AAudioStreamBuilder_setContentType"));
            if (!setUsage || !setContent) throw std::runtime_error("AAudio game audio attributes are unavailable on this Android API.");
        }
        AAudioStreamBuilder* builder=nullptr;
        Check(AAudio_createStreamBuilder(&builder),"AAudio_createStreamBuilder");
        AAudioStreamBuilder_setDirection(builder,AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setFormat(builder,AAUDIO_FORMAT_PCM_FLOAT);
        AAudioStreamBuilder_setChannelCount(builder,2);
        AAudioStreamBuilder_setSampleRate(builder,SampleRate);
        AAudioStreamBuilder_setSharingMode(builder,AAUDIO_SHARING_MODE_SHARED);
        AAudioStreamBuilder_setPerformanceMode(builder,AAUDIO_PERFORMANCE_MODE_NONE);
        if (setUsage) { setUsage(builder,AAUDIO_USAGE_GAME); setContent(builder,AAUDIO_CONTENT_TYPE_MUSIC); }
        AAudioStreamBuilder_setDataCallback(builder,Callback,CallbackContext);
        AAudioStreamBuilder_setErrorCallback(builder,ReportError,this);
        AAudioStream* candidate=nullptr;
        const auto opened=AAudioStreamBuilder_openStream(builder,&candidate);
        const auto deleted=AAudioStreamBuilder_delete(builder);
        if (opened<AAUDIO_OK || deleted<AAUDIO_OK) {
            if (candidate) AAudioStream_close(candidate);
            Check(deleted,"AAudioStreamBuilder_delete");
            if (opened==AAUDIO_ERROR_DISCONNECTED) return;
            Check(opened,"AAudioStreamBuilder_openStream");
        }
        if (!candidate) throw std::runtime_error("AAudio opened no output stream.");
        if (AAudioStream_getFormat(candidate)!=AAUDIO_FORMAT_PCM_FLOAT || AAudioStream_getChannelCount(candidate)!=2 || AAudioStream_getSampleRate(candidate)!=SampleRate) {
            AAudioStream_close(candidate);
            throw std::runtime_error("Android audio requires 48000 Hz stereo PCM float output.");
        }
        const auto started=AAudioStream_requestStart(candidate);
        if (started<AAUDIO_OK) {
            AAudioStream_close(candidate);
            if (started==AAUDIO_ERROR_DISCONNECTED) {
                const auto pending=Error.exchange(AAUDIO_OK,std::memory_order_acq_rel);
                if (pending!=AAUDIO_ERROR_DISCONNECTED) Check(pending,"AAudio asynchronous output failure during start");
                return;
            }
            Check(started,"AAudioStream_requestStart");
        }
        Stream=candidate;
        __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","AAudio output opened: sampleRate=%d channels=2.",SampleRate);
    }
    void AndroidAudioOutput::Update() {
        if (!Initialized) return;
        auto error=Error.exchange(AAUDIO_OK,std::memory_order_acq_rel);
        if (error==AAUDIO_OK && Stream && AAudioStream_getState(Stream)==AAUDIO_STREAM_STATE_DISCONNECTED) error=AAUDIO_ERROR_DISCONNECTED;
        if (error==AAUDIO_ERROR_DISCONNECTED) {
            CloseStream();
            __android_log_print(ANDROID_LOG_INFO,"HelengineAndroid","AAudio output disconnected; reopening the default route when active.");
        } else if (error!=AAUDIO_OK) {
            CloseStream();
            Check(error,"AAudio asynchronous output failure");
        }
        if (!Paused && !Stream) OpenStream();
    }
    void AndroidAudioOutput::ReportError(AAudioStream*,void* userData,aaudio_result_t error) {
        auto& output=*static_cast<AndroidAudioOutput*>(userData);
        auto expected=output.Error.load(std::memory_order_relaxed);
        while ((expected==AAUDIO_OK || (expected==AAUDIO_ERROR_DISCONNECTED && error!=AAUDIO_ERROR_DISCONNECTED))
            && !output.Error.compare_exchange_weak(expected,error,std::memory_order_release,std::memory_order_relaxed)) {}
    }
}
