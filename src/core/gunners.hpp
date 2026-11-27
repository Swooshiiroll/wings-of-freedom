#pragma once

#include <cstddef>

namespace wof {

struct World;
struct Rules;
struct Pilot;

// A bomber's gunners (design/flight.md, Bombers): nose, dorsal and ventral, each firing the real
// gun at its real rate in short bursts at any player within range in its arc, aiming as well as
// the bomber's pilot.

// The gunners' numbers (tuning.txt `gunner.*`), all (start) values.
struct GunnerParams {
    float range_u = 160.0f;
    float burst_s = 0.6f;          // fire this long
    float pause_s = 0.6f;          // then wait this long
    float nose_arc_deg = 60.0f;    // each side of straight ahead
    float dorsal_arc_deg = 120.0f; // each side of the tail, level or above
    float ventral_arc_deg = 60.0f; // each side of the tail, level or below
    float level_u = 5.0f;          // how far above or below still counts as level

    friend constexpr bool operator==(const GunnerParams&, const GunnerParams&) = default;
};

// One gunner's state between ticks.
struct GunnerState {
    float look_s = 0.0f; // until the next look
    float seen_age_s = 0.0f;
    float seen_x = 0.0f;
    float seen_y = 0.0f;
    float seen_z = 0.0f;
    float seen_vx = 0.0f;
    float seen_vy = 0.0f;
    float seen_vz = 0.0f;
    float off_deg = 0.0f; // the current aim error, across and up or down
    float off_pitch_deg = 0.0f;
    float burst_s = 0.0f; // left of the burst or the pause
    bool firing = false;

    friend constexpr bool operator==(const GunnerState&, const GunnerState&) = default;
};

// Whether a target at (dx, dy, dz) from a plane heading `heading_deg` is in gunner `gunner`'s arc
// (0 nose, 1 dorsal, 2 ventral) and range.
[[nodiscard]] bool in_gunner_arc(int gunner, float heading_deg, float dx, float dy, float dz,
                                 const GunnerParams& p) noexcept;

// One tick of a bomber's gunners: each one in action tracks the nearest player in its arc and
// fires into the world.
void fire_gunners(World& w, std::size_t body, Pilot& pilot, const Rules& rules);

} // namespace wof
