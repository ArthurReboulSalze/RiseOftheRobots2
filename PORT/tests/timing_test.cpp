#include "simulation_clock.h"
#include <cstdio>

static bool check(bool ok, const char* message) {
    if (!ok) std::fprintf(stderr, "%s\n", message);
    return ok;
}

int main() {
    // The same simulated second must survive different display refresh rates,
    // including a renderer slower than the DOS combat clock.
    for (int frames : {10, 30, 60, 144}) {
        SimulationClock clock;
        int ticks = 0;
        for (int frame = 0; frame < frames; ++frame) ticks += clock.advance(1.0 / frames);
        if (!check(ticks == 25, "Rendering rate changed combat speed")) return 1;
    }
    SimulationClock clock;
    int ticks = 0;
    for (double elapsed : {0.015, 0.025, 0.12, 0.005, 0.075, 0.2, 0.06})
        ticks += clock.advance(elapsed);
    if (!check(ticks == 12 && clock.advance(0.02) == 1,
               "Irregular frames must retain catch-up ticks and fractional time")) return 1;
    clock.reset();
    if (!check(clock.advance(0.12) == 3, "Three pending DOS updates must run before the next render")) return 1;
    clock.advance(0.03);
    if (!check(clock.advance(10.0, false) == 0 && clock.advance(0.01) == 0 && clock.advance(0.03) == 1,
               "Menus or pauses must not accumulate a burst on resume")) return 1;
    clock.reset();
    if (!check(clock.advance(10.0) == 6 && clock.advance(0.03) == 1,
               "Long stalls must be bounded without dropping the fractional remainder")) return 1;
    clock.reset();
    ticks = 0;
    for (int frame = 0; frame < 480; ++frame) ticks += clock.advance(1.0 / 60);
    if (!check(ticks == 200, "The 200-tick finishing window must last eight seconds")) return 1;
    std::puts("Timing: native rate, render independence, catch-up, pause and finishing window passed");
}
