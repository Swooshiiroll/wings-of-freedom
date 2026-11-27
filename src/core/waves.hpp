#pragma once

#include "core/planes.hpp"

namespace wof {

struct World;
struct Rules;

// Enemy waves (design/flight.md, Waves): the first after first_s, then one every every_s, each
// bringing 1 + n/2 fighters from the north edge near the player's height. At most max_alive are in
// the air; the rest of a wave waits for a place.

// The waves' numbers (tuning.txt `wave.*`), all (start) values.
struct WaveParams {
    float first_s = 10.0f;
    float every_s = 40.0f;
    int max_alive = 6;
    float height_spread_u = 60.0f; // arrive within this of the player's height (3,000 ft)
    float spread_u = 120.0f;       // across, between the planes of a wave
    float north_inset_u = 60.0f;   // how far inside the north edge they appear

    friend constexpr bool operator==(const WaveParams&, const WaveParams&) = default;
};

// 1 + n/2 fighters, rounded down.
[[nodiscard]] constexpr int wave_size(int n) noexcept {
    return 1 + n / 2;
}

// Where the waves are up to.
struct WaveState {
    bool enabled = false; // off: no enemies (practice, tests)
    PlaneId enemy = PlaneId::He51B;
    int next = 1;         // the next wave's number
    float timer_s = 0.0f; // until it comes
    int current = 0;      // the last wave's number: its pilots' skill
    int waiting = 0;      // planes of the waves so far still to arrive
    int kills = 0;

    friend constexpr bool operator==(const WaveState&, const WaveState&) = default;
};

// Waves of `enemy` fighters, starting at wave `first` after the first wave's delay.
[[nodiscard]] WaveState start_waves(const Rules& rules, PlaneId enemy, int first) noexcept;

// One tick: the clock, and any planes arriving (new bodies and their pilots in the world). An
// EventKind::WaveArrived marks each new wave.
void update_waves(World& w, const Rules& rules);

// Enemy fighters in the air and not going down.
[[nodiscard]] int enemies_alive(const World& w) noexcept;

} // namespace wof
