#include "core/damage.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "core/iso.hpp"

namespace wof {
namespace {

[[nodiscard]] float share(float hp, float max_hp) noexcept {
    return max_hp > 0.0f ? std::clamp(hp / max_hp, 0.0f, 1.0f) : 0.0f;
}

void go_down(Body& b, Outcome cause, float spin) noexcept {
    if (b.damage.going_down) {
        return;
    }
    b.damage.going_down = true;
    b.damage.cause = cause;
    b.damage.spin = spin;
    b.engine_on = false;
}

void hit_tail(Body& b, float damage) noexcept {
    b.damage.tail = std::max(0.0f, b.damage.tail - damage);
    if (b.damage.tail <= 0.0f) {
        go_down(b, Outcome::ShotDown, b.damage.spin);
    }
}

} // namespace

const char* stage_name(Stage s) noexcept {
    switch (s) {
    case Stage::Ok:
        return "OK";
    case Stage::Warning:
        return "WARN";
    case Stage::Danger:
        return "DNGR";
    case Stage::Critical:
        return "CRIT";
    case Stage::Destroyed:
        return "GONE";
    }
    return "?";
}

Stage stage_of(float hp, float max_hp) noexcept {
    if (hp <= 0.0f) {
        return Stage::Destroyed;
    }
    const float s = share(hp, max_hp);
    return s > 0.75f   ? Stage::Ok
           : s > 0.5f  ? Stage::Warning
           : s > 0.25f ? Stage::Danger
                       : Stage::Critical;
}

Stage pilot_stage(int wounds, int max_wounds) noexcept {
    if (wounds >= max_wounds) {
        return Stage::Destroyed;
    }
    return wounds <= 0 ? Stage::Ok : wounds == 1 ? Stage::Warning : Stage::Critical;
}

float part_hp(const Body& b, Part p) noexcept {
    switch (p) {
    case Part::Engine:
        return b.damage.engine;
    case Part::Engine2:
        return b.damage.engine2;
    case Part::LeftWing:
        return b.damage.left_wing;
    case Part::RightWing:
        return b.damage.right_wing;
    case Part::Fuselage:
        return b.damage.fuselage;
    case Part::Tail:
        return b.damage.tail;
    case Part::Cockpit:
        return 0.0f;
    }
    return 0.0f;
}

Stage part_stage(const Body& b, Part p, const DamageParams& d) noexcept {
    switch (p) {
    case Part::Engine:
        return stage_of(b.damage.engine, d.engine_hp);
    case Part::Engine2:
        return b.damage.engines > 1 ? stage_of(b.damage.engine2, d.engine_hp) : Stage::Ok;
    case Part::LeftWing:
        return stage_of(b.damage.left_wing, d.wing_hp);
    case Part::RightWing:
        return stage_of(b.damage.right_wing, d.wing_hp);
    case Part::Fuselage:
        return stage_of(b.damage.fuselage, d.fuselage_hp);
    case Part::Tail:
        return stage_of(b.damage.tail, d.tail_hp);
    case Part::Cockpit:
        return pilot_stage(b.damage.wounds, d.pilot_wounds);
    }
    return Stage::Ok;
}

float total_hp(const Body& b) noexcept {
    const Damage& m = b.damage;
    return m.engine + m.engine2 + m.left_wing + m.right_wing + m.fuselage + m.tail;
}

std::uint32_t missing_parts(const Body& b) noexcept {
    std::uint32_t missing = 0;
    if (b.damage.left_wing <= 0.0f) {
        missing |= part_bit(Part::LeftWing);
    }
    if (b.damage.right_wing <= 0.0f) {
        missing |= part_bit(Part::RightWing);
    }
    if (b.damage.tail <= 0.0f) {
        missing |= part_bit(Part::Tail);
    }
    return missing;
}

void repair(Body& b, const DamageParams& d) noexcept {
    b.damage = Damage{};
    b.damage.engines = std::clamp(d.engines, 1, 2);
    b.damage.engine = d.engine_hp;
    b.damage.engine2 = b.damage.engines > 1 ? d.engine_hp : 0.0f;
    b.damage.left_wing = d.wing_hp;
    b.damage.right_wing = d.wing_hp;
    b.damage.fuselage = d.fuselage_hp;
    b.damage.tail = d.tail_hp;
    b.engine_on = b.fuel > 0.0f;
}

HitFrom hit_from(const Body& b, float toward_x, float toward_y) noexcept {
    if (toward_x == 0.0f && toward_y == 0.0f) {
        return HitFrom::Burst;
    }
    const float bearing = std::atan2(toward_x, toward_y) * (180.0f / std::numbers::pi_v<float>);
    float off = std::fmod(bearing - b.heading_deg + 540.0f, 360.0f) - 180.0f; // -180..180
    if (std::abs(off) <= 45.0f) {
        return HitFrom::Ahead;
    }
    if (std::abs(off) >= 135.0f) {
        return HitFrom::Behind;
    }
    return off > 0.0f ? HitFrom::Right : HitFrom::Left;
}

Part pick_part(const Body& b, HitFrom from, const DamageParams& d, Rng& rng) noexcept {
    const std::size_t row = from == HitFrom::Ahead    ? 0
                            : from == HitFrom::Behind ? 1
                            : from == HitFrom::Burst  ? 3
                                                      : 2;
    const auto& w = d.table[row];
    int total = 0;
    for (const int x : w) {
        total += std::max(0, x);
    }
    int r = static_cast<int>(rng.next_below(static_cast<std::uint32_t>(std::max(1, total))));
    std::size_t column = 3;
    for (std::size_t c = 0; c < 4; ++c) {
        r -= std::max(0, w[c]);
        if (r < 0) {
            column = c;
            break;
        }
    }
    switch (column) {
    case 0:
        if (b.damage.engines < 2) {
            return Part::Engine;
        }
        // A twin: the engine on the hit's side, or either one.
        if (from == HitFrom::Left) {
            return Part::Engine;
        }
        if (from == HitFrom::Right) {
            return Part::Engine2;
        }
        return rng.next_below(2) == 0 ? Part::Engine : Part::Engine2;
    case 1:
        return Part::Cockpit;
    case 2:
        if (from == HitFrom::Left) {
            return Part::LeftWing;
        }
        if (from == HitFrom::Right) {
            return Part::RightWing;
        }
        return rng.next_below(2) == 0 ? Part::LeftWing : Part::RightWing;
    default:
        return b.damage.fuselage > 0.0f ? Part::Fuselage : Part::Tail;
    }
}

void apply_hit(Body& b, Part p, float damage, const DamageParams& d) noexcept {
    Damage& m = b.damage;
    switch (p) {
    case Part::Engine:
        m.engine = std::max(0.0f, m.engine - damage);
        if (m.engine <= 0.0f && (m.engines < 2 || m.engine2 <= 0.0f)) {
            b.engine_on = false; // no engine left
        }
        break;
    case Part::Engine2:
        m.engine2 = std::max(0.0f, m.engine2 - damage);
        if (m.engine2 <= 0.0f && m.engine <= 0.0f) {
            b.engine_on = false;
        }
        break;
    case Part::LeftWing:
        m.left_wing = std::max(0.0f, m.left_wing - damage);
        if (m.left_wing <= 0.0f) {
            go_down(b, Outcome::ShotDown, -1.0f);
        }
        break;
    case Part::RightWing:
        m.right_wing = std::max(0.0f, m.right_wing - damage);
        if (m.right_wing <= 0.0f) {
            go_down(b, Outcome::ShotDown, 1.0f);
        }
        break;
    case Part::Fuselage:
        if (m.fuselage <= 0.0f) {
            hit_tail(b, damage);
            break;
        }
        m.fuselage -= damage;
        if (m.fuselage <= 0.0f) {
            const float extra = -m.fuselage;
            m.fuselage = 0.0f;
            b.fuel = 0.0f; // all fuel is lost
            b.engine_on = false;
            if (extra > 0.0f) {
                hit_tail(b, extra);
            }
        }
        break;
    case Part::Tail:
        hit_tail(b, damage);
        break;
    case Part::Cockpit:
        m.pilot += damage;
        m.wounds = std::min(d.pilot_wounds,
                            static_cast<int>(std::floor(m.pilot / std::max(0.1f, d.wound_damage))));
        if (m.wounds >= d.pilot_wounds) {
            go_down(b, Outcome::PilotKilled, b.heading_deg < 180.0f ? 1.0f : -1.0f);
        }
        break;
    }
}

Handling handling_of(const Body& b, const DamageParams& d) noexcept {
    Handling h;
    const Damage& m = b.damage;
    const auto engine_share = [&](float hp) {
        return hp <= 0.0f ? 0.0f
                          : d.engine_min_share + (1.0f - d.engine_min_share) * share(hp, d.engine_hp);
    };
    if (m.engines < 2) {
        h.top_speed_share = engine_share(m.engine);
    } else {
        // A twin: the power adds up, and top speed goes as the cube root of power, so losing one
        // engine (half the power) leaves about 79% of the top speed.
        const float a = engine_share(m.engine);
        const float c = engine_share(m.engine2);
        h.top_speed_share = std::cbrt(0.5f * (a * a * a + c * c * c));
    }
    if (h.top_speed_share <= 0.0f) {
        h.top_speed_share = 1.0f; // no engine at all: the plane glides, nothing to scale
    }
    const float weaker = std::min(share(m.left_wing, d.wing_hp), share(m.right_wing, d.wing_hp));
    h.turn_share = d.wing_min_share + (1.0f - d.wing_min_share) * weaker;
    if (m.fuselage > 0.0f) {
        h.leak_per_s = (1.0f - share(m.fuselage, d.fuselage_hp)) / std::max(1.0f, d.leak_tank_s);
    }
    if (m.going_down) {
        h.spin_deg_s = d.spin_rate_deg_s * m.spin;
        h.spin_pitch_deg = d.spin_pitch_deg;
    }
    return h;
}

} // namespace wof
