#include "../src/platform/android/rendering/FrameRateMath.hpp"
#include <iostream>
#include <limits>

using helengine::android::FrameRateMath;

/// Verifies explicit cadence, display-rate mode and invalid values without an Android device.
int main() {
    try {
        if (FrameRateMath::IntervalNanoseconds(0,16666667)!=16666667 || FrameRateMath::IntervalNanoseconds(0,8333333)!=8333333) throw std::runtime_error("Display-rate mode must use a real reported refresh period.");
        if (FrameRateMath::IntervalNanoseconds(1,0)!=1000000000) throw std::runtime_error("One FPS must retain a full second.");
        if (FrameRateMath::IntervalNanoseconds(30,0)!=33333333 || FrameRateMath::IntervalNanoseconds(60,0)!=16666667) throw std::runtime_error("30/60 FPS cadence differs from Swappy's standard intervals.");
        if (FrameRateMath::IntervalNanoseconds(90,0)!=11111111 || FrameRateMath::IntervalNanoseconds(120,0)!=8333333 || FrameRateMath::IntervalNanoseconds(240,0)!=4166667) throw std::runtime_error("High-refresh targets lost nanosecond precision.");
        for (double rate:{-1.0,240.5,241.0,59.5,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            bool rejected=false;
            try { FrameRateMath::IntervalNanoseconds(rate,16666667); } catch (const std::invalid_argument&) { rejected=true; }
            if (!rejected) throw std::runtime_error("Invalid FPS request was accepted.");
        }
        bool missingDisplayRejected=false;
        try { FrameRateMath::IntervalNanoseconds(0,0); } catch (const std::invalid_argument&) { missingDisplayRejected=true; }
        if (!missingDisplayRejected) throw std::runtime_error("Display-rate mode cannot invent missing display timing.");
        std::cout << "Passed 6 frame-rate contract checks.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
