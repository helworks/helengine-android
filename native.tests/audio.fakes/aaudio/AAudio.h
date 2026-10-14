#pragma once
#include <cstdint>
/// Opaque platform stream supplied by the deterministic native test harness.
struct AAudioStream;
/// Opaque platform builder supplied by the deterministic native test harness.
struct AAudioStreamBuilder;
using aaudio_result_t=int32_t;
using aaudio_data_callback_result_t=int32_t;
using aaudio_usage_t=int32_t;
using aaudio_content_type_t=int32_t;
constexpr int AAUDIO_USAGE_MEDIA=1, AAUDIO_USAGE_GAME=14, AAUDIO_CONTENT_TYPE_MUSIC=2;
using AAudioStream_dataCallback=aaudio_data_callback_result_t (*)(AAudioStream*,void*,void*,int32_t);
using AAudioStream_errorCallback=void (*)(AAudioStream*,void*,aaudio_result_t);
constexpr int AAUDIO_OK=0, AAUDIO_ERROR_DISCONNECTED=-899, AAUDIO_ERROR_INTERNAL=-896, AAUDIO_ERROR_UNAVAILABLE=-889;
constexpr int AAUDIO_DIRECTION_OUTPUT=0, AAUDIO_FORMAT_PCM_FLOAT=2, AAUDIO_SHARING_MODE_SHARED=1, AAUDIO_PERFORMANCE_MODE_NONE=10;
constexpr int AAUDIO_STREAM_STATE_STARTED=4, AAUDIO_STREAM_STATE_DISCONNECTED=13;
constexpr int AAUDIO_CALLBACK_RESULT_CONTINUE=0, AAUDIO_CALLBACK_RESULT_STOP=1;
/// Allocates the test stream configuration.
aaudio_result_t AAudio_createStreamBuilder(AAudioStreamBuilder** builder);
/// Releases the test stream configuration.
aaudio_result_t AAudioStreamBuilder_delete(AAudioStreamBuilder* builder);
/// Captures requested output direction.
void AAudioStreamBuilder_setDirection(AAudioStreamBuilder*,int);
/// Captures requested PCM format.
void AAudioStreamBuilder_setFormat(AAudioStreamBuilder*,int);
/// Captures requested channel count.
void AAudioStreamBuilder_setChannelCount(AAudioStreamBuilder*,int);
/// Captures requested client sample rate.
void AAudioStreamBuilder_setSampleRate(AAudioStreamBuilder*,int);
/// Captures requested sharing mode.
void AAudioStreamBuilder_setSharingMode(AAudioStreamBuilder*,int);
/// Captures requested performance mode.
void AAudioStreamBuilder_setPerformanceMode(AAudioStreamBuilder*,int);
/// Retains the real production mixer callback.
void AAudioStreamBuilder_setDataCallback(AAudioStreamBuilder*,AAudioStream_dataCallback,void*);
/// Retains the real production error callback.
void AAudioStreamBuilder_setErrorCallback(AAudioStreamBuilder*,AAudioStream_errorCallback,void*);
/// Opens the next deterministic test route or returns an injected failure.
aaudio_result_t AAudioStreamBuilder_openStream(AAudioStreamBuilder*,AAudioStream**);
/// Returns the test route's negotiated format.
int AAudioStream_getFormat(AAudioStream*);
/// Returns the test route's negotiated channel count.
int AAudioStream_getChannelCount(AAudioStream*);
/// Returns the test route's negotiated sample rate.
int AAudioStream_getSampleRate(AAudioStream*);
/// Returns whether the fake route was marked disconnected.
int AAudioStream_getState(AAudioStream*);
/// Starts the route or returns an injected failure.
aaudio_result_t AAudioStream_requestStart(AAudioStream*);
/// Stops the route while recording the call thread.
aaudio_result_t AAudioStream_requestStop(AAudioStream*);
/// Closes the route and can publish a final callback error before returning.
aaudio_result_t AAudioStream_close(AAudioStream*);
/// Supplies deterministic platform diagnostics for exception assertions.
const char* AAudio_convertResultToText(aaudio_result_t);
