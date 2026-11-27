#include "core/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "core/map.hpp"

namespace wof {

void apply_edges(Body& b, const EdgeParams& e, float dt) noexcept {
    if (b.outcome != Outcome::Flying) {
        return;
    }
    if (b.z <= 0.0f) {
        b.z = 0.0f;
        // A plane already going down ends as what sent it down.
        b.outcome = b.damage.going_down ? b.damage.cause : Outcome::Crashed;
        return;
    }
    if (b.y < 0.0f && !b.damage.going_down) { // falling over the line isn't getting home
        b.outcome = Outcome::ReturnedHome;
        b.edge_warning = false;
        b.edge_countdown_s = -1.0f;
        return;
    }
    const auto w = static_cast<float>(kMapWidthU);
    const auto d = static_cast<float>(kMapDepthU);
    const bool outside = b.x < 0.0f || b.x > w || b.y > d;
    b.edge_warning =
        !outside && (b.x < e.warning_u || b.x > w - e.warning_u || b.y > d - e.warning_u);
    if (!outside) {
        b.edge_countdown_s = -1.0f;
        return;
    }
    if (b.edge_countdown_s < 0.0f) {
        b.edge_countdown_s = e.countdown_s;
    }
    b.edge_countdown_s -= dt;
    if (b.edge_countdown_s <= 0.0f) {
        b.edge_countdown_s = 0.0f;
        b.outcome = Outcome::LeftArea;
    }
}

const PlaneRules& plane_of(const Rules& rules, const Body& b) noexcept {
    static const PlaneRules kFallback =
        derive_plane(plane_type(PlaneId::P26A), FlightParams{}, DamageParams{}, PlaneScale{});
    if (rules.planes.empty()) {
        return kFallback;
    }
    if (b.fit >= 0 && static_cast<std::size_t>(b.fit) < rules.fitted.size()) {
        const PlaneRules& fitted = rules.fitted[static_cast<std::size_t>(b.fit)];
        if (static_cast<std::uint8_t>(fitted.id) == b.type) {
            return fitted;
        }
    }
    return rules.planes[std::min<std::size_t>(b.type, rules.planes.size() - 1)];
}

Body new_plane(const Rules& rules, PlaneId type, float x, float y, float heading_deg,
               int fit) noexcept {
    Body b;
    b.type = static_cast<std::uint8_t>(type);
    b.fit = static_cast<std::int8_t>(fit);
    const PlaneRules& plane = plane_of(rules, b);
    b = start_body(plane.flight, x, y, heading_deg);
    b.type = static_cast<std::uint8_t>(type);
    b.fit = static_cast<std::int8_t>(fit);
    arm(b, plane);
    repair(b, plane.damage);
    return b;
}

namespace {

// Spreads damage evenly over a plane's five HP parts (collisions).
void spread(Body& b, float damage, const DamageParams& d) noexcept {
    const bool twin = b.damage.engines > 1;
    const float each = damage / (twin ? 6.0f : 5.0f);
    for (const Part p : {Part::Engine, Part::LeftWing, Part::RightWing, Part::Fuselage}) {
        apply_hit(b, p, each, d);
    }
    apply_hit(b, Part::Tail, each, d);
    if (twin) {
        apply_hit(b, Part::Engine2, each, d);
    }
}

void destroy(Body& b) noexcept {
    b.outcome = Outcome::ShotDown;
    b.engine_on = false;
}

} // namespace

void collide(World& w, const Rules& rules) {
    const DamageParams& d = rules.damage; // the shared collision numbers
    for (std::size_t i = 0; i < w.bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < w.bodies.size(); ++j) {
            Body& a = w.bodies[i];
            Body& b = w.bodies[j];
            if (a.outcome != Outcome::Flying || b.outcome != Outcome::Flying) {
                continue;
            }
            const float across = std::hypot(a.x - b.x, a.y - b.y);
            const float reach =
                d.collision_radius_u * (plane_type(a.type).size + plane_type(b.type).size);
            if (across >= reach || std::abs(a.z - b.z) > d.collision_height_u) {
                continue;
            }
            // The weaker plane is destroyed; the other takes what the weaker one had left.
            const float ta = total_hp(a);
            const float tb = total_hp(b);
            int destroyed = -1;
            if (ta < tb) {
                destroy(a);
                spread(b, ta, plane_of(rules, b).damage);
                destroyed = static_cast<int>(i);
            } else if (tb < ta) {
                destroy(b);
                spread(a, tb, plane_of(rules, a).damage);
                destroyed = static_cast<int>(j);
            } else {
                destroy(a);
                destroy(b);
            }
            w.events.push_back(
                Event{EventKind::Collision, destroyed,
                      destroyed == static_cast<int>(i) ? static_cast<int>(j) : static_cast<int>(i),
                      (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f, 0.0f});
        }
    }
}

void debug_hits(World& w, std::size_t body, HitFrom from, float damage, int count,
                const Rules& rules) {
    Body& b = w.bodies[body];
    for (int i = 0; i < count && b.outcome == Outcome::Flying; ++i) {
        const DamageParams& dp = plane_of(rules, b).damage;
        const Part p = pick_part(b, from, dp, w.rng);
        apply_hit(b, p, damage, dp);
        Event e{from == HitFrom::Burst ? EventKind::BlastHitPlane : EventKind::RoundHitPlane,
                static_cast<int>(body),
                -1,
                b.x,
                b.y,
                b.z,
                damage};
        e.part = static_cast<std::int8_t>(p);
        w.events.push_back(e);
    }
}

namespace {

// The pilot flying body `i`, or nullptr (a player's).
Pilot* pilot_of(World& w, std::size_t i) noexcept {
    for (Pilot& p : w.pilots) {
        if (p.body == static_cast<int>(i)) {
            return &p;
        }
    }
    return nullptr;
}

// Enemy fighters never run out (design/flight.md, Enemies): their guns and fuel are topped up.
void top_up(Body& b, const PlaneRules& plane) noexcept {
    for (int g = 0; g < plane.gun_count && g < kGunSlots; ++g) {
        const auto i = static_cast<std::size_t>(g);
        b.loadout.guns[i].rounds = plane.guns[i].rounds;
    }
    b.fuel = 1.0f;
}

// A kill is an enemy going down (or gone) after the player damaged or rammed it.
void credit_kills(World& w) {
    for (const Event& e : w.events) {
        int enemy = -1;
        if ((e.kind == EventKind::RoundHitPlane || e.kind == EventKind::BlastHitPlane) &&
            e.by == 0) {
            enemy = e.who;
        } else if (e.kind == EventKind::Collision && (e.who == 0 || e.by == 0)) {
            enemy = e.who == 0 ? e.by : e.who;
        }
        if (enemy > 0) {
            if (Pilot* p = pilot_of(w, static_cast<std::size_t>(enemy)); p != nullptr) {
                p->hurt = true;
            }
        }
    }
    for (Pilot& p : w.pilots) {
        const Body& b = w.bodies[static_cast<std::size_t>(p.body)];
        const bool lost = b.damage.going_down || b.outcome == Outcome::Crashed ||
                          b.outcome == Outcome::ShotDown || b.outcome == Outcome::PilotKilled;
        if (p.hurt && !p.counted && lost) {
            p.counted = true;
            ++w.waves.kills;
            w.events.push_back(Event{EventKind::EnemyDown, p.body, 0, b.x, b.y, b.z, 0.0f});
        }
    }
}

// A bomber that leaves by the south edge has reached your base.
void count_through(World& w) {
    for (Pilot& p : w.pilots) {
        const Body& b = w.bodies[static_cast<std::size_t>(p.body)];
        if (p.duty == Duty::Bomb && !p.through && b.outcome == Outcome::ReturnedHome) {
            p.through = true;
            ++w.waves.through;
            w.events.push_back(Event{EventKind::BomberThrough, p.body, -1, b.x, b.y, b.z, 0.0f});
        }
    }
}

} // namespace

void step(World& world, std::span<const InputCommand> commands, const Rules& rules) {
    world.events.clear();
    update_waves(world, rules);
    update_flak(world, rules);
    move_targets(world.targets, rules.targets, kTickSeconds);
    for (std::size_t i = 0; i < world.bodies.size(); ++i) {
        Body& b = world.bodies[i];
        if (b.outcome != Outcome::Flying) {
            continue;
        }
        // Each plane flies by its own type's rules, on its player's command or its AI pilot's.
        const PlaneRules& plane = plane_of(rules, b);
        Pilot* pilot = pilot_of(world, i);
        const InputCommand cmd = pilot != nullptr      ? ai_command(*pilot, world, rules, world.rng)
                                 : i < commands.size() ? commands[i]
                                                       : InputCommand{};
        const Handling h = handling_of(b, plane.damage);
        if (h.top_speed_share < 1.0f) {
            // A damaged engine: a lower top speed, and the thrust (so acceleration) to match.
            FlightParams weaker = plane.flight;
            weaker.max_level_u_s *= h.top_speed_share;
            fly(b, cmd, plane.flight, aero_of(weaker), kTickSeconds, h);
        } else {
            fly(b, cmd, plane.flight, plane.aero, kTickSeconds, h);
        }
        if (!b.damage.going_down) {
            fire_weapons(world, i, cmd, rules);
            if (pilot != nullptr && plane.damage.gunners > 0) {
                fire_gunners(world, i, *pilot, rules);
            }
        }
        if (pilot != nullptr) {
            top_up(world.bodies[i], plane);
        }
        apply_edges(world.bodies[i], rules.edge, kTickSeconds);
    }
    move_projectiles(world, rules);
    collide(world, rules);
    credit_kills(world);
    count_through(world);
    ++world.tick;
}

} // namespace wof
