#pragma once
#include <aaudio/AAudio.h>
#include <atomic>

namespace helengine::android {
    /// Owns AAudio streams on the engine thread while callbacks only consume PCM or publish an error.
    class AndroidAudioOutput {
    public:
        /// Fixed client rate keeps queued mixer frames valid across output-device replacement.
        static constexpr int SampleRate=48000;
        /// Closes the last stream before the owner frees callback-visible mixer memory.
        ~AndroidAudioOutput();
        /// Registers the owner callback once and opens output only when the host is active.
        void Initialize(AAudioStream_dataCallback callback,void* userData);
        /// Releases output while paused and opens the current default route when resumed.
        void SetPaused(bool paused);
        /// Replaces a disconnected stream on the engine thread and preserves all other error failures.
        void Update();
        /// Indicates whether callbacks have been registered, independently of an attached audio device.
        bool IsInitialized() const { return Initialized; }
    private:
        /// Opens and validates a candidate before publishing it; a new disconnect is retried on the next update.
        void OpenStream();
        /// Stops and closes a retired stream, propagating errors after releasing its resources.
        void CloseStream();
        /// Publishes errors without allocating or closing in callbacks; a fatal error takes priority over a disconnect.
        static void ReportError(AAudioStream* stream,void* userData,aaudio_result_t error);
        /// Preserves AAudio's diagnostic text for a failed operation.
        static void Check(aaudio_result_t result,const char* operation);
        /// Owned handle accessed only by engine-thread lifecycle methods.
        AAudioStream* Stream=nullptr;
        /// Owner-provided callback consumes the persistent stereo ring without depending on stream lifetime.
        AAudioStream_dataCallback Callback=nullptr;
        /// Borrowed mixer remains alive until the stream is closed.
        void* CallbackContext=nullptr;
        /// First unhandled device error reported from AAudio's callback thread.
        std::atomic<aaudio_result_t> Error{AAUDIO_OK};
        /// Distinguishes suspended initialized playback from playback before backend initialization.
        bool Initialized=false;
        /// Starts suspended until Android has both a resumed activity and a drawable native window.
        bool Paused=true;
    };
}
