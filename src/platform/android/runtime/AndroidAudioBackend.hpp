#pragma once
#include "ISeekableAudioBackend.hpp"
#include "AndroidAudioVoice.hpp"
#include <aaudio/AAudio.h>
#include <array>
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace helengine::android {
    /// Mixes scene-owned PCM voices on a worker while AAudio consumes a lock-free output ring.
    class AndroidAudioBackend final : public ::ISeekableAudioBackend {
    public:
        /// Stops the worker and closes the output stream before freeing callback-visible memory.
        ~AndroidAudioBackend();
        /// Opens one stereo floating-point output stream and starts the non-real-time mixer.
        void Initialize();
        /// Pauses output and source cursors while the Android activity is suspended.
        void SetHostPaused(bool paused);
        /// Copies validated cooked PCM and begins playback at the requested source-time offset.
        int32_t Play(::AudioAsset* asset, ::AudioPlaybackRequest* request) override;
        /// Stops and releases one backend-owned voice.
        void Stop(int32_t voiceId) override;
        /// Changes a linear mixer bus gain, including the master gain.
        void SetBusGain(std::string busId,float gain) override;
        /// Pauses or resumes voices routed through one mixer bus.
        void SetBusPaused(std::string busId,bool paused) override;
        /// Returns whether a voice is active or bus-paused.
        bool IsPlaying(int32_t voiceId) override;
        /// Removes completed voices and reports asynchronous device failures on the engine thread.
        void Update() override;
    private:
        /// Writes queued float samples without allocation, locks, decoding or logging on the real-time callback thread.
        static aaudio_data_callback_result_t WriteAudio(AAudioStream* stream,void* userData,void* audioData,int32_t frames);
        /// Defers device error handling to the engine thread rather than closing from the AAudio callback.
        static void ReportError(AAudioStream* stream,void* userData,aaudio_result_t error);
        /// Produces bounded output chunks on the worker, retaining source cursors under the voice mutex.
        void Mix();
        /// Opens the requested AAudio stream and validates its negotiated format.
        void OpenStream();
        /// Checks one platform call and preserves the AAudio error text when it fails.
        static void Check(aaudio_result_t result,const char* operation);
        /// Number of stereo frames in the single-producer/single-consumer ring.
        static constexpr std::size_t RingFrames=8192;
        /// Interleaved stereo storage accessed through the published read/write counters.
        std::array<float,RingFrames*2> Ring{};
        /// Producer cursor published only after its sample writes complete.
        std::atomic<std::uint64_t> Written{0};
        /// Consumer cursor published only after its sample reads complete.
        std::atomic<std::uint64_t> Read{0};
        /// Requests worker shutdown before output-stream destruction.
        std::atomic<bool> Stopping{false};
        /// Activity pause state shared by the mixer and output callback.
        std::atomic<bool> HostPaused{false};
        /// Asynchronous AAudio error waiting for the engine thread.
        std::atomic<aaudio_result_t> Error{AAUDIO_OK};
        /// Output stream owned exclusively by this backend.
        AAudioStream* Stream=nullptr;
        /// Actual device sample rate used by the resampling cursor.
        int OutputRate=48000;
        /// Worker thread performs all mixing and source interpolation outside the callback.
        std::thread Worker;
        /// Serializes voice and bus mutation with the non-real-time mixer.
        std::mutex VoicesMutex;
        /// Active voices retained until stop or completion maintenance.
        std::unordered_map<int,AndroidAudioVoice> Voices;
        /// Authored linear gain per named bus.
        std::unordered_map<std::string,float> BusGains;
        /// Buses whose voice cursors are currently paused.
        std::unordered_set<std::string> PausedBuses;
        /// Monotonically increasing voice id independent of scene transitions.
        int NextVoice=1;
    };
}
