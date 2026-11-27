#include "core/damage.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/world.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {
World with_plane(const Rules& r, float z = 150.0f) {
    World w;
    Body b = new_plane(r, PlaneId::P26A, 1440.0f, 2400.0f, 0.0f);
    b.z = z;
    b.speed_u_s = 150.0f;
    b.throttle = 1.0f;
    w.bodies.push_back(b);
    return w;
}

void run(World& w, const InputCommand& c, int ticks, const Rules& r) {
    for (int i = 0; i < ticks; ++i) {
        step(w, std::vector<InputCommand>{c, c}, r);
    }
}

InputCommand turning() {
    InputCommand c;
    c.turn = 127;
    return c;
}

// HP plus the pilot's damage: everything a hit can take away.
float everything(const Body& b) {
    return total_hp(b) - b.damage.pilot;
}
} // namespace

TEST_CASE("Stages: OK above 75%, WARNING to 50%, DANGER to 25%, CRITICAL below, then destroyed") {
    CHECK(stage_of(40.0f, 40.0f) == Stage::Ok);
    CHECK(stage_of(30.4f, 40.0f) == Stage::Ok);
    CHECK(stage_of(30.0f, 40.0f) == Stage::Warning);
    CHECK(stage_of(20.0f, 40.0f) == Stage::Danger);
    CHECK(stage_of(10.4f, 40.0f) == Stage::Danger);
    CHECK(stage_of(10.0f, 40.0f) == Stage::Critical);
    CHECK(stage_of(0.5f, 40.0f) == Stage::Critical);
    CHECK(stage_of(0.0f, 40.0f) == Stage::Destroyed);
    CHECK(pilot_stage(0, 3) == Stage::Ok);
    CHECK(pilot_stage(1, 3) == Stage::Warning);
    CHECK(pilot_stage(2, 3) == Stage::Critical);
    CHECK(pilot_stage(3, 3) == Stage::Destroyed);
}

TEST_CASE("A damaged engine lowers the top speed: at half HP, about 80% (120 u/s)") {
    Rules r;
    World w = with_plane(r);
    w.bodies[0].damage.engine = 20.0f;
    run(w, turning(), 90 * kTicksPerSecond, r); // circling, so it stays over the map
    CHECK(w.bodies[0].outcome == Outcome::Flying);
    CHECK(w.bodies[0].speed_u_s == doctest::Approx(120.0f).epsilon(0.025));
}

TEST_CASE("A destroyed engine stops, and the plane glides") {
    const Rules r;
    World w = with_plane(r);
    apply_hit(w.bodies[0], Part::Engine, 40.0f, r.damage);
    CHECK_FALSE(w.bodies[0].engine_on);
    CHECK(part_stage(w.bodies[0], Part::Engine, r.damage) == Stage::Destroyed);
    run(w, turning(), 10 * kTicksPerSecond, r);
    CHECK(w.bodies[0].z < 150.0f);
    CHECK_FALSE(w.bodies[0].engine_on);
}

TEST_CASE("Damaged wings slow the turn, to 60% at 1 HP, and the weaker wing sets it") {
    const DamageParams d;
    Body b;
    repair(b, d);
    b.damage.left_wing = 1.0f;
    CHECK(handling_of(b, d).turn_share == doctest::Approx(0.61f).epsilon(0.001));
    b.damage.left_wing = 20.0f;
    b.damage.right_wing = 30.0f;
    CHECK(handling_of(b, d).turn_share == doctest::Approx(0.8f));
    b.damage.left_wing = 40.0f;
    b.damage.right_wing = 40.0f;
    CHECK(handling_of(b, d).turn_share == doctest::Approx(1.0f));
    CHECK(handling_of(b, d).top_speed_share == doctest::Approx(1.0f));
    CHECK(handling_of(b, d).leak_per_s == doctest::Approx(0.0f));
}

