#include "../src/platform/android/runtime/AndroidAudioBackend.hpp"
#include "audio.fakes/FakeAudioApi.hpp"
#include <AudioAsset.hpp>
#include <AudioPlaybackRequest.hpp>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

/// Exercises the real PCM mixer and output owner with deterministic device failures, without Android or an emulator.
class AndroidAudioLifecycleTests {
public:
    /// Runs independent scenarios, checking for leaked streams and configuration builders between them.
    static void Run() {
        ColdStartupAndContinuity();
        ReconnectPreservesVoicesAndBuses();
        DisconnectedStateWithoutCallback();
        FatalErrorsRemainFatal();
        ClosingCallbacksAreRetired();
        InvalidNegotiationClosesCandidate();
        OpeningFailureReleasesBuilder();
        StartingFailureClosesCandidate();
        DisconnectDuringOpenIsDeferred();
        DisconnectDuringStartIsDeferred();
        SuspendedInitializationAllowsVoiceControl();
        FocusLossMutesBeforeMaintenance();
        HostPauseWinsOverFocusGain();
        FocusRequestAndOutputAttributesMatch();
        FakeAudioApi::Reset();
        std::cout << "Passed 14 Android audio lifecycle scenarios.\n";
    }
private:
    /// Reports an invariant without depending on a test framework or generated engine library.
    static void Require(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
    /// Confirms a platform failure remains visible to the engine thread.
    static void RequireThrows(const std::function<void()>& operation) {
        bool rejected=false;
        try { operation(); } catch (const std::runtime_error&) { rejected=true; }
        Require(rejected,"Expected an explicit audio output failure.");
    }
    /// Waits for the production worker to publish its first ramp sample, with a bounded desktop timeout.
    static float FirstSample() {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
        while (std::chrono::steady_clock::now()<deadline) {
            const auto frame=FakeAudioApi::Render(1);
            Require(frame[0]==frame[1],"Mono interpolation did not produce equal stereo channels.");
            if (frame[0]!=0) return frame[0];
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        throw std::runtime_error("Mixer did not publish a source sample.");
    }
    /// Initializes cold with no device, then verifies pause/resume preserves queued samples rather than restarting the voice.
    static void ColdStartupAndContinuity() {
        FakeAudioApi::Reset();
        helengine::android::AndroidAudioBackend backend;
        backend.Initialize(); backend.Initialize();
        AudioAsset asset; AudioPlaybackRequest request;
        const auto voice=backend.Play(&asset,&request);
        Require(FakeAudioApi::Opens==0,"Suspended initialization opened an audio device.");
        backend.SetHostPaused(false);
        Require(FirstSample()==1.0f/32768,"Cold startup skipped source frames while suspended.");
        const auto frames=FakeAudioApi::Render(31);
        Require(frames.back()==32.0f/32768,"Client-rate ramp changed before pausing.");
        backend.SetHostPaused(true);
        backend.SetHostPaused(true);
        backend.Update();
        Require(!FakeAudioApi::Live && FakeAudioApi::Closes==1 && backend.IsPlaying(voice),"Pause did not release the device while preserving the voice.");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        backend.SetHostPaused(false);
        backend.SetHostPaused(false);
        Require(FakeAudioApi::Opens==2,"Repeated resume recreated an already active stream.");
        Require(FirstSample()==33.0f/32768,"Pause/resume discarded queued PCM or reset its cursor.");
    }
    /// Preserves PCM continuity, loop state, voice identity and authored volume after the default device disconnects.
    static void ReconnectPreservesVoicesAndBuses() {
        FakeAudioApi::Reset();
        helengine::android::AndroidAudioBackend backend; backend.Initialize();
        AudioAsset asset; AudioPlaybackRequest request; request.Gain=0.5f; request.Loop=true;
        backend.SetBusGain("master",0.5f); backend.SetBusGain("music",0.25f);
        const auto voice=backend.Play(&asset,&request);
        backend.SetHostPaused(false);
        Require(FirstSample()==0.0625f/32768,"Authored voice and bus volumes were not applied.");
        FakeAudioApi::Report(AAUDIO_ERROR_DISCONNECTED);
        Require(FakeAudioApi::Closes==0,"Disconnect callback performed device lifecycle work.");
        backend.Update();
        Require(FakeAudioApi::Opens==2 && FakeAudioApi::Closes==1 && backend.IsPlaying(voice),"Device reconnect lost the voice or failed to replace the old route.");
        Require(FirstSample()==0.125f/32768,"Reconnect lost queued samples or authored gains.");
        backend.Stop(voice);
        Require(!backend.IsPlaying(voice),"Reconnected voice could no longer be stopped by its original ID.");
    }
    /// Detects a disconnected stream even when the engine has not yet received its error callback.
    static void DisconnectedStateWithoutCallback() {
        FakeAudioApi::Reset();
        helengine::android::AndroidAudioBackend backend; backend.Initialize(); backend.SetHostPaused(false);
        FakeAudioApi::Live->State=AAUDIO_STREAM_STATE_DISCONNECTED;
        backend.Update();
        Require(FakeAudioApi::Opens==2 && FakeAudioApi::Closes==1,"Disconnected platform state was ignored.");
    }
    /// A fatal asynchronous error takes priority over a preceding disconnect and never silently reopens output.
    static void FatalErrorsRemainFatal() {
        FakeAudioApi::Reset();
        helengine::android::AndroidAudioBackend backend; backend.Initialize(); backend.SetHostPaused(false);
        FakeAudioApi::Report(AAUDIO_ERROR_DISCONNECTED);
        FakeAudioApi::Report(AAUDIO_ERROR_INTERNAL);
        RequireThrows([&backend] { backend.Update(); });
        Require(FakeAudioApi::Opens==1 && FakeAudioApi::Closes==1 && !FakeAudioApi::Live,"Fatal error was hidden by reconnection.");
    }
    /// Retired stream callbacks cannot trigger another reconnection or open a device while the host is paused.
    static void ClosingCallbacksAreRetired() {
        FakeAudioApi::Reset();
        helengine::android::AndroidAudioBackend backend; backend.Initialize(); backend.SetHostPaused(false);
        FakeAudioApi::ClosingError=AAUDIO_ERROR_DISCONNECTED;
        backend.SetHostPaused(true); backend.Update();
        Require(FakeAudioApi::Opens==1 && !FakeAudioApi::Live,"Paused maintenance reopened a retired device.");
        backend.SetHostPaused(false); backend.Update();
        Require(FakeAudioApi::Opens==2 && FakeAudioApi::Closes==1,"Old callback error was applied to the replacement stream.");
    }
    /// Rejects a changed client rate so PCM queued for the prior route cannot be played at a different speed.
    static void InvalidNegotiationClosesCandidate() {
        FakeAudioApi::Reset(); FakeAudioApi::NegotiatedRate=44100;
        helengine::android::AndroidAudioBackend backend; backend.Initialize();
        RequireThrows([&backend] { backend.SetHostPaused(false); });
        Require(!FakeAudioApi::Live && FakeAudioApi::Closes==1 && FakeAudioApi::Builders==0,"Rejected negotiation leaked its candidate or builder.");
    }
    /// A nonrecoverable route-open failure remains visible and releases the stream builder.
    static void OpeningFailureReleasesBuilder() {
        FakeAudioApi::Reset(); FakeAudioApi::NextOpenError=AAUDIO_ERROR_UNAVAILABLE;
        helengine::android::AndroidAudioBackend backend; backend.Initialize();
        RequireThrows([&backend] { backend.SetHostPaused(false); });
        Require(!FakeAudioApi::Live && FakeAudioApi::Builders==0,"Opening failure leaked platform configuration.");
    }
    /// A failed start cannot leave a half-initialized output route attached to the mixer.
    static void StartingFailureClosesCandidate() {
        FakeAudioApi::Reset(); FakeAudioApi::NextStartError=AAUDIO_ERROR_INTERNAL;
        helengine::android::AndroidAudioBackend backend; backend.Initialize();
        RequireThrows([&backend] { backend.SetHostPaused(false); });
        Require(!FakeAudioApi::Live && FakeAudioApi::Closes==1,"Starting failure leaked its output candidate.");
    }
    /// A route that disconnects while being opened is retried once on the next maintenance frame, without a busy loop.
    static void DisconnectDuringOpenIsDeferred() {
        FakeAudioApi::Reset(); FakeAudioApi::NextOpenError=AAUDIO_ERROR_DISCONNECTED;
        helengine::android::AndroidAudioBackend backend; backend.Initialize(); backend.SetHostPaused(false);
        Require(FakeAudioApi::Opens==1 && !FakeAudioApi::Live,"Transient open retried inside one frame.");
        backend.Update();
        Require(FakeAudioApi::Opens==2 && FakeAudioApi::Live,"Transient route open was never retried.");
    }
    /// A start-time disconnect releases its candidate and defers the next attempt until maintenance.
    static void DisconnectDuringStartIsDeferred() {
        FakeAudioApi::Reset(); FakeAudioApi::NextStartError=AAUDIO_ERROR_DISCONNECTED;
        helengine::android::AndroidAudioBackend backend; backend.Initialize(); backend.SetHostPaused(false);
        Require(FakeAudioApi::Opens==1 && FakeAudioApi::Closes==1 && !FakeAudioApi::Live,"Start-time disconnect leaked its candidate or retried immediately.");
        backend.Update();
        Require(FakeAudioApi::Opens==2 && FakeAudioApi::Live,"Start-time disconnect was never retried.");
    }
    /// Gameplay may create, pause and stop voices while no native output device is attached.
    static void SuspendedInitializationAllowsVoiceControl() {
        FakeAudioApi::Reset();
        helengine::android::AndroidAudioBackend backend; backend.Initialize();
        AudioAsset asset; AudioPlaybackRequest request;
        const auto voice=backend.Play(&asset,&request);
        backend.SetBusPaused("music",true); backend.Update();
        Require(backend.IsPlaying(voice) && FakeAudioApi::Opens==0,"Suspended gameplay required an attached audio device.");
        backend.Stop(voice);
        Require(!backend.IsPlaying(voice),"Suspended gameplay could not stop a voice.");
    }
    /// Focus callbacks immediately silence output without performing stream lifecycle work or consuming queued PCM.
    static void FocusLossMutesBeforeMaintenance() {
        FakeAudioApi::Reset();
        helengine::android::AndroidAudioBackend backend; backend.Initialize();
        AudioAsset asset; AudioPlaybackRequest request;
        backend.Play(&asset,&request); backend.SetHostPaused(false);
        Require(FirstSample()==1.0f/32768,"Focus test did not begin at the first sample.");
        std::thread callback([&backend] { backend.SetFocusPaused(true); }); callback.join();
        const auto silence=FakeAudioApi::Render(127);
        for (float sample:silence) Require(sample==0,"Focus loss left audible PCM before the next engine frame.");
        Require(FakeAudioApi::Closes==0,"Focus callback closed output outside engine maintenance.");
        backend.Update();
        Require(!FakeAudioApi::Live && FakeAudioApi::Closes==1,"Engine maintenance did not release unfocused output.");
        backend.SetFocusPaused(false);
        Require(FakeAudioApi::Opens==1,"Focus gain opened output from the callback thread.");
        backend.Update();
        Require(FirstSample()==2.0f/32768,"Focus interruption consumed or discarded queued PCM.");
    }
    /// A late focus gain may change permission but cannot reopen output while the activity remains suspended.
    static void HostPauseWinsOverFocusGain() {
        FakeAudioApi::Reset();
        helengine::android::AndroidAudioBackend backend; backend.Initialize(); backend.SetHostPaused(false);
        backend.SetFocusPaused(true); backend.SetHostPaused(true);
        backend.SetFocusPaused(false); backend.Update();
        Require(!FakeAudioApi::Live && FakeAudioApi::Opens==1,"Focus gain overrode host suspension.");
        backend.SetFocusPaused(true); backend.SetHostPaused(false); backend.Update();
        Require(!FakeAudioApi::Live && FakeAudioApi::Opens==1,"Host resume overrode missing audio focus.");
        backend.SetFocusPaused(false); backend.Update();
        Require(FakeAudioApi::Live && FakeAudioApi::Opens==2,"Combined active host and granted focus did not resume output.");
    }
    /// The client usage matches the Java focus request without requiring API-28 symbols on the minimum API.
    static void FocusRequestAndOutputAttributesMatch() {
        FakeAudioApi::Reset();
        {
            helengine::android::AndroidAudioBackend backend; backend.Initialize(); backend.SetHostPaused(false);
            Require(FakeAudioApi::Live->Configuration.Usage==AAUDIO_USAGE_GAME && FakeAudioApi::Live->Configuration.ContentType==AAUDIO_CONTENT_TYPE_MUSIC,"Modern client attributes did not match game focus.");
        }
        FakeAudioApi::Reset(); FakeAudioApi::AndroidApi=26;
        {
            helengine::android::AndroidAudioBackend backend; backend.Initialize(); backend.SetHostPaused(false);
            Require(FakeAudioApi::Live->Configuration.Usage==AAUDIO_USAGE_MEDIA && FakeAudioApi::Live->Configuration.ContentType==AAUDIO_CONTENT_TYPE_MUSIC
                && FakeAudioApi::AttributeLookups==0,"Minimum API path imported unsupported game attributes.");
        }
    }
};

/// Reports native test results with a nonzero process status on failure.
int main() {
    try { AndroidAudioLifecycleTests::Run(); return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
