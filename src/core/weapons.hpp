#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "core/body.hpp"
#include "core/input.hpp"

namespace wof {

struct World;
struct Rules;

// How guns judge height against other planes (design/flight.md, Weapons): a game option.
enum class HeightRule : std::uint8_t {
    Forgiving, // a round hits a plane within ±band_forgiving_u of its height (the default)
    Strict,    // within ±band_strict_u
    AutoAim,   // rounds angle toward a target near the nose and near your height
};
inline constexpr int kHeightRules = 3;

[[nodiscard]] const char* height_rule_name(HeightRule r) noexcept;
bool parse_height_rule(std::string_view text, HeightRule& out) noexcept;

// The weapons' numbers (tuning.txt, `weapon.*`), all (start) values.
struct WeaponParams {
    float gun_rate_per_s = 12.0f;
    int rounds = 600;
    float round_speed_u_s = 360.0f; // on top of the plane's own speed
    float round_life_s = 0.8f;
    int round_damage = 2;         // owner, 2026-11-26: "parts matter"
    int tracer_every = 3;         // one round in this many is a tracer
    float gun_spacing_u = 1.0f;   // two guns, this far either side of the nose
    float muzzle_u = 9.0f;        // rounds start this far ahead of the plane's centre
    float hit_radius_u = 8.0f;    // a plane's size, for rounds
    float strafe_radius_u = 6.0f; // rounds landing this close to a ground target's edge hit it
    float band_forgiving_u = 10.0f;
    float band_strict_u = 3.0f;
    float autoaim_cone_deg = 12.0f;
    float autoaim_height_u = 40.0f;
    int bombs = 4;
    float bomb_interval_s = 0.3f; // holding Space drops one this often
    float bomb_drop_u = 2.5f;     // bombs leave this far below the plane
    float blast_radius_u = 20.0f;
    float blast_damage = 40.0f; // at the centre, falling to 0 at the edge

    friend constexpr bool operator==(const WeaponParams&, const WeaponParams&) = default;
};

// A round in flight.
struct Round {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
    float life_s = 0.0f;
    int owner = -1; // the body that fired it
    bool tracer = false;

    friend constexpr bool operator==(const Round&, const Round&) = default;
};

// A bomb falling.
struct Bomb {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
    int owner = -1;

    friend constexpr bool operator==(const Bomb&, const Bomb&) = default;
};

// Something that happened this tick, for the front end's effects and for damage (PR 4).
enum class EventKind : std::uint8_t {
    RoundHitPlane,  // who: the body hit; by: the shooter
    RoundHitTarget, // who: the target hit
    RoundGround,    // a round reached the ground at (x, y)
    Blast,          // a bomb burst at (x, y)
    BlastHitPlane,  // who: the body caught; damage: how much
    BlastHitTarget, // who: the target caught
    Collision,      // who: the plane destroyed (-1: both); by: the other
};

struct Event {
    EventKind kind = EventKind::Blast;
    int who = -1;
    int by = -1;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float damage = 0.0f;
    std::int8_t part = -1; // a hit on a plane: the Part it landed on

    friend constexpr bool operator==(const Event&, const Event&) = default;
};

// Fills a plane's guns and bomb racks.
void arm(Body& b, const WeaponParams& p) noexcept;

// The blast's damage at a distance from the burst: full at the centre, 0 at the edge and beyond.
[[nodiscard]] float blast_damage_at(const WeaponParams& p, float distance) noexcept;

// Where a bomb released now would land (the bomb marker), worked out with the same steps the
// simulation uses, so it's exact.
struct GroundSpot {
    float x = 0.0f;
    float y = 0.0f;
};
[[nodiscard]] GroundSpot bomb_impact(const Body& b, const WeaponParams& p, float gravity) noexcept;

// One tick of a body's weapons: guns while held, bombs while held (one every bomb_interval_s), and
// switching the heavy weapon. New rounds and bombs go into the world.
void fire_weapons(World& w, std::size_t body, const InputCommand& cmd, const Rules& rules);

// One tick of every round and bomb in the world: movement, hits, ground impacts and blasts, each
// recorded as an event.
void move_projectiles(World& w, const Rules& rules);

} // namespace wof
