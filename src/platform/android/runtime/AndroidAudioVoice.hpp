#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace helengine::android {
    /// Owns a PCM voice independently of scene asset lifetime while the mixer consumes its samples.
    struct AndroidAudioVoice {
        /// Little-endian signed sixteen-bit interleaved source samples.
        std::vector<std::int16_t> Samples;
        /// Source channel count, restricted to mono or stereo.
        int Channels=0;
        /// Source frames per second before output-device resampling.
        int SampleRate=0;
        /// Fractional source-frame cursor used for linear interpolation.
        double Position=0;
        /// Mixer bus routing and voice gain specified by the engine.
        std::string Bus;
        /// Linear voice gain before master and bus volume.
        float Gain=1;
        /// Whether the cursor wraps to the beginning after the final frame.
        bool Loop=false;
        /// Remains false once a nonlooping cursor reaches the end of its samples.
        bool Playing=true;
    };
}
