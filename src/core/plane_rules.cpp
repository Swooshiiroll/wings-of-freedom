#include "core/plane_rules.hpp"

#include <algorithm>

namespace wof {

float best_climb(const FlightParams& f) noexcept {
    const Aero a = aero_of(f);
    float best = 0.0f;
    for (float v = std::max(1.0f, f.stall_u_s); v <= f.max_level_u_s; v += 0.5f) {
        best = std::max(best, v * (thrust(a, v, 1.0f) - drag(a, f, v)) / f.gravity);
    }
    return best;
}

FlightParams derive_flight(const PlaneType& t, const FlightParams& base,
                           const PlaneScale& s) noexcept {
    FlightParams f = base;
    const float top = t.top_mph * s.u_per_mph;
    f.max_level_u_s = top;
    f.dive_limit_u_s = top * s.dive_factor;
    f.stall_u_s = t.stall_mph * s.u_per_mph;
    f.stall_recover_u_s = f.stall_u_s + s.stall_margin_u_s;
    f.turn_rate_deg_s = t.turn_s > 0.0f ? 360.0f / t.turn_s * s.turn_factor : base.turn_rate_deg_s;
    f.best_turn_u_s = top * s.best_turn_share;
    f.pitch_rate_deg_s = t.pitch_rate_deg_s;
    f.tank_s = t.endurance_h * s.tank_s_per_hour;
    f.full_thrust_below_u_s = std::max(1.0f, f.stall_u_s); // constant power above the stall

    // A higher best-glide speed means more induced drag at low speed, so less to spare for
    // climbing: search it out so the best climb matches the plane's.
    const float target = t.climb_fpm * s.u_per_fpm;
    float lo = std::max(1.0f, f.stall_u_s * 1.05f);
    float hi = std::max(lo, top * 0.98f);
    for (int i = 0; i < 40; ++i) {
        f.best_glide_u_s = 0.5f * (lo + hi);
        if (best_climb(f) > target) {
            lo = f.best_glide_u_s;
        } else {
            hi = f.best_glide_u_s;
        }
    }
    f.best_glide_u_s = 0.5f * (lo + hi);
    return f;
}

PlaneRules derive_plane(const PlaneType& t, const FlightParams& base, const DamageParams& damage,
                        const PlaneScale& s) {
    PlaneRules r;
    r.flight = derive_flight(t, base, s);
    r.aero = aero_of(r.flight);
    r.damage = damage;
    r.damage.engines = t.engines;
    r.damage.engine_hp = t.engine_hp;
    r.damage.wing_hp = t.wing_hp;
    r.damage.fuselage_hp = t.fuselage_hp;
    r.damage.tail_hp = t.tail_hp;
    r.guns = t.guns;
    r.gun_count = t.gun_count;
    r.bomb_lb = t.bomb_lb;
    r.bombs = t.bombs;
    return r;
}

std::vector<PlaneRules> derive_roster(const FlightParams& base, const DamageParams& damage,
                                      const PlaneScale& s) {
    std::vector<PlaneRules> out;
    for (int i = 0; i < kPlaneTypes; ++i) {
        PlaneRules r = derive_plane(plane_type(i), base, damage, s);
        r.id = static_cast<PlaneId>(i);
        out.push_back(r);
    }
    return out;
}

} // namespace wof
