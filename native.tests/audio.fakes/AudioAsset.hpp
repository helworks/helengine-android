#pragma once
#include "FakeAudioBytes.hpp"
#include <string>
/// Supplies a monotonic PCM16 ramp to observe audible cursor continuity through the real mixer.
class AudioAsset {
public:
    /// Creates ten thousand valid positive mono sample frames.
    AudioAsset() {
        for (int sample=1;sample<=10000;++sample) { Bytes.Data.push_back(static_cast<std::uint8_t>(sample&255)); Bytes.Data.push_back(static_cast<std::uint8_t>(sample>>8)); }
        Bytes.Length=static_cast<int>(Bytes.Data.size());
    }
    /// Provides mono source layout.
    int get_Channels() const { return 1; }
    /// Uses the same client rate to make individual samples distinguishable.
    int get_SampleRate() const { return 48000; }
    /// Borrows the test's encoded source bytes.
    FakeAudioBytes* get_EncodedBytes() { return &Bytes; }
    /// Identifies the ramp in production diagnostics.
    std::string get_Id() const { return "ramp"; }
private:
    /// Owns the encoded PCM source used by Play's normal copy path.
    FakeAudioBytes Bytes;
};
