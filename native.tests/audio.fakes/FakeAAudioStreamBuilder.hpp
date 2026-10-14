#pragma once
#include <aaudio/AAudio.h>
/// Retains requested format and callback ownership for each fake route.
struct AAudioStreamBuilder {
    /// Requested client format.
    int Format=0;
    /// Requested client channel layout.
    int Channels=0;
    /// Requested client sample rate.
    int Rate=0;
    /// Default media usage remains unchanged on Android API 26/27.
    int Usage=AAUDIO_USAGE_MEDIA;
    /// Content type supplied on Android API 28 and later.
    int ContentType=AAUDIO_CONTENT_TYPE_MUSIC;
    /// Production PCM consumer.
    AAudioStream_dataCallback DataCallback=nullptr;
    /// Borrowed production mixer.
    void* DataContext=nullptr;
    /// Production asynchronous error publisher.
    AAudioStream_errorCallback ErrorCallback=nullptr;
    /// Borrowed production stream owner.
    void* ErrorContext=nullptr;
};
