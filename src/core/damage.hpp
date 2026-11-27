#pragma once

#include <array>
#include <cstdint>

#include "core/body.hpp"
#include "core/rng.hpp"

namespace wof {

// The damage model (design/flight.md, Damage): parts with HP, where hits land, the gradual
// effects on flying, the stepped stages the HUD shows, and what breaks.

// Where a hit came from, relative to the plane's nose. Blasts (bombs, flak) are Burst.
enum class HitFrom : std::uint8_t { Ahead, Behind, Left, Right, Burst };

// The stage a part is in, as the HUD shows it.
enum class Stage : std::uint8_t { Ok, Warning, Danger, Critical, Destroyed };

[[nodiscard]] const char* stage_name(Stage s) noexcept; // OK, WARN, DNGR, CRIT, GONE

// The damage numbers (tuning.txt, `damage.*` and `hit.*`), all (start) values.
struct DamageParams {
    int engines = 1;         // 2 for twin-engined planes (design/planes.md)
    float engine_hp = 40.0f; // each
    float wing_hp = 40.0f;   // each
    float fuselage_hp = 60.0f;
    float tail_hp = 20.0f; // takes the hits once the fuselage is gone
    int pilot_wounds = 3;  // the last one kills
    float wound_damage = 5.0f;
    float engine_min_share = 0.6f; // top speed (and acceleration) at 1 HP
    float wing_min_share = 0.6f;   // turn rate at 1 HP, from the weaker wing
    float leak_tank_s = 60.0f;     // at 1 HP the fuselage leaks a full tank in this long
    float collision_radius_u = 7.0f;
    float collision_height_u = 6.0f;
    float spin_rate_deg_s = 180.0f; // going down
    float spin_pitch_deg = -35.0f;
    // Where hits land, in percent: rows ahead, behind, side, burst; columns engine, pilot, wings,
    // fuselage.
    std::array<std::array<int, 4>, 4> table{{
        {{45, 20, 25, 10}},
        {{10, 10, 30, 50}},
        {{10, 15, 50, 25}},
        {{15, 10, 45, 30}},
    }};

    friend constexpr bool operator==(const DamageParams&, const DamageParams&) = default;
};

// A part's stage from its HP: OK above 75%, WARNING 50-75%, DANGER 25-50%, CRITICAL below, and
// destroyed at 0.
[[nodiscard]] Stage stage_of(float hp, float max_hp) noexcept;

// The pilot's: OK unhurt, WARNING after one wound, CRITICAL after more, destroyed (killed) at the
// last.
[[nodiscard]] Stage pilot_stage(int wounds, int max_wounds) noexcept;

// A part's HP and its stage, for the readout and the HUD.
[[nodiscard]] float part_hp(const Body& b, Part p) noexcept;
[[nodiscard]] Stage part_stage(const Body& b, Part p, const DamageParams& d) noexcept;

// Engine + both wings + fuselage + tail (the pilot isn't HP): what collisions compare.
[[nodiscard]] float total_hp(const Body& b) noexcept;

// The parts no longer on the plane (wings and the tail), as part_bit()s, for drawing.
[[nodiscard]] std::uint32_t missing_parts(const Body& b) noexcept;

// A new (or mended) plane: every part at full HP, the pilot unhurt, not going down; and the
// engine running again if there's fuel.
void repair(Body& b, const DamageParams& d) noexcept;

// Where a hit came from: (toward_x, toward_y) points from the plane toward the source.
[[nodiscard]] HitFrom hit_from(const Body& b, float toward_x, float toward_y) noexcept;

// The part a hit lands on, drawn from the table. The wings' share goes to the wing on the hit's
// side, or either wing; hits on a destroyed fuselage go to the tail.
[[nodiscard]] Part pick_part(const Body& b, HitFrom from, const DamageParams& d, Rng& rng) noexcept;

// Applies damage to a part and what follows from it:
// - the engine at 0 stops (the plane glides);
// - the fuselage at 0 loses all the fuel (any extra goes to the tail);
// - a wing or the tail at 0 sends the plane down (ShotDown), spinning toward a lost wing;
// - every wound_damage on the pilot is a wound, and the last one sends it down (PilotKilled).
void apply_hit(Body& b, Part p, float damage, const DamageParams& d) noexcept;

// How damage changes flying, gradually as HP falls.
struct Handling {
    float top_speed_share = 1.0f; // the engine: top speed and acceleration
    float turn_share = 1.0f;      // the weaker wing
    float leak_per_s = 0.0f;      // the fuselage: share of a full tank lost per second
    float spin_deg_s = 0.0f;      // going down: spin rate (signed); 0 when in control
    float spin_pitch_deg = 0.0f;  // going down: where the nose goes
};
[[nodiscard]] Handling handling_of(const Body& b, const DamageParams& d) noexcept;

} // namespace wof
