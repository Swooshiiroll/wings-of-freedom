#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "core/body.hpp"
#include "core/flight.hpp"
#include "core/input.hpp"

namespace wof {

inline constexpr int kTicksPerSecond = 60;
inline constexpr float kTickSeconds = 1.0f / static_cast<float>(kTicksPerSecond);

// The battle area's edges (design/flight.md, World and view; tuning.txt `edge.*`).
struct EdgeParams {
    float warning_u = 200.0f;  // a warning shows this close to the east, west or north edge
    float countdown_s = 10.0f; // past one of them, this long to fly back in

    friend constexpr bool operator==(const EdgeParams&, const EdgeParams&) = default;
};

// Everything step() needs to know about the rules.
struct Rules {
    FlightParams flight;
    EdgeParams edge;

    friend constexpr bool operator==(const Rules&, const Rules&) = default;
};

// Everything that flies. Any number of planes; nothing here assumes there is only one player.
struct World {
    std::vector<Body> bodies;
    std::int64_t tick = 0;
};

// The ground and the edges, after a plane has moved:
// - touching the ground is a crash;
// - crossing the south edge is a return to base;
// - near the east, west or north edge a warning shows, and past one a countdown runs: flying
//   back in stops it (it starts from the top next time), and at 0 the plane self-destructs.
void apply_edges(Body& b, const EdgeParams& e, float dt) noexcept;

// Advances the world one tick (1/60 s). commands[i] drives bodies[i]; a body without a command gets
// no input. Bodies whose flight has ended stay where they are.
void step(World& world, std::span<const InputCommand> commands, const Rules& rules);

} // namespace wof
