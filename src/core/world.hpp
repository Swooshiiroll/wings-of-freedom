#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "core/input.hpp"

namespace wof {

inline constexpr int kTicksPerSecond = 60;
inline constexpr float kTickSeconds = 1.0f / static_cast<float>(kTicksPerSecond);

// The stand-in plane's numbers (tuning.txt, `stand_in.*`). PR 1 only: it gives the camera something
// to follow and has no physics. Real flight (design/flight.md, Flight) replaces it in PR 2.
struct StandInParams {
    float speed_u_s = 110.0f;  // fixed speed
    float turn_deg_s = 100.0f; // at full turn input
    float climb_u_s = 18.0f;   // at full pitch input, up or down
    float ceiling_u = 240.0f;  // the highest it can go

    friend constexpr bool operator==(const StandInParams&, const StandInParams&) = default;
};

// A plane's position and direction. x east, y north, z height, in u; the heading is a compass angle
// in degrees (0 north, 90 east), always in [0, 360).
struct Body {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float heading_deg = 0.0f;
    float speed_u_s = 0.0f;

    friend constexpr bool operator==(const Body&, const Body&) = default;
};

// Everything that flies. Any number of planes; nothing here assumes there is only one player.
struct World {
    std::vector<Body> bodies;
    std::int64_t tick = 0;
};

// Advances the world one tick (1/60 s). commands[i] drives bodies[i]; a body without a command gets
// no input that tick.
void step(World& world, std::span<const InputCommand> commands, const StandInParams& p);

} // namespace wof
