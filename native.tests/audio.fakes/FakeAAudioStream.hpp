#pragma once
#include "FakeAAudioStreamBuilder.hpp"
/// Retains a negotiated route and the production callbacks exercised by desktop tests.
struct AAudioStream {
    /// Immutable client format and callbacks copied from the builder.
    AAudioStreamBuilder Configuration;
    /// Mutable platform state used to simulate unplugging a device.
    int State=0;
};
