#pragma once
#include "FakeAAudioStream.hpp"
#include <vector>
/// Injects device failures while recording lifecycle effects around the production backend.
class FakeAudioApi {
public:
    /// Currently owned default output route.
    inline static AAudioStream* Live=nullptr;
    /// Number of attempted route openings.
    inline static int Opens=0;
    /// Number of released routes.
    inline static int Closes=0;
    /// Number of live configuration builders.
    inline static int Builders=0;
    /// One injected opening failure, consumed by the next attempt.
    inline static int NextOpenError=AAUDIO_OK;
    /// One injected starting failure, consumed by the next attempt.
    inline static int NextStartError=AAUDIO_OK;
    /// Optional negotiated rate mismatch.
    inline static int NegotiatedRate=48000;
    /// Optional asynchronous error emitted while a retired stream closes.
    inline static int ClosingError=AAUDIO_OK;
    /// Guards against forbidden lifecycle work on the error callback thread.
    inline static bool InErrorCallback=false;
    /// Runtime version selects the API-26/27 and API-28+ attribute paths.
    inline static int AndroidApi=35;
    /// Detects accidental dynamic attribute lookups on unsupported APIs.
    inline static int AttributeLookups=0;
    /// Resets injection and diagnostics after the previous route has been released.
    static void Reset();
    /// Runs the registered production PCM callback for an arbitrary burst size.
    static std::vector<float> Render(int frames);
    /// Sends a simulated platform error through the real callback, optionally without changing stream state.
    static void Report(int error,bool disconnected=true);
};
