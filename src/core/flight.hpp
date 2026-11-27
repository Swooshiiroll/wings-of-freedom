#pragma once

#include "core/body.hpp"
#include "core/input.hpp"

namespace wof {

// The flight model's numbers (design/flight.md, Flight and Fuel; tuning.txt `flight.*`), all
// (start) values. Speeds in u/s, angles in degrees, times in seconds.
struct FlightParams {
    float gravity = 90.0f;                // u/s²: the pull along a climb or a dive
    float max_level_u_s = 150.0f;         // full throttle levels off here
    float dive_limit_u_s = 240.0f;        // never faster than this
    float full_thrust_below_u_s = 110.0f; // the propeller's thrust is full up to here, then ~1/v
    float glide_ratio = 6.0f;             // engine off: forward per unit of height lost
    float best_glide_u_s = 90.0f;         // at this speed
    float pitch_rate_deg_s = 60.0f;       // W/S; the nose also eases back to level at this rate
    float pitch_limit_deg = 40.0f;
    float turn_rate_deg_s = 100.0f; // at the best turn speed; slower above and below it
    float best_turn_u_s = 110.0f;
    float throttle_per_s = 0.5f;         // Shift/Ctrl: share of full throttle per second
    float stall_u_s = 55.0f;             // below this the plane stalls
    float stall_recover_u_s = 63.0f;     // and recovers above this
    float stall_nose_deg = -30.0f;       // a stall drops the nose toward this
    float stall_nose_rate_deg_s = 40.0f; // at this rate
    float stall_control = 0.3f;          // share of turn and pitch control while stalled
    float ceiling_u = 240.0f;
    float tank_s = 240.0f;  // a full tank lasts this long at full throttle
    float idle_burn = 0.3f; // fuel burn at idle, as a share of full-throttle burn
    float bank_deg = 30.0f; // looks only: how far the plane banks into a turn
    float bank_rate_deg_s = 120.0f;
    float start_height_u = 120.0f; // a new run starts in the air at this height and speed,
    float start_speed_u_s = 110.0f;
    float start_throttle = 0.8f; // with this throttle

    friend constexpr bool operator==(const FlightParams&, const FlightParams&) = default;
};

// The speed model, worked out from FlightParams so its targets all hold:
// - drag is k1·v² (parasitic) + k2/v² (induced); with best glide 6:1 at 90 u/s that sets k1, k2;
// - thrust is a propeller's: full up to full_thrust_below_u_s, then power / v, scaled by the
//   throttle; the power is set so full throttle levels off at max_level_u_s.
struct Aero {
    float k1 = 0.0f;
    float k2 = 0.0f;
    float power = 0.0f; // thrust x speed above full_thrust_below_u_s
    float full_below = 0.0f;
};
[[nodiscard]] Aero aero_of(const FlightParams& p) noexcept;

// Deceleration from drag at speed v (u/s²). The induced part stops growing below the stall
// speed (a stalled wing isn't lifting).
[[nodiscard]] float drag(const Aero& a, const FlightParams& p, float v) noexcept;

// Acceleration from thrust at speed v and a throttle (0..1), u/s².
[[nodiscard]] float thrust(const Aero& a, float v, float throttle) noexcept;

// The turn rate at speed v with full input, degrees per second: best at best_turn_u_s, in
// proportion to v below it and to 1/v above it.
[[nodiscard]] float turn_rate(const FlightParams& p, float v) noexcept;

// A plane at the start of a run.
[[nodiscard]] Body start_body(const FlightParams& p, float x, float y, float heading_deg) noexcept;

// One tick of flight for one plane: throttle, fuel, stall, pitch, turn, bank, speed and position.
// Doesn't apply the ground or the edges (step() in world.hpp does).
void fly(Body& b, const InputCommand& cmd, const FlightParams& p, const Aero& a, float dt) noexcept;

} // namespace wof