TEST_CASE("A damaged fuselage leaks, up to a full tank in 60 s at 1 HP") {
    const DamageParams d;
    Body b;
    repair(b, d);
    b.damage.fuselage = 1.0f;
    CHECK(1.0f / handling_of(b, d).leak_per_s == doctest::Approx(61.0f).epsilon(0.01));
    b.damage.fuselage = 30.0f;
    CHECK(handling_of(b, d).leak_per_s == doctest::Approx(0.5f / 60.0f));

    // In flight it drains the tank much faster than the engine alone.
    Rules r;
    World leaky = with_plane(r);
    World sound = with_plane(r);
    leaky.bodies[0].damage.fuselage = 1.0f;
    run(leaky, turning(), 30 * kTicksPerSecond, r);
    run(sound, turning(), 30 * kTicksPerSecond, r);
    CHECK(sound.bodies[0].fuel - leaky.bodies[0].fuel ==
          doctest::Approx(30.0f / 61.0f).epsilon(0.03));
}

TEST_CASE("A destroyed fuselage loses all the fuel; then the tail takes the hits, and losing it is "
          "going down") {
    const DamageParams d;
    Body b;
    repair(b, d);
    b.fuel = 0.8f;
    apply_hit(b, Part::Fuselage, 55.0f, d);
    CHECK(b.fuel == doctest::Approx(0.8f));
    apply_hit(b, Part::Fuselage, 10.0f, d); // 5 over: to the tail
    CHECK(b.damage.fuselage == doctest::Approx(0.0f));
    CHECK(b.fuel == doctest::Approx(0.0f));
    CHECK_FALSE(b.engine_on);
    CHECK(b.damage.tail == doctest::Approx(15.0f));
    CHECK_FALSE(b.damage.going_down);
    Rng rng(1);
    CHECK(pick_part(b, HitFrom::Behind, d, rng) != Part::Fuselage); // fuselage hits go to the tail
    apply_hit(b, Part::Fuselage, 15.0f, d);
    CHECK(b.damage.tail == doctest::Approx(0.0f));
    CHECK(b.damage.going_down);
    CHECK(b.damage.cause == Outcome::ShotDown);
    CHECK((missing_parts(b) & part_bit(Part::Tail)) != 0u);
}

TEST_CASE("A lost wing sends the plane spinning down out of control, and it ends as shot down") {
    const Rules r;
    World a = with_plane(r, 100.0f);
    apply_hit(a.bodies[0], Part::LeftWing, 40.0f, r.damage);
    CHECK(a.bodies[0].damage.going_down);
    CHECK(a.bodies[0].damage.spin < 0.0f); // toward the lost wing
    CHECK(missing_parts(a.bodies[0]) == part_bit(Part::LeftWing));
    World b = a;
    // The controls do nothing now: whatever's pressed, it falls the same way.
    InputCommand pull;
    pull.pitch = 127;
    pull.turn = 127;
    pull.guns = true;
    run(a, InputCommand{}, 60, r);
    run(b, pull, 60, r);
    CHECK(a.bodies[0] == b.bodies[0]);
    CHECK(b.rounds.empty());
    CHECK(a.bodies[0].pitch_deg < -20.0f);
    CHECK(a.bodies[0].heading_deg > 90.0f); // spun left past west
    run(a, InputCommand{}, 30 * kTicksPerSecond, r);
    CHECK(a.bodies[0].outcome == Outcome::ShotDown);
}

TEST_CASE("Every 5 damage on the pilot is a wound, and the third kills") {
    const Rules r;
    World w = with_plane(r, 60.0f);
    Body& b = w.bodies[0];
    apply_hit(b, Part::Cockpit, 4.9f, r.damage);
    CHECK(b.damage.wounds == 0);
    apply_hit(b, Part::Cockpit, 0.2f, r.damage);
    CHECK(b.damage.wounds == 1);
    CHECK(part_stage(b, Part::Cockpit, r.damage) == Stage::Warning);
    apply_hit(b, Part::Cockpit, 5.0f, r.damage);
    CHECK(part_stage(b, Part::Cockpit, r.damage) == Stage::Critical);
    CHECK_FALSE(b.damage.going_down);
    apply_hit(b, Part::Cockpit, 5.0f, r.damage);
    CHECK(b.damage.wounds == 3);
    CHECK(b.damage.going_down);
    run(w, InputCommand{}, 30 * kTicksPerSecond, r);
    CHECK(w.bodies[0].outcome == Outcome::PilotKilled);
}

