#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "core/body.hpp"
#include "core/input.hpp"
#include "core/plane_rules.hpp"
#include "core/planes.hpp"

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

// What a round of each calibre does (design/planes.md, Guns by calibre). Damage keeps the real
// calibres' proportions; speed and life are game values (real muzzle velocities would make rounds
// near instant).
struct CalibreParams {
    float damage = 1.0f;
    float speed_u_s = 360.0f; // on top of the plane's own speed
    float life_s = 0.8f;

    friend constexpr bool operator==(const CalibreParams&, const CalibreParams&) = default;
};

// The weapons' numbers that aren't a plane's own (tuning.txt `weapon.*`, `calibre.*`), all (start)
// values. Each plane's guns and bombs are its real ones (core/planes.hpp).
struct WeaponParams {
    // Rifle calibre (.30 / 7.92 mm), heavy machine gun (.50 / 12.7 mm), 20 mm cannon.
    std::array<CalibreParams, 3> calibres{{
        {1.0f, 360.0f, 0.8f},
        {2.5f, 380.0f, 0.9f},
        {8.0f, 340.0f, 0.8f},
    }};
    int tracer_every = 3;         // one round in this many, per gun, is a tracer
    float muzzle_u = 9.0f;        // rounds start this far ahead of the plane's centre
    float hit_radius_u = 8.0f;    // a plane's size, for rounds
    float strafe_radius_u = 6.0f; // rounds landing this close to a ground target's edge hit it
    float band_forgiving_u = 10.0f;
    float band_strict_u = 3.0f;
    float autoaim_cone_deg = 12.0f;
    float autoaim_height_u = 40.0f;
    float bomb_interval_s = 0.3f; // holding Space drops one this often
    float bomb_drop_u = 2.5f;     // bombs leave this far below the plane
    // A 100 lb bomb's blast; other weights scale with the cube root of the weight.
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
    float damage = 1.0f; // by its calibre

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
    float radius_u = 20.0f; // its blast, by its weight
    float damage = 40.0f;

    friend constexpr bool operator==(const Bomb&, const Bomb&) = default;
};

// A bomb's blast by its weight: the 100 lb blast scaled by the cube root of the weight.
struct Blast {
    float radius_u = 0.0f;
    float damage = 0.0f;
};
[[nodiscard]] Blast blast_of(float weight_lb, const WeaponParams& p) noexcept;

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

// Fills a plane's guns and bomb racks with its real loads.
void arm(Body& b, const PlaneRules& plane) noexcept;

// A blast's damage at a distance from the burst: full at the centre, 0 at the edge and beyond.
[[nodiscard]] float blast_damage_at(const Blast& blast, float distance) noexcept;

// Rounds left in a plane's guns of one calibre.
[[nodiscard]] int rounds_left(const Body& b, const PlaneRules& plane, Calibre c) noexcept;

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
