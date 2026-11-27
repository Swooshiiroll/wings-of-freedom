#pragma once

#include <cstdint>

#include "core/body.hpp"
#include "core/input.hpp"
#include "core/rng.hpp"

namespace wof {

struct World;
struct Rules;

// Enemy pilots (design/flight.md, Enemies). They fly the same planes under the same rules as the
// player, asking for what they want through ordinary input commands, so a host runs them for
// every client in co-op later. Everything random comes from the world's generator.

// How a fighter attacks; each one gets a style at random.
enum class Style : std::uint8_t {
    Turn,      // gets on your tail and stays there
    DiveClimb, // climbs above you, dives through, climbs away
    HeadOn,    // lines up passes from the front
};
inline constexpr int kStyles = 3;
[[nodiscard]] const char* style_name(Style s) noexcept;

// What it's doing now (also the F1 overlay's label).
enum class Phase : std::uint8_t {
    Pursue,  // turn-fighter after its target
    Climb,   // dive-and-climb: gaining height
    Line,    // head-on: getting out in front
    Attack,  // diving or head-on at the target
    Extend,  // flying clear before coming back
    Break,   // turning away so as not to ram
    Recover, // pulling up from the ground
    Return,  // turning back inside the battle area
};
[[nodiscard]] const char* phase_name(Phase p) noexcept;

// The pilots' numbers (tuning.txt `ai.*`), all (start) values.
struct AiParams {
    float aim_error_first_deg = 8.0f; // wave 1's rookies
    float aim_error_best_deg = 2.0f;  // never better than this
    float aim_error_step_deg = 1.0f;  // sharper by this each wave
    float reaction_first_s = 0.5f;    // how often it looks again
    float reaction_best_s = 0.15f;
    float reaction_step_s = 0.07f;
    float fire_range_u = 220.0f;
    float fire_cone_deg = 5.0f;   // fires with its nose this close to where it's aiming
    float break_range_u = 45.0f;  // breaks off closer than this, nose on
    float floor_u = 40.0f;        // pulls up so as never to go below this
    float edge_margin_u = 250.0f; // turns back this far inside the soft edges
    float dive_height_u = 80.0f;  // dive-and-climb: gets this far above first
    float head_on_run_u = 400.0f; // head-on: lines up from this far in front
    float extend_s = 3.0f;        // flies clear this long after a pass

    friend constexpr bool operator==(const AiParams&, const AiParams&) = default;
};

// How good a pilot is.
struct Skill {
    float aim_error_deg = 8.0f; // the most its aim is off, each way
    float reaction_s = 0.5f;    // between looks at its target
};
[[nodiscard]] Skill skill_of(const AiParams& p, int wave) noexcept;

// One enemy pilot.
struct Pilot {
    int body = -1;  // the plane it flies (World::bodies)
    int target = 0; // the plane it hunts
    Style style = Style::Turn;
    Phase phase = Phase::Pursue; // its style's own step
    Phase doing = Phase::Pursue; // what it's doing this tick (the step, or pulling up and so on)
    Skill skill{};
    float phase_s = 0.0f; // time left in a timed phase
    float look_s = 0.0f;  // until it looks at its target again
    // What it saw last: its target's position and velocity.
    float seen_x = 0.0f;
    float seen_y = 0.0f;
    float seen_z = 0.0f;
    float seen_vx = 0.0f;
    float seen_vy = 0.0f;
    float seen_vz = 0.0f;
    float seen_age_s = 0.0f;
    float aim_off_deg = 0.0f; // its current error, across and up or down
    float aim_off_pitch_deg = 0.0f;
    float side = 1.0f;    // which way it breaks
    bool hurt = false;    // the player has damaged or rammed it
    bool counted = false; // already counted as a kill
};

// A new pilot for the body, hunting body 0 (the player), starting its style's first phase.
[[nodiscard]] Pilot new_pilot(int body, Style style, Skill skill, Rng& rng) noexcept;

// What the pilot asks of its plane this tick.
[[nodiscard]] InputCommand ai_command(Pilot& p, const World& world, const Rules& rules,
                                      Rng& rng) noexcept;

} // namespace wof