TEST_CASE("Where a hit comes from, relative to the nose") {
    Body b;
    b.heading_deg = 0.0f;
    CHECK(hit_from(b, 0.0f, 1.0f) == HitFrom::Ahead);
    CHECK(hit_from(b, 0.9f, 1.0f) == HitFrom::Ahead);
    CHECK(hit_from(b, 0.0f, -1.0f) == HitFrom::Behind);
    CHECK(hit_from(b, 1.0f, 0.0f) == HitFrom::Right);
    CHECK(hit_from(b, -1.0f, 0.2f) == HitFrom::Left);
    b.heading_deg = 90.0f;
    CHECK(hit_from(b, 1.0f, 0.0f) == HitFrom::Ahead);
    CHECK(hit_from(b, 0.0f, 1.0f) == HitFrom::Left);
}

TEST_CASE("Hits land by the table: shares hold for each direction, and side hits take that wing") {
    const DamageParams d;
    Body b;
    repair(b, d);
    Rng rng(42);
    constexpr int kDraws = 20000;
    const std::array<HitFrom, 4> froms = {HitFrom::Ahead, HitFrom::Behind, HitFrom::Left,
                                          HitFrom::Burst};
    const std::array<std::size_t, 4> rows = {0, 1, 2, 3};
    for (std::size_t k = 0; k < froms.size(); ++k) {
        std::array<int, kParts> counts{};
        for (int i = 0; i < kDraws; ++i) {
            ++counts[static_cast<std::size_t>(pick_part(b, froms[k], d, rng))];
        }
        const auto pct = [&](Part p) {
            return 100.0f * static_cast<float>(counts[static_cast<std::size_t>(p)]) / kDraws;
        };
        const auto& row = d.table[rows[k]];
        CAPTURE(k);
        CHECK(std::abs(pct(Part::Engine) - static_cast<float>(row[0])) < 1.5f);
        CHECK(std::abs(pct(Part::Cockpit) - static_cast<float>(row[1])) < 1.5f);
        CHECK(std::abs(pct(Part::LeftWing) + pct(Part::RightWing) - static_cast<float>(row[2])) <
              1.5f);
        CHECK(std::abs(pct(Part::Fuselage) - static_cast<float>(row[3])) < 1.5f);
        if (froms[k] == HitFrom::Left) {
            CHECK(counts[static_cast<std::size_t>(Part::RightWing)] == 0);
        } else {
            CHECK(std::abs(pct(Part::LeftWing) - pct(Part::RightWing)) < 1.5f);
        }
    }
}

TEST_CASE("Another plane's rounds do their calibre's damage to the parts they hit") {
    const Rules r;
    World w = with_plane(r, 100.0f);
    Body target = w.bodies[0];
    target.y += 150.0f;
    target.speed_u_s = 150.0f;
    w.bodies.push_back(target);
    const float before = everything(w.bodies[1]);
    InputCommand fire;
    fire.guns = true;
    float dealt = 0.0f;
    for (int i = 0; i < 40; ++i) {
        step(w, std::vector<InputCommand>{fire, InputCommand{}}, r);
        for (const Event& e : w.events) {
            if (e.kind == EventKind::RoundHitPlane) {
                // The P-26A's .30 does 1, its .50 2.5 (design/planes.md).
                CHECK((e.damage == doctest::Approx(1.0f) || e.damage == doctest::Approx(2.5f)));
                dealt += e.damage;
            }
        }
    }
    const float lost = before - everything(w.bodies[1]);
    CHECK(lost > 0.0f);
    CHECK(lost == doctest::Approx(dealt));
    CHECK(everything(w.bodies[0]) == doctest::Approx(before)); // the shooter is untouched
}

