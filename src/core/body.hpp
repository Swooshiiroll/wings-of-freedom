#pragma once

#include <cstdint>

namespace wof {

// How a plane's flight ended, or that it hasn't.
enum class Outcome : std::uint8_t {
    Flying,
    Crashed,      // hit the ground
    LeftArea,     // the edge countdown ran out: self-destructed
    ReturnedHome, // flew off the south edge
};

[[nodiscard]] constexpr const char* outcome_name(Outcome o) noexcept {
    switch (o) {
    case Outcome::Flying:
        return "flying";
    case Outcome::Crashed:
        return "crashed";
    case Outcome::LeftArea:
        return "left the battle area";
    case Outcome::ReturnedHome:
        return "returned to base";
    }
    return "?";
}

// The heavy weapons a plane can carry (design/flight.md, Weapons). Rockets and missiles come with
// later planes.
enum class Heavy : std::uint8_t { Bombs };

[[nodiscard]] constexpr const char* heavy_name(Heavy h) noexcept {
    switch (h) {
    case Heavy::Bombs:
        return "BOMBS";
    }
    return "?";
}

// What a plane has left to fire, and its weapons' timing (in ticks, so firing is exact).
struct Loadout {
    int rounds = 0;
    int bombs = 0;
    int gun_cooldown = 0;  // ticks until the guns can fire again
    int bomb_cooldown = 0; // ticks until the next bomb can drop
    int rounds_fired = 0;  // the guns take turns, and every few rounds is a tracer
    Heavy heavy = Heavy::Bombs;

    friend constexpr bool operator==(const Loadout&, const Loadout&) = default;
};

// One plane's state. x east, y north, z height, in u; angles in degrees.
struct Body {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float heading_deg = 0.0f; // compass: 0 north, 90 east, always in [0, 360)
    float pitch_deg = 0.0f;   // nose up is positive
    float bank_deg = 0.0f;    // right wing down is positive (looks only)
    float speed_u_s = 0.0f;   // airspeed, along the flight path
    float throttle = 0.0f;    // 0..1
    float fuel = 1.0f;        // share of a full tank
    bool engine_on = true;
    bool stalled = false;
    bool edge_warning = false;      // within the warning distance of a soft edge
    float edge_countdown_s = -1.0f; // past a soft edge: time left; negative when not running
    Outcome outcome = Outcome::Flying;
    Loadout loadout;

    friend constexpr bool operator==(const Body&, const Body&) = default;
};

} // namespace wof
