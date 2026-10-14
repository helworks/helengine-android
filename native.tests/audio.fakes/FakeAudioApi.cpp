#include "FakeAudioApi.hpp"
#include <stdexcept>
#include <cstring>

void FakeAudioApi::Reset() {
    if (Live || Builders!=0) throw std::runtime_error("Previous test leaked an AAudio route or builder.");
    Opens=0; Closes=0; NextOpenError=AAUDIO_OK; NextStartError=AAUDIO_OK; NegotiatedRate=48000; ClosingError=AAUDIO_OK;
    AndroidApi=35; AttributeLookups=0;
}
std::vector<float> FakeAudioApi::Render(int frames) {
    if (!Live) throw std::runtime_error("Test attempted to render a closed route.");
    std::vector<float> output(static_cast<std::size_t>(frames)*2);
    Live->Configuration.DataCallback(Live,Live->Configuration.DataContext,output.data(),frames);
    return output;
}
void FakeAudioApi::Report(int error,bool disconnected) {
    if (!Live) throw std::runtime_error("Test attempted to report an error on a closed route.");
    if (disconnected) Live->State=AAUDIO_STREAM_STATE_DISCONNECTED;
    InErrorCallback=true;
    Live->Configuration.ErrorCallback(Live,Live->Configuration.ErrorContext,error);
    InErrorCallback=false;
}
aaudio_result_t AAudio_createStreamBuilder(AAudioStreamBuilder** builder) { ++FakeAudioApi::Builders; *builder=new AAudioStreamBuilder(); return AAUDIO_OK; }
aaudio_result_t AAudioStreamBuilder_delete(AAudioStreamBuilder* builder) { --FakeAudioApi::Builders; delete builder; return AAUDIO_OK; }
void AAudioStreamBuilder_setDirection(AAudioStreamBuilder*,int) {}
void AAudioStreamBuilder_setFormat(AAudioStreamBuilder* builder,int value) { builder->Format=value; }
void AAudioStreamBuilder_setChannelCount(AAudioStreamBuilder* builder,int value) { builder->Channels=value; }
void AAudioStreamBuilder_setSampleRate(AAudioStreamBuilder* builder,int value) { builder->Rate=value; }
void AAudioStreamBuilder_setSharingMode(AAudioStreamBuilder*,int) {}
void AAudioStreamBuilder_setPerformanceMode(AAudioStreamBuilder*,int) {}
void AAudioStreamBuilder_setDataCallback(AAudioStreamBuilder* builder,AAudioStream_dataCallback callback,void* context) { builder->DataCallback=callback; builder->DataContext=context; }
void AAudioStreamBuilder_setErrorCallback(AAudioStreamBuilder* builder,AAudioStream_errorCallback callback,void* context) { builder->ErrorCallback=callback; builder->ErrorContext=context; }
aaudio_result_t AAudioStreamBuilder_openStream(AAudioStreamBuilder* builder,AAudioStream** stream) {
    ++FakeAudioApi::Opens;
    const int error=FakeAudioApi::NextOpenError; FakeAudioApi::NextOpenError=AAUDIO_OK;
    if (error!=AAUDIO_OK) { *stream=nullptr; return error; }
    if (FakeAudioApi::Live) throw std::runtime_error("Replacement opened before the old stream was closed.");
    auto* opened=new AAudioStream(); opened->Configuration=*builder; opened->Configuration.Rate=FakeAudioApi::NegotiatedRate;
    FakeAudioApi::Live=opened; *stream=opened; return AAUDIO_OK;
}
int AAudioStream_getFormat(AAudioStream* stream) { return stream->Configuration.Format; }
int AAudioStream_getChannelCount(AAudioStream* stream) { return stream->Configuration.Channels; }
int AAudioStream_getSampleRate(AAudioStream* stream) { return stream->Configuration.Rate; }
int AAudioStream_getState(AAudioStream* stream) { return stream->State; }
aaudio_result_t AAudioStream_requestStart(AAudioStream* stream) {
    const int error=FakeAudioApi::NextStartError; FakeAudioApi::NextStartError=AAUDIO_OK;
    if (error==AAUDIO_OK) stream->State=AAUDIO_STREAM_STATE_STARTED;
    return error;
}
aaudio_result_t AAudioStream_requestStop(AAudioStream* stream) {
    if (FakeAudioApi::InErrorCallback) throw std::runtime_error("Stream stopped from an error callback.");
    return stream->State==AAUDIO_STREAM_STATE_DISCONNECTED?AAUDIO_ERROR_DISCONNECTED:AAUDIO_OK;
}
aaudio_result_t AAudioStream_close(AAudioStream* stream) {
    if (FakeAudioApi::InErrorCallback) throw std::runtime_error("Stream closed from an error callback.");
    if (FakeAudioApi::ClosingError!=AAUDIO_OK) { const auto error=FakeAudioApi::ClosingError; FakeAudioApi::ClosingError=AAUDIO_OK; FakeAudioApi::Report(error); }
    ++FakeAudioApi::Closes; FakeAudioApi::Live=nullptr; delete stream; return AAUDIO_OK;
}
const char* AAudio_convertResultToText(aaudio_result_t error) { return error==AAUDIO_ERROR_DISCONNECTED?"disconnected":"injected platform failure"; }

/// Captures the dynamically resolved game usage without importing a newer Android symbol into the APK.
static void SetUsage(AAudioStreamBuilder* builder,aaudio_usage_t usage) { builder->Usage=usage; }
/// Captures the dynamically resolved music content attribute.
static void SetContent(AAudioStreamBuilder* builder,aaudio_content_type_t content) { builder->ContentType=content; }
int android_get_device_api_level() { return FakeAudioApi::AndroidApi; }
void* dlsym(void*,const char* name) {
    ++FakeAudioApi::AttributeLookups;
    if (std::strcmp(name,"AAudioStreamBuilder_setUsage")==0) return reinterpret_cast<void*>(&SetUsage);
    if (std::strcmp(name,"AAudioStreamBuilder_setContentType")==0) return reinterpret_cast<void*>(&SetContent);
    return nullptr;
}
