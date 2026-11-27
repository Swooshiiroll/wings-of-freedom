#pragma once

#include "core/planes.hpp"

namespace wof {

struct World;
struct Rules;

// Enemy waves (design/flight.md, Waves): the first after first_s, then one every every_s, each
// bringing 1 + n/2 fighters in from the north, east or west edge near the player's height. At most
// max_alive are in the air; the rest of a wave waits for a place. Their fighters grow with the
// waves up the enemy's aircraft tree (design/aircraft_tree.md).

// The waves' numbers (tuning.txt `wave.*`), all (start) values.
struct WaveParams {
    float first_s = 10.0f;
    float every_s = 40.0f;
    int max_alive = 6;
    float height_spread_u = 60.0f; // arrive within this of the player's height (3,000 ft)
    float spread_u = 120.0f;       // along the edge, between the planes of a wave
    float along_u = 6000.0f;       // how far along the edge from abreast of you (or north)
    float edge_inset_u = 300.0f;   // how far inside the edge they appear
    int tier_every = 4;            // the enemy moves a stage up its tree every this many waves

    friend constexpr bool operator==(const WaveParams&, const WaveParams&) = default;
};

// The tree steps wave n's fighters are drawn from: the same twice, or two for a mix (each plane
// either, at random). With `every` 4: waves 1-4 step 1, 5-8 steps 1 and 2, 9-12 steps 2 and 3,
// 13 on step 3.
struct TierSpan {
    int lo = 0;
    int hi = 0;
};
[[nodiscard]] constexpr TierSpan enemy_steps(int n, int every) noexcept {
    const int stage = (n > 0 ? n - 1 : 0) / (every > 0 ? every : 1);
    const int last = kTreeSteps - 1;
    const int lo = stage <= 1 ? 0 : stage - 1;
    return {lo > last ? last : lo, stage > last ? last : stage};
}

// 1 + n/2 fighters, rounded down.
[[nodiscard]] constexpr int wave_size(int n) noexcept {
    return 1 + n / 2;
}

// Where the waves are up to.
struct WaveState {
    bool enabled = false; // off: no enemies (practice, tests)
    FighterTree enemy{PlaneId::Ar65E, PlaneId::He51B, PlaneId::Bf109B};
    int next = 1;         // the next wave's number
    float timer_s = 0.0f; // until it comes
    int current = 0;      // the last wave's number: its pilots' skill
    int waiting = 0;      // planes of the waves so far still to arrive
    int kills = 0;

    friend constexpr bool operator==(const WaveState&, const WaveState&) = default;
};

// Waves of fighters from the enemy's tree, starting at wave `first` after the first wave's delay.
[[nodiscard]] WaveState start_waves(const Rules& rules, const FighterTree& enemy,
                                    int first) noexcept;

// One tick: the clock, and any planes arriving (new bodies and their pilots in the world). An
// EventKind::WaveArrived marks each new wave.
void update_waves(World& w, const Rules& rules);

// Enemy fighters in the air and not going down.
[[nodiscard]] int enemies_alive(const World& w) noexcept;

} // namespace wof
