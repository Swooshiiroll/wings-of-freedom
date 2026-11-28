#include "core/ai.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "core/map.hpp"
#include "core/world.hpp"

namespace wof {
namespace {

constexpr float kToDeg = 180.0f / std::numbers::pi_v<float>;
constexpr float kToRad = std::numbers::pi_v<float> / 180.0f;
constexpr float kDodgeS = 0.35f;  // a dodge holds this long past the last sight of the danger
constexpr float kAttackS = 8.0f;  // an attack that hasn't closed by now is given up
constexpr float kPartedAt = 3.0f; // a fight is over this many times dogfight_u apart

[[nodiscard]] float wrap180(float deg) noexcept {
    float w = std::fmod(deg + 180.0f, 360.0f);
    if (w < 0.0f) {
        w += 360.0f;
    }
    return w - 180.0f;
}

// The compass bearing of a ground offset.
[[nodiscard]] float bearing(float dx, float dy) noexcept {
    return std::atan2(dx, dy) * kToDeg;
}

[[nodiscard]] std::int8_t to_axis(float v) noexcept {
    return static_cast<std::int8_t>(
        std::lround(std::clamp(v, -1.0f, 1.0f) * static_cast<float>(kAxisFull)));
}

[[nodiscard]] std::int8_t turn_toward(float have, float want) noexcept {
    return to_axis(wrap180(want - have) / 8.0f);
}

// Moves the nose to `want` degrees and holds it there (letting go would ease it to level).
[[nodiscard]] std::int8_t pitch_toward(float have, float want) noexcept {
    if (have < want - 1.0f) {
        return to_axis(1.0f);
    }
    if (have > want + 1.0f) {
        return to_axis(-1.0f);
    }
    if (std::abs(want) < 1.0f) {
        return 0;
    }
    return static_cast<std::int8_t>(want > 0.0f ? 1 : -1);
}

// The nose angle for gaining or losing `dz` of height.
[[nodiscard]] float climb_pitch(float dz) noexcept {
    return std::clamp(dz * 0.8f, -30.0f, 30.0f);
}

// What a pilot wants to do this tick.
struct Wish {
    float heading = 0.0f;
    float pitch = 0.0f;
    bool shoot = false;
    bool slow = false;
};

// The throttle to hold a speed: up when slow, back when fast.
[[nodiscard]] std::int8_t throttle_for(float speed, float want) noexcept {
    return to_axis(speed < want - 1.0f ? 1.0f : speed > want + 1.0f ? -1.0f : 0.0f);
}

// The flying every duty shares at the end: not into the ground or a stall.
void keep_flying(Pilot& p, const Body& self, const PlaneRules& plane, const AiParams& a,
                 Wish& w) noexcept {
    const float sink = std::min(0.0f, self.speed_u_s * std::sin(self.pitch_deg * kToRad)) *
                       plane.flight.height_share;
    if (self.z + sink * 1.5f < a.floor_u) {
        w.pitch = 30.0f;
        w.shoot = false;
        p.doing = Phase::Recover;
    }
    if (self.stalled || self.speed_u_s < plane.flight.stall_u_s * 1.2f) {
        w.pitch = std::min(w.pitch, -5.0f);
    }
}

// A body's velocity: across the ground and in height (which moves at its share, core/flight).
struct Velocity {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};
[[nodiscard]] Velocity velocity_of(const Rules& rules, const Body& b) noexcept {
    const float h = b.heading_deg * kToRad;
    const float p = b.pitch_deg * kToRad;
    const float level = b.speed_u_s * std::cos(p);
    return {std::sin(h) * level, std::cos(h) * level,
            b.speed_u_s * std::sin(p) * plane_of(rules, b).flight.height_share};
}

// Never ram a player (design/flight.md, Enemies): if this plane and a player's will pass too close
// within avoid_s, turn away from where they'll be. The turn is level: tipping over or under as
// well walks a dogfight up or down the sky, as each follows the other's height.
void dodge(Pilot& p, const World& world, const Rules& rules, const Body& self, Wish& w) noexcept {
    const AiParams& a = rules.ai;
    p.dodge_s = std::max(0.0f, p.dodge_s - kTickSeconds);
    const Velocity mine = velocity_of(rules, self);
    const float h = self.heading_deg * kToRad;
    float soonest = a.avoid_s + 1.0f;
    for (std::size_t i = 0; i < world.bodies.size(); ++i) {
        const Body& o = world.bodies[i];
        const bool piloted =
            std::any_of(world.pilots.begin(), world.pilots.end(),
                        [i](const Pilot& q) { return q.body == static_cast<int>(i); });
        if (piloted || o.outcome != Outcome::Flying) {
            continue;
        }
        const Velocity theirs = velocity_of(rules, o);
        const float rx = o.x - self.x;
        const float ry = o.y - self.y;
        const float rz = o.z - self.z;
        const float vx = theirs.x - mine.x;
        const float vy = theirs.y - mine.y;
        const float vz = theirs.z - mine.z;
        const float v2 = vx * vx + vy * vy + vz * vz;
        // When the two are closest, within the look-ahead, and how far apart then.
        const float t =
            v2 > 1.0f ? std::clamp(-(rx * vx + ry * vy + rz * vz) / v2, 0.0f, a.avoid_s) : 0.0f;
        const float mx = rx + vx * t;
        const float my = ry + vy * t;
        const float mz = rz + vz * t;
        const float size = (plane_type(self.type).size + plane_type(o.type).size) * 0.5f;
        const float tall = rules.damage.collision_height_u * 2.5f;
        if (std::hypot(mx, my) >= a.avoid_u * size || std::abs(mz) >= tall || t >= soonest) {
            continue;
        }
        soonest = t;
        if (p.dodge_s <= 0.0f) {
            // Pick a way once and keep it, so it doesn't dither.
            const float right = mx * std::cos(h) - my * std::sin(h);
            p.dodge_turn = std::abs(right) < 2.0f ? p.side : (right > 0.0f ? -1.0f : 1.0f);
        }
        p.dodge_s = kDodgeS;
    }
    if (p.dodge_s > 0.0f) {
        w.heading = self.heading_deg + p.dodge_turn * 90.0f;
        w.pitch = 0.0f;
        w.slow = false;
        p.doing = Phase::Break;
    }
}

// The middle of a raid's bombers still flying, and their course. False if none are.
[[nodiscard]] bool formation_of(const World& world, int raid, float& x, float& y, float& z,
                                float& course) noexcept {
    int n = 0;
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
    for (const Pilot& other : world.pilots) {
        const Body& o = world.bodies[static_cast<std::size_t>(other.body)];
        if (other.duty != Duty::Bomb || other.raid != raid || o.outcome != Outcome::Flying ||
            o.damage.going_down) {
            continue;
        }
        x += o.x;
        y += o.y;
        z += o.z;
        course = other.course_deg;
        ++n;
    }
    if (n == 0) {
        return false;
    }
    x /= static_cast<float>(n);
    y /= static_cast<float>(n);
    z /= static_cast<float>(n);
    return true;
}

// Another AI's plane close enough in front to be in the way of this one's rounds.
[[nodiscard]] bool friend_in_line(const World& w, const Pilot& p, const Body& self,
                                  float range) noexcept {
    for (const Pilot& other : w.pilots) {
        if (other.body == p.body) {
            continue;
        }
        const Body& o = w.bodies[static_cast<std::size_t>(other.body)];
        if (o.outcome != Outcome::Flying) {
            continue;
        }
        const float dx = o.x - self.x;
        const float dy = o.y - self.y;
        const float d = std::hypot(dx, dy);
        if (d < range && std::abs(o.z - self.z) < 12.0f &&
            std::abs(wrap180(bearing(dx, dy) - self.heading_deg)) < 5.0f) {
            return true;
        }
    }
    return false;
}

} // namespace

const char* style_name(Style s) noexcept {
    switch (s) {
    case Style::Turn:
        return "turn-fighter";
    case Style::DiveClimb:
        return "dive-and-climb";
    case Style::HeadOn:
        return "head-on";
    }
    return "?";
}

const char* phase_name(Phase p) noexcept {
    switch (p) {
    case Phase::Pursue:
        return "pursue";
    case Phase::Climb:
        return "climb";
    case Phase::Line:
        return "line up";
    case Phase::Attack:
        return "attack";
    case Phase::Extend:
        return "extend";
    case Phase::Break:
        return "break";
    case Phase::Recover:
        return "pull up";
    case Phase::Return:
        return "turn back";
    case Phase::Course:
        return "on course";
    case Phase::Guard:
        return "guard";
    }
    return "?";
}

Skill skill_of(const AiParams& p, int wave) noexcept {
    const auto steps = static_cast<float>(std::max(0, wave - 1));
    return {std::max(p.aim_error_best_deg, p.aim_error_first_deg - p.aim_error_step_deg * steps),
            std::max(p.reaction_best_s, p.reaction_first_s - p.reaction_step_s * steps)};
}

Pilot new_pilot(int body, Style style, Skill skill, Rng& rng) noexcept {
    Pilot p;
    p.body = body;
    p.style = style;
    p.skill = skill;
    p.phase = style == Style::Turn        ? Phase::Pursue
              : style == Style::DiveClimb ? Phase::Climb
                                          : Phase::Line;
    p.doing = p.phase;
    p.side = rng.next_below(2) == 0 ? -1.0f : 1.0f;
    return p;
}

InputCommand ai_command(Pilot& p, const World& world, const Rules& rules, Rng& rng) noexcept {
    InputCommand c;
    const Body& self = world.bodies[static_cast<std::size_t>(p.body)];
    if (self.outcome != Outcome::Flying || self.damage.going_down) {
        return c;
    }
    const Body& target = world.bodies[static_cast<std::size_t>(p.target)];
    const PlaneRules& plane = plane_of(rules, self);
    const AiParams& a = rules.ai;
    const bool hunting = target.outcome == Outcome::Flying;
    const auto command = [&](const Wish& w, std::int8_t throttle) {
        const float pitch =
            std::clamp(w.pitch, -plane.flight.pitch_limit_deg, plane.flight.pitch_limit_deg);
        c.turn = turn_toward(self.heading_deg, w.heading);
        c.pitch = pitch_toward(self.pitch_deg, pitch);
        c.throttle = throttle;
    };

    // A bomber holds its course, height and speed whatever happens (design/flight.md, Bombers).
    if (p.duty == Duty::Bomb) {
        Wish w{p.course_deg, std::clamp(climb_pitch(p.height_u - self.z), -15.0f, 15.0f), false,
               false};
        p.doing = Phase::Course;
        keep_flying(p, self, plane, a, w);
        command(w, throttle_for(self.speed_u_s, p.cruise_u_s));
        return c;
    }
    // An escort keeps its station by its bombers until you come near them.
    if (p.duty == Duty::Escort) {
        float fx = 0.0f;
        float fy = 0.0f;
        float fz = 0.0f;
        float course = 180.0f;
        if (!formation_of(world, p.raid, fx, fy, fz, course)) {
            p.duty = Duty::Hunt; // nothing left to guard
        } else {
            const float away = hunting ? std::hypot(target.x - fx, target.y - fy) : 1e9f;
            p.engaged = p.engaged ? away <= a.escort_return_u : away < a.escort_break_u;
            if (!p.engaged) {
                const float h = course * kToRad;
                const float tx =
                    fx + std::sin(h) * p.station_ahead_u + std::cos(h) * p.station_right_u;
                const float ty =
                    fy + std::cos(h) * p.station_ahead_u - std::sin(h) * p.station_right_u;
                const float dx = tx - self.x;
                const float dy = ty - self.y;
                const float along = dx * std::sin(h) + dy * std::cos(h);
                const float want = along > 20.0f    ? plane.flight.max_level_u_s
                                   : along < -20.0f ? p.cruise_u_s * 0.8f
                                                    : p.cruise_u_s;
                Wish w{std::hypot(dx, dy) > 100.0f ? bearing(dx, dy) : course,
                       climb_pitch(fz + 20.0f - self.z), false, false};
                p.doing = Phase::Guard;
                dodge(p, world, rules, self, w);
                keep_flying(p, self, plane, a, w);
                command(w, throttle_for(self.speed_u_s, want));
                return c;
            }
        }
    }

    // A look at the target now and then; between looks, it's where it was heading.
    p.look_s -= kTickSeconds;
    p.seen_age_s += kTickSeconds;
    p.phase_s -= kTickSeconds;
    if (p.look_s <= 0.0f) {
        p.look_s = p.skill.reaction_s;
        p.seen_age_s = 0.0f;
        const float h = target.heading_deg * kToRad;
        const float pitch = target.pitch_deg * kToRad;
        const float level = target.speed_u_s * std::cos(pitch);
        p.seen_x = target.x;
        p.seen_y = target.y;
        p.seen_z = target.z;
        p.seen_vx = std::sin(h) * level;
        p.seen_vy = std::cos(h) * level;
        p.seen_vz =
            target.speed_u_s * std::sin(pitch) * plane_of(rules, target).flight.height_share;
        p.aim_off_deg = (rng.next_float() * 2.0f - 1.0f) * p.skill.aim_error_deg;
        p.aim_off_pitch_deg = (rng.next_float() * 2.0f - 1.0f) * p.skill.aim_error_deg * 0.5f;
    }
    const float tx = p.seen_x + p.seen_vx * p.seen_age_s;
    const float ty = p.seen_y + p.seen_vy * p.seen_age_s;
    const float tz = p.seen_z + p.seen_vz * p.seen_age_s;
    const float d = std::hypot(tx - self.x, ty - self.y);
    const float dz = tz - self.z;

    // Lead: where the target will be when a round gets there.
    const float round_speed =
        (plane.gun_count > 0
             ? rules.weapon.calibres[static_cast<std::size_t>(plane.guns[0].calibre)].speed_u_s
             : 360.0f) +
        self.speed_u_s;
    const float flight_s = d / round_speed;
    const float lx = tx + p.seen_vx * flight_s;
    const float ly = ty + p.seen_vy * flight_s;
    const float lz = tz + p.seen_vz * flight_s;
    const float ld = std::max(1.0f, std::hypot(lx - self.x, ly - self.y));
    const float to_lead = bearing(lx - self.x, ly - self.y);
    const float aim_heading = to_lead + p.aim_off_deg;
    const float aim_pitch = std::atan2(lz - self.z, ld) * kToDeg + p.aim_off_pitch_deg;
    const float nose_off = std::abs(wrap180(to_lead - self.heading_deg));

    Wish w{self.heading_deg, 0.0f, false, false};

    // Gone past each other: the target is behind, near, and not pointing this way (a plane on this
    // one's tail is to be turned against, not flown away from).
    const bool chased =
        std::abs(wrap180(bearing(self.x - tx, self.y - ty) - target.heading_deg)) < 60.0f;
    const bool passed = nose_off > 110.0f && d < 200.0f && !chased;
    const bool close = d < a.dogfight_u;
    // Once a fight is joined there's no climbing away to set up again: that walks it up the sky.
    p.fighting = close || (p.fighting && d < a.dogfight_u * kPartedAt);
    // An attack ends after a pass, or if it never closes.
    const auto attack_over = [&] { return passed || (p.phase_s <= 0.0f && !close); };
    const auto attack = [&] {
        p.phase = Phase::Attack;
        p.phase_s = kAttackS;
        p.set_up = false;
    };

    if (!hunting) {
        w.heading = self.heading_deg + 30.0f; // circle
    } else if (p.style == Style::Turn) {
        p.phase = Phase::Pursue;
        // How far off its tail this plane is: aim behind it until it's there.
        const float off_tail =
            std::abs(wrap180(bearing(self.x - tx, self.y - ty) - (target.heading_deg + 180.0f)));
        if (off_tail > 70.0f && d > 60.0f) {
            const float h = target.heading_deg * kToRad;
            w.heading =
                bearing(tx - std::sin(h) * 90.0f - self.x, ty - std::cos(h) * 90.0f - self.y);
        } else {
            w.heading = aim_heading;
        }
        w.pitch = d < a.fire_range_u && std::abs(dz) < 40.0f ? aim_pitch : climb_pitch(dz);
        w.shoot = true;
        w.slow = off_tail < 60.0f && d < 250.0f &&
                 self.speed_u_s > target.speed_u_s + std::max(0.0f, d - 70.0f) * 0.3f;
    } else if (p.style == Style::DiveClimb) {
        if (p.phase == Phase::Climb) {
            // From a distance, climb toward the target to a height fixed now, then dive on it.
            if (!p.set_up) {
                p.set_up = true;
                p.phase_s = a.climb_max_s;
                p.climb_to_u = std::min(plane.flight.ceiling_u - 10.0f, tz + a.dive_height_u);
            }
            w.heading = aim_heading;
            w.pitch = climb_pitch(p.climb_to_u - self.z);
            w.shoot = true;
            if (p.fighting || p.phase_s <= 0.0f || (self.z >= p.climb_to_u - 15.0f && d < 450.0f)) {
                attack();
            }
        } else if (p.phase == Phase::Attack) {
            w.heading = aim_heading;
            w.pitch = aim_pitch;
            w.shoot = true;
            if (attack_over()) {
                p.phase = Phase::Extend;
                p.phase_s = a.extend_s;
            }
        } else {
            p.phase = Phase::Extend;
            w.pitch = 0.0f;
            w.shoot = true;
            if (p.phase_s <= 0.0f) {
                p.phase = Phase::Climb;
            }
        }
    } else { // head-on
        if (p.phase == Phase::Line) {
            if (!p.set_up) {
                p.set_up = true;
                p.phase_s = a.climb_max_s;
            }
            const float h = target.heading_deg * kToRad;
            const float ax = tx + std::sin(h) * a.head_on_run_u;
            const float ay = ty + std::cos(h) * a.head_on_run_u;
            w.heading = bearing(ax - self.x, ay - self.y);
            w.pitch = climb_pitch(dz);
            w.shoot = true;
            const float off_nose =
                std::abs(wrap180(bearing(self.x - tx, self.y - ty) - target.heading_deg));
            if (p.fighting || p.phase_s <= 0.0f ||
                (off_nose < 35.0f && d > 150.0f && d < a.head_on_run_u * 1.3f) ||
                std::hypot(ax - self.x, ay - self.y) < 80.0f) {
                attack();
            }
        } else if (p.phase == Phase::Attack) {
            w.heading = aim_heading;
            w.pitch = d < a.fire_range_u ? aim_pitch : climb_pitch(dz);
            w.shoot = true;
            if (attack_over()) {
                p.phase = Phase::Extend;
                p.phase_s = a.extend_s;
            }
        } else {
            p.phase = Phase::Extend;
            w.pitch = 0.0f;
            w.shoot = true;
            if (p.phase_s <= 0.0f) {
                p.phase = Phase::Line;
            }
        }
    }
    p.doing = p.phase;
    dodge(p, world, rules, self, w);

    // Other enemies: don't fly into each other (bombers are bigger).
    for (const Pilot& other : world.pilots) {
        const Body& o = world.bodies[static_cast<std::size_t>(other.body)];
        if (other.body == p.body || o.outcome != Outcome::Flying) {
            continue;
        }
        const float clear = 25.0f * plane_type(o.type).size;
        if (std::hypot(o.x - self.x, o.y - self.y) < clear && std::abs(o.z - self.z) < 8.0f) {
            const float away = wrap180(bearing(o.x - self.x, o.y - self.y) - self.heading_deg);
            w.heading = self.heading_deg + (away > 0.0f ? -60.0f : 60.0f);
            w.shoot = false;
            p.doing = Phase::Break;
            break;
        }
    }

    // Stay inside the battle area.
    const auto width = static_cast<float>(kMapWidthU);
    const auto depth = static_cast<float>(kMapDepthU);
    if (self.x < a.edge_margin_u || self.x > width - a.edge_margin_u || self.y < a.edge_margin_u ||
        self.y > depth - a.edge_margin_u) {
        w.heading = bearing(width * 0.5f - self.x, depth * 0.5f - self.y);
        w.shoot = false;
        p.doing = Phase::Return;
    }

    // Never into the ground or a stall.
    keep_flying(p, self, plane, a, w);
    command(w, to_axis(w.slow ? -1.0f : 1.0f));
    c.guns = w.shoot && hunting && d < a.fire_range_u &&
             std::abs(wrap180(aim_heading - self.heading_deg)) < a.fire_cone_deg &&
             std::abs(aim_pitch - self.pitch_deg) < a.fire_cone_deg &&
             !friend_in_line(world, p, self, d);
    return c;
}

} // namespace wof
