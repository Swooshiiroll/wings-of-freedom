#include "core/weapons.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

#include "core/iso.hpp"
#include "core/world.hpp"

namespace wof {
namespace {

constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
constexpr int kMaxBombSteps = 60 * 60; // the bomb marker gives up after a minute of falling

struct Vec {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Along the nose, pitch included.
[[nodiscard]] Vec nose_of(const Body& b) noexcept {
    const float h = b.heading_deg * kDegToRad;
    const float p = b.pitch_deg * kDegToRad;
    return {std::sin(h) * std::cos(p), std::cos(h) * std::cos(p), std::sin(p)};
}

[[nodiscard]] int ticks_of(float seconds) noexcept {
    return std::max(1,
                    static_cast<int>(std::lround(seconds * static_cast<float>(kTicksPerSecond))));
}

[[nodiscard]] float band_of(const WeaponParams& p, HeightRule r) noexcept {
    return r == HeightRule::Strict ? p.band_strict_u : p.band_forgiving_u;
}

// The closest a path (x0, y0) to (x1, y1) comes to a point on the ground plane: how far along the
// path (0..1), and the distance there.
struct Approach {
    float t = 0.0f;
    float dist = 0.0f;
};
[[nodiscard]] Approach closest(float x0, float y0, float x1, float y1, float px,
                               float py) noexcept {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len2 = dx * dx + dy * dy;
    const float t =
        len2 > 0.0f ? std::clamp(((px - x0) * dx + (py - y0) * dy) / len2, 0.0f, 1.0f) : 0.0f;
    return {t, std::hypot(x0 + dx * t - px, y0 + dy * t - py)};
}

// Under auto-aim: sets a new round's climb toward the nearest plane or drone within the cone off
// the nose and the height window, if there is one in range.
void auto_aim(const World& w, std::size_t shooter, const WeaponParams& p, Round& r) {
    const Body& b = w.bodies[shooter];
    const float speed_across = std::hypot(r.vx, r.vy);
    const float range = speed_across * r.life_s;
    float best = range;
    float aim_z = 0.0f;
    bool found = false;
    const auto consider = [&](float tx, float ty, float tz) {
        const float dx = tx - b.x;
        const float dy = ty - b.y;
        const float across = std::hypot(dx, dy);
        if (across < 1.0f || across >= best || std::abs(tz - b.z) > p.autoaim_height_u) {
            return;
        }
        float off = std::atan2(dx, dy) / kDegToRad - b.heading_deg;
        off = std::fmod(off + 540.0f, 360.0f) - 180.0f;
        if (std::abs(off) > p.autoaim_cone_deg) {
            return;
        }
        best = across;
        aim_z = tz;
        found = true;
    };
    for (std::size_t j = 0; j < w.bodies.size(); ++j) {
        const Body& o = w.bodies[j];
        if (j != shooter && o.outcome == Outcome::Flying) {
            consider(o.x, o.y, o.z);
        }
    }
    for (const Target& t : w.targets) {
        if (t.kind == TargetKind::Drone) {
            consider(t.x, t.y, t.z);
        }
    }
    if (found && speed_across > 0.0f) {
        r.vz = (aim_z - r.z) / (best / speed_across);
    }
}

void burst(World& w, const Rules& rules, float gx, float gy, int owner, const Blast& blast) {
    w.events.push_back(Event{EventKind::Blast, -1, owner, gx, gy, 0.0f, blast.radius_u});
    for (std::size_t j = 0; j < w.bodies.size(); ++j) {
        Body& b = w.bodies[j];
        if (b.outcome != Outcome::Flying) {
            continue;
        }
        const float d = std::sqrt((b.x - gx) * (b.x - gx) + (b.y - gy) * (b.y - gy) + b.z * b.z);
        const float damage = blast_damage_at(blast, d);
        if (damage > 0.0f) {
            // Blasts land like a flak burst's hits (design/flight.md, Where hits land).
            const DamageParams& dp = plane_of(rules, b).damage;
            const Part part = pick_part(b, HitFrom::Burst, dp, w.rng);
            apply_hit(b, part, damage, dp);
            Event e{EventKind::BlastHitPlane, static_cast<int>(j), owner, b.x, b.y, b.z, damage};
            e.part = static_cast<std::int8_t>(part);
            w.events.push_back(e);
        }
    }
    for (std::size_t k = 0; k < w.targets.size(); ++k) {
        Target& t = w.targets[k];
        const float across = std::hypot(t.x - gx, t.y - gy);
        const float d = t.kind == TargetKind::Drone ? std::hypot(across, t.z)
                                                    : across - rules.targets.ground_radius_u;
        if (d < blast.radius_u) {
            hit_target(t);
            w.events.push_back(Event{EventKind::BlastHitTarget, static_cast<int>(k), owner, t.x,
                                     t.y, t.z, blast_damage_at(blast, std::max(0.0f, d))});
        }
    }
}

} // namespace

const char* height_rule_name(HeightRule r) noexcept {
    switch (r) {
    case HeightRule::Forgiving:
        return "forgiving";
    case HeightRule::Strict:
        return "strict";
    case HeightRule::AutoAim:
        return "auto";
    }
    return "?";
}

bool parse_height_rule(std::string_view text, HeightRule& out) noexcept {
    for (int i = 0; i < kHeightRules; ++i) {
        const auto r = static_cast<HeightRule>(i);
        if (text == height_rule_name(r)) {
            out = r;
            return true;
        }
    }
    return false;
}

void arm(Body& b, const PlaneRules& plane) noexcept {
    b.loadout = Loadout{};
    for (int g = 0; g < std::min(plane.gun_count, kGunSlots); ++g) {
        b.loadout.guns[static_cast<std::size_t>(g)].rounds =
            plane.guns[static_cast<std::size_t>(g)].rounds;
    }
    b.loadout.bombs = plane.bombs;
}

Blast blast_of(float weight_lb, const WeaponParams& p) noexcept {
    const float k = std::cbrt(std::max(0.0f, weight_lb) / 100.0f);
    return {p.blast_radius_u * k, p.blast_damage * k};
}

float blast_damage_at(const Blast& blast, float distance) noexcept {
    if (blast.radius_u <= 0.0f || distance >= blast.radius_u) {
        return 0.0f;
    }
    return blast.damage * (1.0f - std::max(0.0f, distance) / blast.radius_u);
}

int rounds_left(const Body& b, const PlaneRules& plane, Calibre c) noexcept {
    int n = 0;
    for (int g = 0; g < std::min(plane.gun_count, kGunSlots); ++g) {
        const auto i = static_cast<std::size_t>(g);
        if (plane.guns[i].mount == Mount::Fixed && plane.guns[i].calibre == c) {
            n += b.loadout.guns[i].rounds;
        }
    }
    return n;
}

namespace {

// A bomb as it leaves the plane: just below it, with the plane's velocity (its real up or down
// speed, so the climb's height share too).
Bomb release(const Body& b, const WeaponParams& p, float height_share, int owner) noexcept {
    const Vec nose = nose_of(b);
    return Bomb{b.x,
                b.y,
                b.z - p.bomb_drop_u,
                nose.x * b.speed_u_s,
                nose.y * b.speed_u_s,
                nose.z * b.speed_u_s * height_share,
                owner};
}

// One tick of a bomb's fall. True when it reaches the ground, with (gx, gy) where.
bool fall(Bomb& m, float gravity, float& gx, float& gy) noexcept {
    m.vz -= gravity * kTickSeconds;
    const float x1 = m.x + m.vx * kTickSeconds;
    const float y1 = m.y + m.vy * kTickSeconds;
    const float z1 = m.z + m.vz * kTickSeconds;
    if (z1 <= 0.0f) {
        const float t = m.z > 0.0f ? m.z / (m.z - z1) : 0.0f;
        gx = m.x + (x1 - m.x) * t;
        gy = m.y + (y1 - m.y) * t;
        return true;
    }
    m.x = x1;
    m.y = y1;
    m.z = z1;
    return false;
}

} // namespace

GroundSpot bomb_impact(const Body& b, const WeaponParams& p, const FlightParams& flight) noexcept {
    Bomb m = release(b, p, flight.height_share, -1);
    GroundSpot spot{m.x, m.y};
    for (int i = 0; i < kMaxBombSteps; ++i) {
        if (fall(m, flight.gravity, spot.x, spot.y)) {
            return spot;
        }
    }
    return {m.x, m.y};
}

void fire_weapons(World& w, std::size_t body, const InputCommand& cmd, const Rules& rules) {
    const WeaponParams& p = rules.weapon;
    const PlaneRules& plane = plane_of(rules, w.bodies[body]);
    Body& b = w.bodies[body];
    Loadout& l = b.loadout;
    if (cmd.switch_heavy) {
        l.heavy = Heavy::Bombs; // the only heavy weapon so far: nothing else to switch to
    }

    // Guns: each of the pilot's fixed guns fires at its own real rate while F is held.
    const Vec nose = nose_of(b);
    const float h = b.heading_deg * kDegToRad;
    for (int g = 0; g < std::min(plane.gun_count, kGunSlots); ++g) {
        const GunSpec& spec = plane.guns[static_cast<std::size_t>(g)];
        GunState& gun = l.guns[static_cast<std::size_t>(g)];
        if (spec.mount != Mount::Fixed) {
            continue; // a gunner's gun (the bombers', PR 6)
        }
        // The time between shots carries over from tick to tick, so the gun keeps its exact rate;
        // while it isn't firing, it can't save shots up.
        gun.wait_s -= kTickSeconds;
        if (!cmd.guns || gun.rounds <= 0) {
            gun.wait_s = std::max(0.0f, gun.wait_s);
            continue;
        }
        if (gun.wait_s > 0.0f) {
            continue;
        }
        const CalibreParams& cal = p.calibres[static_cast<std::size_t>(spec.calibre)];
        const float speed = cal.speed_u_s + b.speed_u_s;
        Round r;
        r.x = b.x + nose.x * p.muzzle_u + std::cos(h) * spec.side_u;
        r.y = b.y + nose.y * p.muzzle_u - std::sin(h) * spec.side_u;
        r.z = b.z + nose.z * p.muzzle_u;
        r.vx = nose.x * speed;
        r.vy = nose.y * speed;
        r.vz = nose.z * speed;
        r.life_s = cal.life_s;
        r.owner = static_cast<int>(body);
        r.tracer = p.tracer_every > 0 && gun.fired % p.tracer_every == 0;
        r.damage = cal.damage;
        if (rules.height_rule == HeightRule::AutoAim) {
            auto_aim(w, body, p, r);
        }
        w.rounds.push_back(r);
        --gun.rounds;
        ++gun.fired;
        gun.wait_s += 60.0f / std::max(1.0f, spec.rpm);
    }

    // Bombs: one now, then one every bomb_interval_s while Space is held (a ripple). Each bomb's
    // blast is by its real weight.
    if (l.bomb_cooldown > 0) {
        --l.bomb_cooldown;
    }
    if (cmd.heavy && l.heavy == Heavy::Bombs && l.bombs > 0 && l.bomb_cooldown == 0) {
        Bomb m = release(b, p, plane.flight.height_share, static_cast<int>(body));
        const Blast blast = blast_of(plane.bomb_lb, p);
        m.radius_u = blast.radius_u;
        m.damage = blast.damage;
        w.bombs.push_back(m);
        --l.bombs;
        l.bomb_cooldown = ticks_of(p.bomb_interval_s);
    }
}

void move_projectiles(World& w, const Rules& rules) {
    const WeaponParams& p = rules.weapon;
    const float band = band_of(p, rules.height_rule);

    std::vector<Round> rounds;
    rounds.reserve(w.rounds.size());
    for (Round r : w.rounds) {
        const float x1 = r.x + r.vx * kTickSeconds;
        const float y1 = r.y + r.vy * kTickSeconds;
        const float z1 = r.z + r.vz * kTickSeconds;
        r.life_s -= kTickSeconds;

        // The first plane or drone along this tick's path, within the hit radius on the ground
        // and the height band there.
        float best_t = 2.0f;
        int hit_body = -1;
        int hit_drone = -1;
        const auto test = [&](float tx, float ty, float tz) {
            const Approach a = closest(r.x, r.y, x1, y1, tx, ty);
            const float z_there = r.z + (z1 - r.z) * a.t;
            return a.dist <= p.hit_radius_u && std::abs(z_there - tz) <= band && a.t < best_t
                       ? a.t
                       : -1.0f;
        };
        for (std::size_t j = 0; j < w.bodies.size(); ++j) {
            const Body& o = w.bodies[j];
            if (static_cast<int>(j) == r.owner || o.outcome != Outcome::Flying) {
                continue; // a plane's own rounds never hit it
            }
            if (const float t = test(o.x, o.y, o.z); t >= 0.0f) {
                best_t = t;
                hit_body = static_cast<int>(j);
                hit_drone = -1;
            }
        }
        for (std::size_t k = 0; k < w.targets.size(); ++k) {
            const Target& t = w.targets[k];
            if (t.kind != TargetKind::Drone) {
                continue;
            }
            if (const float at = test(t.x, t.y, t.z); at >= 0.0f) {
                best_t = at;
                hit_drone = static_cast<int>(k);
                hit_body = -1;
            }
        }
        const float hx = r.x + (x1 - r.x) * best_t;
        const float hy = r.y + (y1 - r.y) * best_t;
        const float hz = r.z + (z1 - r.z) * best_t;
        if (hit_body >= 0) {
            // The part it lands on depends on where it came from: back along its path.
            Body& target = w.bodies[static_cast<std::size_t>(hit_body)];
            const float damage = r.damage;
            const DamageParams& dp = plane_of(rules, target).damage;
            const Part part = pick_part(target, hit_from(target, -r.vx, -r.vy), dp, w.rng);
            apply_hit(target, part, damage, dp);
            Event e{EventKind::RoundHitPlane, hit_body, r.owner, hx, hy, hz, damage};
            e.part = static_cast<std::int8_t>(part);
            w.events.push_back(e);
            continue;
        }
        if (hit_drone >= 0) {
            hit_target(w.targets[static_cast<std::size_t>(hit_drone)]);
            w.events.push_back(
                Event{EventKind::RoundHitTarget, hit_drone, r.owner, hx, hy, hz, r.damage});
            continue;
        }

        // The ground: an impact, and strafing any ground target close by.
        if (z1 <= 0.0f) {
            const float t = r.z > 0.0f ? r.z / (r.z - z1) : 0.0f;
            const float gx = r.x + (x1 - r.x) * t;
            const float gy = r.y + (y1 - r.y) * t;
            w.events.push_back(Event{EventKind::RoundGround, -1, r.owner, gx, gy, 0.0f, 0.0f});
            int nearest = -1;
            float nearest_d = 0.0f;
            for (std::size_t k = 0; k < w.targets.size(); ++k) {
                const Target& g = w.targets[k];
                if (g.kind != TargetKind::Ground) {
                    continue;
                }
                const float d = std::hypot(g.x - gx, g.y - gy);
                if (d <= rules.targets.ground_radius_u + p.strafe_radius_u &&
                    (nearest < 0 || d < nearest_d)) {
                    nearest = static_cast<int>(k);
                    nearest_d = d;
                }
            }
            if (nearest >= 0) {
                hit_target(w.targets[static_cast<std::size_t>(nearest)]);
                w.events.push_back(
                    Event{EventKind::RoundHitTarget, nearest, r.owner, gx, gy, 0.0f, r.damage});
            }
            continue;
        }
        if (r.life_s <= 0.0f) {
            continue;
        }
        r.x = x1;
        r.y = y1;
        r.z = z1;
        rounds.push_back(r);
    }
    w.rounds = std::move(rounds);

    std::vector<Bomb> bombs;
    bombs.reserve(w.bombs.size());
    for (Bomb m : w.bombs) {
        float gx = 0.0f;
        float gy = 0.0f;
        if (fall(m, rules.flight.gravity, gx, gy)) {
            burst(w, rules, gx, gy, m.owner, Blast{m.radius_u, m.damage});
        } else {
            bombs.push_back(m);
        }
    }
    w.bombs = std::move(bombs);
}

} // namespace wof
