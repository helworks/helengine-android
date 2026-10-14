#pragma once
#include <cstdint>
#include <vector>
/// Minimal encoded-PCM contract required to exercise the production native backend.
struct FakeAudioBytes {
    /// Source bytes populated by the test asset.
    std::vector<std::uint8_t> Data;
    /// Mirrors the generated array's portable length property.
    int Length=0;
    /// Returns one encoded sample byte.
    std::uint8_t operator[](std::size_t index) const { return Data.at(index); }
};
