#include "core/flak.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "core/map.hpp"
#include "core/progress.hpp"
#include "core/world.hpp"

namespace wof {
namespace {

constexpr float kToDeg = 180.0f / std::numbers::pi_v<float>;
constexpr float kToRad = std::numbers::pi_v<float> / 180.0f;

// By nation_slot: the US, then Germany. Rates and reaches from design/planes.md.
constexpr std::array<std::array<FlakStats, kFlakKinds>, 2> kStats = {{
    {{{".50 cal AA", 500.0f, 2.5f, 100.0f, 180.0f, 20.0f, 380.0f, 0.0f},
      {"37 mm M1", 90.0f, 14.0f, 210.0f, 240.0f, 30.0f, 340.0f, 0.0f},
      {"3-inch M3", 25.0f, 13.0f, 560.0f, 400.0f, 45.0f, 400.0f, 15.0f}}},
    {{{"MG 34", 900.0f, 1.0f, 60.0f, 180.0f, 20.0f, 360.0f, 0.0f},
      {"2 cm Flak 30", 120.0f, 8.0f, 70.0f, 240.0f, 30.0f, 340.0f, 0.0f},
      {"8.8 cm Flak 18", 18.0f, 14.0f, 520.0f, 400.0f, 45.0f, 400.0f, 16.0f}}},
}};

[[nodiscard]] bool piloted(const World& w, std::size_t body) noexcept {
    return std::any_of(w.pilots.begin(), w.pilots.end(),
                       [body](const Pilot& p) { return p.body == static_cast<int>(body); });
}

void add_gun(FlakField& f, FlakKind kind, const FlakPlace& at, float spread, Rng& rng) {
    const float a = rng.next_float() * 2.0f * std::numbers::pi_v<float>;
    const float d = std::sqrt(rng.next_float()) * spread;
    FlakGun g;
    g.kind = kind;
    g.x = at.x + std::sin(a) * d;
    g.y = at.y + std::cos(a) * d;
    g.hp = flak_stats(f.nation, kind).hp;
    f.guns.push_back(g);
}

// What a new wave brings: cannon swapped in, heavy guns, and now and then one more gun.
void apply_wave(FlakField& f, const FlakParams& p, int n, Rng& rng) {
    if (n == p.cannon_wave) {
        for (FlakGun& g : f.guns) {
            if (g.alive && g.kind == FlakKind::MachineGun) {
                g.kind = FlakKind::Cannon;
                g.hp = flak_stats(f.nation, FlakKind::Cannon).hp;
            }
        }
    }
    if (n == p.heavy_wave) {
        for (const FlakPlace& at : f.places) {
            if (at.site || at.strong) {
                add_gun(f, FlakKind::Heavy, at, p.spread_u, rng);
            }
        }
    }
    if (p.reinforce_every > 0 && n % p.reinforce_every == 0 && f.reinforced < p.reinforce_max &&
        !f.places.empty()) {
        const auto at = rng.next_below(static_cast<std::uint32_t>(f.places.size()));
        add_gun(f, n >= p.cannon_wave ? FlakKind::Cannon : FlakKind::MachineGun, f.places[at],
                p.spread_u, rng);
        ++f.reinforced;
    }
}

// Another enemy plane between the gun and its aim: it holds fire.
[[nodiscard]] bool friend_in_way(const World& w, const Rules& rules, float gx, float gy, float tx,
                                 float ty, float tz) noexcept {
    const float dx = tx - gx;
    const float dy = ty - gy;
    const float len2 = dx * dx + dy * dy;
    for (const Pilot& other : w.pilots) {
        const Body& o = w.bodies[static_cast<std::size_t>(other.body)];
        if (o.outcome != Outcome::Flying || len2 <= 0.0f) {
            continue;
        }
        const float t = ((o.x - gx) * dx + (o.y - gy) * dy) / len2;
        if (t <= 0.0f || t >= 1.0f) {
            continue;
        }
        if (std::hypot(gx + dx * t - o.x, gy + dy * t - o.y) <=
                rules.weapon.hit_radius_u * plane_type(o.type).size &&
            std::abs(tz * t - o.z) <= rules.weapon.band_forgiving_u) {
            return true;
        }
    }
    return false;
}

void fire(World& w, const Rules& rules, std::size_t index) {
    FlakGun& g = w.flak.guns[index];
    const FlakStats& s = flak_stats(w.flak.nation, g.kind);
    const FlakParams& p = rules.flak;
    g.wait_s -= kTickSeconds;

    // The nearest player in range and low enough.
    int target = -1;
    float nearest = s.range_u;
    for (std::size_t j = 0; j < w.bodies.size(); ++j) {
        const Body& o = w.bodies[j];
        if (o.outcome != Outcome::Flying || o.z > s.reach_u || piloted(w, j)) {
            continue;
        }
        const float d = std::hypot(o.x - g.x, o.y - g.y);
        if (d <= nearest) {
            nearest = d;
            target = static_cast<int>(j);
        }
    }
    if (target < 0) {
        g.wait_s = std::max(0.0f, g.wait_s);
        g.look_s = 0.0f;
        return;
    }
    const Body& o = w.bodies[static_cast<std::size_t>(target)];
    const Skill skill = skill_of(rules.ai, std::max(1, w.waves.current));

    // A look now and then; between looks the target is where it was heading.
    g.look_s -= kTickSeconds;
    g.seen_age_s += kTickSeconds;
    if (g.look_s <= 0.0f) {
        g.look_s = skill.reaction_s;
        g.seen_age_s = 0.0f;
        const float h = o.heading_deg * kToRad;
        const float pitch = o.pitch_deg * kToRad;
        g.seen_x = o.x;
        g.seen_y = o.y;
        g.seen_z = o.z;
        g.seen_vx = std::sin(h) * std::cos(pitch) * o.speed_u_s;
        g.seen_vy = std::cos(h) * std::cos(pitch) * o.speed_u_s;
        g.seen_vz = o.speed_u_s * std::sin(pitch) * plane_of(rules, o).flight.height_share;
        g.off_deg = (w.rng.next_float() * 2.0f - 1.0f) * skill.aim_error_deg;
        g.off_pitch_deg = (w.rng.next_float() * 2.0f - 1.0f) * skill.aim_error_deg * 0.5f;
    }
    if (g.wait_s > 0.0f) {
        return;
    }

    const float now_x = g.seen_x + g.seen_vx * g.seen_age_s;
    const float now_y = g.seen_y + g.seen_vy * g.seen_age_s;
    const float now_z = g.seen_z + g.seen_vz * g.seen_age_s;
    const float speed = std::max(1.0f, s.speed_u_s);
    const float flight =
        std::sqrt((now_x - g.x) * (now_x - g.x) + (now_y - g.y) * (now_y - g.y) + now_z * now_z) /
        speed;
    float ax = now_x + g.seen_vx * flight;
    float ay = now_y + g.seen_vy * flight;
    float az = std::max(1.0f, now_z + g.seen_vz * flight);
    if (friend_in_way(w, rules, g.x, g.y, ax, ay, az)) {
        return;
    }
    if (g.kind == FlakKind::Heavy) {
        // A timed shell, scattered: worse the higher you are.
        const float scatter = p.scatter_u + p.scatter_per_u * az;
        float ox = 0.0f;
        float oy = 0.0f;
        float oz = 0.0f;
        do {
            ox = w.rng.next_float() * 2.0f - 1.0f;
            oy = w.rng.next_float() * 2.0f - 1.0f;
            oz = w.rng.next_float() * 2.0f - 1.0f;
        } while (ox * ox + oy * oy + oz * oz > 1.0f);
        w.flak.shells.push_back(FlakShell{ax + ox * scatter, ay + oy * scatter,
                                          std::max(1.0f, az + oz * scatter), flight, s.damage,
                                          s.burst_u, static_cast<int>(index)});
    } else {
        const float across = std::max(1.0f, std::hypot(ax - g.x, ay - g.y));
        const float heading = (std::atan2(ax - g.x, ay - g.y) * kToDeg + g.off_deg) * kToRad;
        const float elevation = std::atan2(az - 1.0f, across) + g.off_pitch_deg * kToRad;
        Round r;
        r.x = g.x;
        r.y = g.y;
        r.z = 1.0f;
        r.vx = std::sin(heading) * std::cos(elevation) * speed;
        r.vy = std::cos(heading) * std::cos(elevation) * speed;
        r.vz = std::sin(elevation) * speed;
        r.life_s = s.range_u * 1.5f / speed;
        r.owner = -2 - static_cast<int>(index); // no plane's
        r.tracer = true;                        // streams you can see
        r.damage = s.damage;
        w.rounds.push_back(r);
    }
    g.wait_s += 60.0f / std::max(1.0f, s.rpm);
    g.since_fired_s = 0.0f;
}

// A heavy burst: any plane near it, friend or foe, takes it like a bomb's blast.
void burst(World& w, const Rules& rules, const FlakShell& shell) {
    w.events.push_back(
        Event{EventKind::FlakBurst, shell.gun, -1, shell.x, shell.y, shell.z, shell.radius_u});
    const Blast blast{shell.radius_u, shell.damage};
    for (std::size_t j = 0; j < w.bodies.size(); ++j) {
        Body& b = w.bodies[j];
        if (b.outcome != Outcome::Flying) {
            continue;
        }
        const float d =
            std::sqrt((b.x - shell.x) * (b.x - shell.x) + (b.y - shell.y) * (b.y - shell.y) +
                      (b.z - shell.z) * (b.z - shell.z));
        const float damage = blast_damage_at(blast, d);
        if (damage <= 0.0f) {
            continue;
        }
        const DamageParams& dp = plane_of(rules, b).damage;
        const Part part = pick_part(b, HitFrom::Burst, dp, w.rng);
        apply_hit(b, part, damage, dp);
        Event e{
            EventKind::BlastHitPlane, static_cast<int>(j), -2 - shell.gun, b.x, b.y, b.z, damage};
        e.part = static_cast<std::int8_t>(part);
        w.events.push_back(e);
    }
}

} // namespace

const FlakStats& flak_stats(Nation n, FlakKind k) noexcept {
    return kStats[static_cast<std::size_t>(nation_slot(n))][static_cast<std::size_t>(k)];
}

FlakField place_flak(Nation enemy, const FlakParams& p, Rng& rng) {
    FlakField f;
    f.nation = enemy;
    const auto width = static_cast<float>(kMapWidthU);
    const auto front = static_cast<float>(kFrontU);
    const int groups = std::max(0, p.front_groups);
    // The front line: a group in each stretch of it, a little off the line either way.
    for (int i = 0; i < groups; ++i) {
        const float stretch = width / static_cast<float>(groups);
        FlakPlace at;
        at.x = (static_cast<float>(i) + 0.25f + rng.next_float() * 0.5f) * stretch;
        at.y = front + (rng.next_float() - 0.5f) * 200.0f;
        f.places.push_back(at);
    }
    for (int s = 0, made = 0; made < std::min(p.strongpoints, groups) && s < 1000; ++s) {
        FlakPlace& at = f.places[rng.next_below(static_cast<std::uint32_t>(groups))];
        if (!at.strong) {
            at.strong = true;
            ++made;
        }
    }
    // The sites beyond it.
    for (int i = 0; i < p.sites; ++i) {
        FlakPlace at;
        at.site = true;
        at.x = 600.0f + rng.next_float() * (width - 1200.0f);
        at.y =
            front + p.site_near_u + rng.next_float() * std::max(0.0f, p.site_far_u - p.site_near_u);
        at.y = std::min(at.y, static_cast<float>(kMapDepthU) - 600.0f);
        f.places.push_back(at);
    }
    for (const FlakPlace& at : f.places) {
        const int count = at.site ? p.guns_per_site : p.guns_per_group;
        for (int i = 0; i < count; ++i) {
            add_gun(f, FlakKind::MachineGun, at, p.spread_u, rng);
        }
    }
    return f;
}

void update_flak(World& w, const Rules& rules) {
    FlakField& f = w.flak;
    if (f.places.empty()) {
        return;
    }
    while (f.last_wave < w.waves.current) {
        apply_wave(f, rules.flak, ++f.last_wave, w.rng);
    }
    for (std::size_t i = 0; i < f.guns.size(); ++i) {
        f.guns[i].since_fired_s += kTickSeconds;
        if (f.guns[i].alive) {
            fire(w, rules, i);
        }
    }
    std::vector<FlakShell> flying;
    flying.reserve(f.shells.size());
    for (FlakShell shell : f.shells) {
        shell.fuse_s -= kTickSeconds;
        if (shell.fuse_s <= 0.0f) {
            burst(w, rules, shell);
        } else {
            flying.push_back(shell);
        }
    }
    f.shells = std::move(flying);
}

void hit_flak(World& w, std::size_t gun, float damage, int by) {
    FlakGun& g = w.flak.guns[gun];
    if (!g.alive) {
        return;
    }
    g.hp -= damage;
    if (g.hp > 0.0f) {
        return;
    }
    g.alive = false;
    ++w.flak.destroyed;
    // damage: 1 for a heavy gun, 0 a light one (what it's worth).
    w.events.push_back(Event{EventKind::FlakDown, static_cast<int>(gun), by, g.x, g.y, 0.0f,
                             g.kind == FlakKind::Heavy ? 1.0f : 0.0f});
}

} // namespace wof
