#include "core/flight.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/iso.hpp"

namespace wof {
namespace {

constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

// Moves `value` toward `target` by at most `step`.
[[nodiscard]] float approach(float value, float target, float step) noexcept {
    return value < target ? std::min(value + step, target) : std::max(value - step, target);
}

} // namespace

Aero aero_of(const FlightParams& p) noexcept {
    Aero a;
    // At the best glide speed the two drags are equal, and drag / g is the glide path's sine.
    const float sin_glide = 1.0f / std::sqrt(1.0f + p.glide_ratio * p.glide_ratio);
    const float v2 = p.best_glide_u_s * p.best_glide_u_s;
    a.k1 = p.gravity * sin_glide / (2.0f * v2);
    a.k2 = a.k1 * v2 * v2;
    a.full_below = std::max(1.0f, p.full_thrust_below_u_s);
    const float top = std::max(1.0f, p.max_level_u_s);
    a.power = drag(a, p, top) * std::max(top, a.full_below);
    return a;
}

float drag(const Aero& a, const FlightParams& p, float v) noexcept {
    const float lift_v = std::max(v, std::max(1.0f, p.stall_u_s));
    return a.k1 * v * v + a.k2 / (lift_v * lift_v);
}

float thrust(const Aero& a, float v, float throttle) noexcept {
    return throttle * a.power / std::max(v, a.full_below);
}

float turn_rate(const FlightParams& p, float v) noexcept {
    const float best = std::max(1.0f, p.best_turn_u_s);
    const float share = v <= best ? v / best : best / v;
    return p.turn_rate_deg_s * std::max(0.0f, share);
}

Body start_body(const FlightParams& p, float x, float y, float heading_deg) noexcept {
    Body b;
    b.x = x;
    b.y = y;
    b.z = std::clamp(p.start_height_u, 0.0f, p.ceiling_u);
    b.heading_deg = wrap_heading(heading_deg);
    b.speed_u_s = p.start_speed_u_s;
    b.throttle = std::clamp(p.start_throttle, 0.0f, 1.0f);
    return b;
}

void fly(Body& b, const InputCommand& cmd, const FlightParams& p, const Aero& a,
         float dt) noexcept {
    // Throttle, then fuel: burn follows the throttle, down to idle_burn at idle.
    b.throttle = std::clamp(b.throttle + axis(cmd.throttle) * p.throttle_per_s * dt, 0.0f, 1.0f);
    if (b.engine_on) {
        const float burn =
            (p.idle_burn + (1.0f - p.idle_burn) * b.throttle) * dt / std::max(1.0f, p.tank_s);
        b.fuel = std::max(0.0f, b.fuel - burn);
        if (b.fuel <= 0.0f) {
            b.engine_on = false; // the plane glides from here
        }
    }

    // Stall: below stall_u_s until back above stall_recover_u_s. The nose drops on its own and
    // the controls go soft.
    if (!b.stalled && b.speed_u_s < p.stall_u_s) {
        b.stalled = true;
    } else if (b.stalled && b.speed_u_s > p.stall_recover_u_s) {
        b.stalled = false;
    }
    const float control = b.stalled ? p.stall_control : 1.0f;

    // Pitch: W/S move it; let go and it eases back to level. A stall pulls the nose down.
    const float pitch_in = axis(cmd.pitch) * control;
    const float pitch_step = p.pitch_rate_deg_s * dt;
    if (pitch_in != 0.0f) {
        b.pitch_deg += pitch_in * pitch_step;
    } else if (!b.stalled) {
        b.pitch_deg = approach(b.pitch_deg, 0.0f, pitch_step);
    }
    if (b.stalled && b.pitch_deg > p.stall_nose_deg) {
        b.pitch_deg = std::max(p.stall_nose_deg, b.pitch_deg - p.stall_nose_rate_deg_s * dt);
    }
    if (b.z >= p.ceiling_u && b.pitch_deg > 0.0f) {
        b.pitch_deg = approach(b.pitch_deg, 0.0f, pitch_step); // nothing to climb into
    }
    b.pitch_deg = std::clamp(b.pitch_deg, -p.pitch_limit_deg, p.pitch_limit_deg);

    // Turning, and the bank that shows it.
    const float turn_in = axis(cmd.turn) * control;
    b.heading_deg = wrap_heading(b.heading_deg + turn_in * turn_rate(p, b.speed_u_s) * dt);
    b.bank_deg = approach(b.bank_deg, axis(cmd.turn) * p.bank_deg, p.bank_rate_deg_s * dt);

    // Speed along the flight path: thrust, drag, and gravity along the climb or dive.
    const float pitch = b.pitch_deg * kDegToRad;
    const float push = b.engine_on ? thrust(a, b.speed_u_s, b.throttle) : 0.0f;
    const float accel = push - drag(a, p, b.speed_u_s) - p.gravity * std::sin(pitch);
    b.speed_u_s = std::clamp(b.speed_u_s + accel * dt, 0.0f, p.dive_limit_u_s);

    // Position: along the heading at v·cos(pitch), up or down at v·sin(pitch).
    const float heading = b.heading_deg * kDegToRad;
    const float across = b.speed_u_s * std::cos(pitch) * dt;
    b.x += std::sin(heading) * across;
    b.y += std::cos(heading) * across;
    b.z = std::min(b.z + b.speed_u_s * std::sin(pitch) * dt, p.ceiling_u);
}

} // namespace wof
