#pragma once
#include "InputGamepadButton.hpp"
#include <cstdint>
/// Minimal engine ABI used to compile and execute the actual native gamepad adapter on Windows.
class InputGamepadState {
public:
    /// Published device availability.
    bool Connected=false;
    /// Portable button mask.
    uint64_t Buttons=0;
    /// Signed normalized stick axes.
    int16_t LeftStickX=0,LeftStickY=0,RightStickX=0,RightStickY=0;
    /// Nonnegative normalized analog triggers.
    int16_t LeftTrigger=0,RightTrigger=0;
    /// Checks the portable bit assigned to a button.
    bool IsButtonDown(InputGamepadButton button) { return (Buttons&(uint64_t{1}<<static_cast<int>(button)))!=0; }
    /// Updates a portable button without disturbing others.
    void SetButtonDown(InputGamepadButton button,bool down) {
        const auto mask=uint64_t{1}<<static_cast<int>(button);
        if (down) Buttons|=mask; else Buttons&=~mask;
    }
};
