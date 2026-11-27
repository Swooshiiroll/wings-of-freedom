#pragma once

#include <array>
#include <cstdint>

namespace wof {

// How a plane's flight ended, or that it hasn't.
enum class Outcome : std::uint8_t {
    Flying,
    Crashed,      // hit the ground
    LeftArea,     // the edge countdown ran out: self-destructed
    ReturnedHome, // flew off the south edge
    ShotDown,     // lost a wing or the tail, or was destroyed in a collision
    PilotKilled,  // the third wound
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
    case Outcome::ShotDown:
        return "shot down";
    case Outcome::PilotKilled:
        return "pilot killed";
    }
    return "?";
}

// The parts of a plane (design/flight.md, Damage). Each takes hits, and each block of a plane's
// model belongs to one, so a plane can be drawn with a lost part missing. The cockpit is the
// pilot. Twin-engined planes (design/planes.md) have a second engine: Engine is the left one and
// Engine2 the right. A bomber's gunners take hits too; they're not parts of its model.
enum class Part : std::uint8_t {
    Engine,
    LeftWing,
    RightWing,
    Fuselage,
    Tail,
    Cockpit,
    Engine2,
    NoseGunner,
    DorsalGunner,
    VentralGunner
};
inline constexpr int kParts = 10;
inline constexpr std::uint32_t kAllParts = 0x7fu; // every part of a model
inline constexpr int kGunners = 3;                // nose, dorsal, ventral

[[nodiscard]] constexpr std::uint32_t part_bit(Part p) noexcept {
    return 1u << static_cast<std::uint32_t>(p);
}

// A plane's damage: what's left of each part, the pilot's wounds, and whether it's going down
// (out of control after losing a wing or the tail, or its pilot). The defaults are a new plane's
// (core/damage.hpp's DamageParams; repair() sets them from the tuning).
struct Damage {
    float engine = 40.0f;
    float engine2 = 0.0f; // twin-engined planes' right engine
    int engines = 1;
    float left_wing = 40.0f;
    float right_wing = 40.0f;
    float fuselage = 60.0f;
    float tail = 20.0f;
    float pilot = 0.0f;                   // damage taken by the pilot
    int wounds = 0;                       // every wound_damage of it is a wound
    std::array<float, kGunners> gunner{}; // damage each of a bomber's gunners has taken
    bool going_down = false;
    float spin = 1.0f;                 // going down: 1 spins right, -1 left
    Outcome cause = Outcome::ShotDown; // going down: how the flight will end

    friend constexpr bool operator==(const Damage&, const Damage&) = default;
};

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

// One gun's state: rounds left, time until it can fire again, and rounds fired (every few is a
// tracer).
struct GunState {
    int rounds = 0;
    float wait_s = 0.0f;
    int fired = 0;

    friend constexpr bool operator==(const GunState&, const GunState&) = default;
};
inline constexpr int kGunSlots = 4; // core/planes.hpp's kMaxGuns

// What a plane has left to fire, and its weapons' timing. Each gun fires at its own real rate
// (core/planes.hpp).
struct Loadout {
    std::array<GunState, kGunSlots> guns{};
    int bombs = 0;
    int bomb_cooldown = 0; // ticks until the next bomb can drop
    Heavy heavy = Heavy::Bombs;

    friend constexpr bool operator==(const Loadout&, const Loadout&) = default;
};

// One plane's state. x east, y north, z height, in u; angles in degrees.
struct Body {
    std::uint8_t type = 0; // which plane it is: core/planes.hpp's PlaneId
    std::int8_t fit = -1;  // its upgrades: Rules::fitted[fit], or the stock plane
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
    Damage damage;

    friend constexpr bool operator==(const Body&, const Body&) = default;
};

} // namespace wof
