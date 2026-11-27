#include "core/gunners.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/ai.hpp"
#include "core/world.hpp"

namespace wof {
namespace {

constexpr float kToDeg = 180.0f / std::numbers::pi_v<float>;
constexpr float kToRad = std::numbers::pi_v<float> / 180.0f;

[[nodiscard]] float wrap180(float deg) noexcept {
    float w = std::fmod(deg + 180.0f, 360.0f);
    if (w < 0.0f) {
        w += 360.0f;
    }
    return w - 180.0f;
}

[[nodiscard]] bool is_piloted(const World& w, std::size_t body) noexcept {
    return std::any_of(w.pilots.begin(), w.pilots.end(),
                       [body](const Pilot& p) { return p.body == static_cast<int>(body); });
}

// Another enemy plane between the gun and the target: a gunner holds fire rather than hit it.
[[nodiscard]] bool friend_in_way(const World& w, const Rules& rules, std::size_t self, float tx,
                                 float ty, float tz) noexcept {
    const Body& b = w.bodies[self];
    const float dx = tx - b.x;
    const float dy = ty - b.y;
    const float len2 = dx * dx + dy * dy;
    for (const Pilot& other : w.pilots) {
        const auto i = static_cast<std::size_t>(other.body);
        const Body& o = w.bodies[i];
        if (i == self || o.outcome != Outcome::Flying || len2 <= 0.0f) {
            continue;
        }
        const float t = ((o.x - b.x) * dx + (o.y - b.y) * dy) / len2;
        if (t <= 0.0f || t >= 1.0f) {
            continue;
        }
        const float reach = rules.weapon.hit_radius_u * plane_type(o.type).size;
        if (std::hypot(b.x + dx * t - o.x, b.y + dy * t - o.y) <= reach &&
            std::abs(b.z + (tz - b.z) * t - o.z) <= rules.weapon.band_forgiving_u) {
            return true;
        }
    }
    return false;
}

} // namespace

bool in_gunner_arc(int gunner, float heading_deg, float dx, float dy, float dz,
                   const GunnerParams& p) noexcept {
    if (dx * dx + dy * dy + dz * dz > p.range_u * p.range_u) {
        return false;
    }
    const float off = std::abs(wrap180(std::atan2(dx, dy) * kToDeg - heading_deg));
    const float off_tail = 180.0f - off;
    switch (gunner) {
    case 0:
        return off <= p.nose_arc_deg;
    case 1:
        return off_tail <= p.dorsal_arc_deg && dz >= -p.level_u;
    case 2:
        return off_tail <= p.ventral_arc_deg && dz <= p.level_u;
    default:
        return false;
    }
}

void fire_gunners(World& w, std::size_t body, Pilot& pilot, const Rules& rules) {
    const PlaneRules& plane = plane_of(rules, w.bodies[body]);
    const GunnerParams& p = rules.gunner;
    for (int g = 0; g < std::min(plane.gun_count, kGunSlots); ++g) {
        const GunSpec& spec = plane.guns[static_cast<std::size_t>(g)];
        if (spec.mount == Mount::Fixed) {
            continue;
        }
        Body& self = w.bodies[body];
        GunState& gun = self.loadout.guns[static_cast<std::size_t>(g)];
        const int slot = static_cast<int>(spec.mount) - static_cast<int>(Mount::Nose);
        GunnerState& s = pilot.gunners[static_cast<std::size_t>(slot)];
        if (gunner_out(self, slot, plane.damage)) {
            continue;
        }
        gun.wait_s -= kTickSeconds;
        s.burst_s -= kTickSeconds;
        if (s.burst_s <= 0.0f) {
            s.firing = !s.firing;
            s.burst_s += std::max(kTickSeconds, s.firing ? p.burst_s : p.pause_s);
        }

        // The nearest player in the arc.
        int target = -1;
        float nearest = 0.0f;
        for (std::size_t j = 0; j < w.bodies.size(); ++j) {
            const Body& o = w.bodies[j];
            if (j == body || o.outcome != Outcome::Flying || is_piloted(w, j)) {
                continue;
            }
            const float dx = o.x - self.x;
            const float dy = o.y - self.y;
            const float dz = o.z - self.z;
            const float d = dx * dx + dy * dy + dz * dz;
            if (in_gunner_arc(slot, self.heading_deg, dx, dy, dz, p) &&
                (target < 0 || d < nearest)) {
                target = static_cast<int>(j);
                nearest = d;
            }
        }
        if (target < 0) {
            gun.wait_s = std::max(0.0f, gun.wait_s);
            s.look_s = 0.0f; // the next target is looked at afresh
            continue;
        }
        const Body& o = w.bodies[static_cast<std::size_t>(target)];

        // A look now and then; between looks the target is where it was heading.
        s.look_s -= kTickSeconds;
        s.seen_age_s += kTickSeconds;
        if (s.look_s <= 0.0f) {
            s.look_s = pilot.skill.reaction_s;
            s.seen_age_s = 0.0f;
            const float h = o.heading_deg * kToRad;
            const float pitch = o.pitch_deg * kToRad;
            s.seen_x = o.x;
            s.seen_y = o.y;
            s.seen_z = o.z;
            s.seen_vx = std::sin(h) * std::cos(pitch) * o.speed_u_s;
            s.seen_vy = std::cos(h) * std::cos(pitch) * o.speed_u_s;
            s.seen_vz = o.speed_u_s * std::sin(pitch) * plane_of(rules, o).flight.height_share;
            s.off_deg = (w.rng.next_float() * 2.0f - 1.0f) * pilot.skill.aim_error_deg;
            s.off_pitch_deg = (w.rng.next_float() * 2.0f - 1.0f) * pilot.skill.aim_error_deg * 0.5f;
        }
        if (!s.firing || gun.wait_s > 0.0f || gun.rounds <= 0) {
            gun.wait_s = std::max(0.0f, gun.wait_s);
            continue;
        }

        const CalibreParams& cal = rules.weapon.calibres[static_cast<std::size_t>(spec.calibre)];
        const float speed = std::max(1.0f, cal.speed_u_s);
        // Where the target will be when a round gets there.
        const float now_x = s.seen_x + s.seen_vx * s.seen_age_s;
        const float now_y = s.seen_y + s.seen_vy * s.seen_age_s;
        const float now_z = s.seen_z + s.seen_vz * s.seen_age_s;
        const float flight =
            std::sqrt((now_x - self.x) * (now_x - self.x) + (now_y - self.y) * (now_y - self.y) +
                      (now_z - self.z) * (now_z - self.z)) /
            speed;
        const float ax = now_x + s.seen_vx * flight;
        const float ay = now_y + s.seen_vy * flight;
        const float az = now_z + s.seen_vz * flight;
        if (friend_in_way(w, rules, body, ax, ay, az)) {
            continue;
        }
        const float across = std::max(1.0f, std::hypot(ax - self.x, ay - self.y));
        const float heading = (std::atan2(ax - self.x, ay - self.y) * kToDeg + s.off_deg) * kToRad;
        const float elevation = std::atan2(az - self.z, across) + s.off_pitch_deg * kToRad;
        const float vx = std::sin(heading) * std::cos(elevation);
        const float vy = std::cos(heading) * std::cos(elevation);
        const float vz = std::sin(elevation);
        Round r;
        r.x = self.x + vx * rules.weapon.muzzle_u;
        r.y = self.y + vy * rules.weapon.muzzle_u;
        r.z = self.z + vz * rules.weapon.muzzle_u;
        r.vx = vx * speed;
        r.vy = vy * speed;
        r.vz = vz * speed;
        r.life_s = cal.life_s;
        r.owner = static_cast<int>(body);
        r.tracer = rules.weapon.tracer_every > 0 && gun.fired % rules.weapon.tracer_every == 0;
        r.damage = cal.damage;
        w.rounds.push_back(r);
        --gun.rounds;
        ++gun.fired;
        gun.wait_s += 60.0f / std::max(1.0f, spec.rpm);
    }
}

} // namespace wof
