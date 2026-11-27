#pragma once

#include "core/body.hpp"
#include "core/damage.hpp"
#include "core/planes.hpp"

namespace wof {

struct World;

// A run's points (design/aircraft_tree.md, Points): kills, chains, and what's banked when the
// player gets home.

// The points' numbers (tuning.txt `score.*`), all (start) values.
struct ScoreParams {
    int fighter_points = 100;
    int bomber_points = 250;
    float chain_window_s = 20.0f; // a kill within this of the last keeps the chain
    float chain_step = 0.5f;      // and raises the multiplier by this
    float chain_max = 4.0f;
    float condition_bonus = 0.5f; // getting home undamaged adds this share
    float wave_bonus = 0.1f;      // and each wave flown in, this share

    friend constexpr bool operator==(const ScoreParams&, const ScoreParams&) = default;
};

// One run's score so far.
struct RunScore {
    int fighters = 0; // shot down
    int bombers = 0;
    int points = 0;       // from kills, chains included
    int chain = 0;        // kills in the chain now
    float chain_s = 0.0f; // until the chain ends
    int waves = 0;        // waves that came while the player flew

    friend constexpr bool operator==(const RunScore&, const RunScore&) = default;
};

// The multiplier for the `chain`th kill of a chain: x1, x1.5, x2 ... up to chain_max.
[[nodiscard]] float chain_multiplier(int chain, const ScoreParams& p) noexcept;

// A kill of a plane in a role.
void score_kill(RunScore& s, Role role, const ScoreParams& p) noexcept;

// One tick, after the world's step: the chain's clock, new waves and the player's kills.
void score_tick(RunScore& s, const World& w, const ScoreParams& p, float dt) noexcept;

// How whole a plane is, 0 to 1: its parts' HP, less the share of the pilot's wounds.
[[nodiscard]] float condition_of(const Body& b, const DamageParams& d) noexcept;

// Home: flown off the south edge, not falling out of control.
[[nodiscard]] bool got_home(const Body& b) noexcept;

// What a finished run earns.
struct Payout {
    bool banked = false; // got home
    int points = 0;      // from kills
    float condition_share = 0.0f;
    float wave_share = 0.0f;
    int total = 0; // points with the bonuses; 0 if not banked
};
[[nodiscard]] Payout payout(const RunScore& s, const Body& b, const DamageParams& d,
                            const ScoreParams& p) noexcept;

} // namespace wof
