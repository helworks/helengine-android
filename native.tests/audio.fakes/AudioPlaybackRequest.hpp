#pragma once
#include <string>
/// Supplies the exact authored playback getters consumed by the production backend.
class AudioPlaybackRequest {
public:
    /// Linear authored voice volume.
    float Gain=1;
    /// Selects source wrapping independently of device reconnection.
    bool Loop=false;
    /// Named route whose gain and pause state belongs to gameplay.
    std::string Bus="music";
    /// Returns the authored voice volume.
    float get_Gain() const { return Gain; }
    /// Begins the ramp at its first source frame.
    double get_StartOffsetSeconds() const { return 0; }
    /// Returns the authored wrap policy.
    bool get_Loop() const { return Loop; }
    /// Returns the authored bus route.
    std::string get_BusId() const { return Bus; }
};
