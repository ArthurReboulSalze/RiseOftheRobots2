#pragma once
#include <algorithm>
#include <cmath>

// DOS: 100 Hz IRQ -> 25 Hz counter -> one combat update per pending tick.
// Rendering and movie clocks are independent of this simulation clock.
class SimulationClock {
public:
    static constexpr int frequency = 25;
    static constexpr double tick_seconds = 1.0 / frequency;

    int advance(double elapsed, bool active = true) {
        if (!active) { reset(); return 0; }
        // Bound stalls (e.g. dragging the window), retaining normal catch-up.
        pending_ += std::clamp(elapsed, 0.0, 0.25);
        const int ticks = static_cast<int>(std::floor((pending_ + 1e-9) / tick_seconds));
        pending_ = std::max(0.0, pending_ - ticks * tick_seconds);
        return ticks;
    }

    void reset() { pending_ = 0.0; }

private:
    double pending_ = 0.0;
};