TEST_CASE("Your own low bomb takes about 20 from you") {
    const Rules r;
    World w = with_plane(r, 10.0f);
    const float before = everything(w.bodies[0]);
    InputCommand drop;
    drop.heavy = true;
    step(w, std::vector<InputCommand>{drop}, r);
    for (int i = 0; i < 60; ++i) {
        step(w, std::vector<InputCommand>{InputCommand{}}, r);
    }
    CHECK(before - everything(w.bodies[0]) == doctest::Approx(20.0f).epsilon(0.15));
}

TEST_CASE("Collisions: the weaker plane is destroyed, the other takes what it had left") {
    const Rules r;
    World w = with_plane(r, 100.0f);
    Body other = w.bodies[0];
    other.x += 5.0f;
    other.z += 2.0f;
    other.damage.engine = 0.0f;
    other.damage.left_wing = 10.0f;
    other.damage.right_wing = 10.0f;
    other.damage.fuselage = 20.0f;
    other.damage.tail = 20.0f; // 60 in all
    w.bodies.push_back(other);
    collide(w, r);
    CHECK(w.bodies[1].outcome == Outcome::ShotDown);
    CHECK(w.bodies[0].outcome == Outcome::Flying);
    CHECK(total_hp(w.bodies[0]) == doctest::Approx(200.0f - 60.0f));
    CHECK(w.bodies[0].damage.engine == doctest::Approx(28.0f)); // 12 on each part
    REQUIRE(w.events.size() == 1u);
    CHECK(w.events[0].kind == EventKind::Collision);
    CHECK(w.events[0].who == 1);
}

TEST_CASE("Equal planes destroy each other; planes apart in height or across don't touch") {
    const Rules r;
    World same = with_plane(r, 100.0f);
    same.bodies.push_back(same.bodies[0]);
    same.bodies[1].x += 3.0f;
    collide(same, r);
    CHECK(same.bodies[0].outcome == Outcome::ShotDown);
    CHECK(same.bodies[1].outcome == Outcome::ShotDown);

    World high = with_plane(r, 100.0f);
    high.bodies.push_back(high.bodies[0]);
    high.bodies[1].z += 7.0f;
    collide(high, r);
    CHECK(high.bodies[1].outcome == Outcome::Flying);

    World wide = with_plane(r, 100.0f);
    wide.bodies.push_back(wide.bodies[0]);
    wide.bodies[1].x += 15.0f;
    collide(wide, r);
    CHECK(wide.bodies[1].outcome == Outcome::Flying);
}

TEST_CASE("Repair mends everything and restarts the engine if there's fuel") {
    const DamageParams d;
    Body b;
    repair(b, d);
    apply_hit(b, Part::Engine, 40.0f, d);
    apply_hit(b, Part::RightWing, 40.0f, d);
    b.fuel = 0.5f;
    repair(b, d);
    CHECK(b.engine_on);
    CHECK_FALSE(b.damage.going_down);
    CHECK(total_hp(b) == doctest::Approx(200.0f));
    CHECK(missing_parts(b) == 0u);
}

TEST_CASE("The same commands and seed give exactly the same damage") {
    const Rules r;
    const auto make = [&] {
        World w = with_plane(r, 100.0f);
        Body target = w.bodies[0];
        target.y += 120.0f;
        w.bodies.push_back(target);
        w.rng = Rng(7);
        return w;
    };
    World a = make();
    World b = make();
    InputCommand fire;
    fire.guns = true;
    for (int i = 0; i < 300; ++i) {
        step(a, std::vector<InputCommand>{fire, InputCommand{}}, r);
        step(b, std::vector<InputCommand>{fire, InputCommand{}}, r);
    }
    CHECK(a.bodies == b.bodies);
    CHECK(a.bodies[1].damage != Damage{});
}
