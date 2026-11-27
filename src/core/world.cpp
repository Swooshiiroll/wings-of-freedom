#include "core/world.hpp"

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
    if (b.y < 0.0f) {
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

Body new_plane(const Rules& rules, float x, float y, float heading_deg) noexcept {
    Body b = start_body(rules.flight, x, y, heading_deg);
    arm(b, rules.weapon);
    repair(b, rules.damage);
    return b;
}

namespace {

// Spreads damage evenly over a plane's five HP parts (collisions).
void spread(Body& b, float damage, const DamageParams& d) noexcept {
    const float each = damage / 5.0f;
    for (const Part p : {Part::Engine, Part::LeftWing, Part::RightWing, Part::Fuselage}) {
        apply_hit(b, p, each, d);
    }
    apply_hit(b, Part::Tail, each, d);
}

void destroy(Body& b) noexcept {
    b.outcome = Outcome::ShotDown;
    b.engine_on = false;
}

} // namespace

void collide(World& w, const Rules& rules) {
    const DamageParams& d = rules.damage;
    for (std::size_t i = 0; i < w.bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < w.bodies.size(); ++j) {
            Body& a = w.bodies[i];
            Body& b = w.bodies[j];
            if (a.outcome != Outcome::Flying || b.outcome != Outcome::Flying) {
                continue;
            }
            const float across = std::hypot(a.x - b.x, a.y - b.y);
            if (across >= 2.0f * d.collision_radius_u ||
                std::abs(a.z - b.z) > d.collision_height_u) {
                continue;
            }
            // The weaker plane is destroyed; the other takes what the weaker one had left.
            const float ta = total_hp(a);
            const float tb = total_hp(b);
            int destroyed = -1;
            if (ta < tb) {
                destroy(a);
                spread(b, ta, d);
                destroyed = static_cast<int>(i);
            } else if (tb < ta) {
                destroy(b);
                spread(a, tb, d);
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
        const Part p = pick_part(b, from, rules.damage, w.rng);
        apply_hit(b, p, damage, rules.damage);
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

void step(World& world, std::span<const InputCommand> commands, const Rules& rules) {
    world.events.clear();
    move_targets(world.targets, rules.targets, kTickSeconds);
    const Aero aero = aero_of(rules.flight);
    for (std::size_t i = 0; i < world.bodies.size(); ++i) {
        Body& b = world.bodies[i];
        if (b.outcome != Outcome::Flying) {
            continue;
        }
        const InputCommand cmd = i < commands.size() ? commands[i] : InputCommand{};
        const Handling h = handling_of(b, rules.damage);
        if (h.top_speed_share < 1.0f) {
            // A damaged engine: a lower top speed, and the thrust (so acceleration) to match.
            FlightParams weaker = rules.flight;
            weaker.max_level_u_s *= h.top_speed_share;
            fly(b, cmd, rules.flight, aero_of(weaker), kTickSeconds, h);
        } else {
            fly(b, cmd, rules.flight, aero, kTickSeconds, h);
        }
        if (!b.damage.going_down) {
            fire_weapons(world, i, cmd, rules);
        }
        apply_edges(world.bodies[i], rules.edge, kTickSeconds);
    }
    move_projectiles(world, rules);
    collide(world, rules);
    ++world.tick;
}

} // namespace wof
