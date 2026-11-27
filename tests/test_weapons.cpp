#include "core/weapons.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/world.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {
InputCommand fire(bool guns, bool bombs = false, int pitch = 0) {
    InputCommand c;
    c.guns = guns;
    c.heavy = bombs;
    c.pitch = static_cast<std::int8_t>(pitch);
    return c;
}

// One armed plane, level at full throttle (its steady speed, 150 u/s), heading north.
World one_plane(const Rules& r, float z = 100.0f) {
    World w;
    Body b = start_body(r.flight, 1440.0f, 2000.0f, 0.0f);
    b.z = z;
    b.speed_u_s = 150.0f;
    b.throttle = 1.0f;
    arm(b, r.weapon);
    w.bodies.push_back(b);
    return w;
}

Target drone_at(float x, float y, float z) {
    Target t;
    t.kind = TargetKind::Drone;
    t.cx = x;
    t.cy = y;
    t.x = x;
    t.y = y;
    t.z = z;
    return t; // orbit 0: it stays put
}

Target ground_at(float x, float y) {
    Target t;
    t.kind = TargetKind::Ground;
    t.cx = x;
    t.cy = y;
    t.x = x;
    t.y = y;
    return t;
}

void run(World& w, const InputCommand& c, int ticks, const Rules& r) {
    for (int i = 0; i < ticks; ++i) {
        step(w, std::vector<InputCommand>{c}, r);
    }
}

// Steps until something of `kind` happens (or `ticks` run out); the first such event.
bool run_until(World& w, const InputCommand& c, int ticks, const Rules& r, EventKind kind,
               Event& out) {
    for (int i = 0; i < ticks; ++i) {
        step(w, std::vector<InputCommand>{c}, r);
        for (const Event& e : w.events) {
            if (e.kind == kind) {
                out = e;
                return true;
            }
        }
    }
    return false;
}

// Fires one round from a level plane at a still drone straight ahead, `dz` above or below.
int hits_on_drone_ahead(HeightRule rule, float dz, float side = 0.0f) {
    Rules r;
    r.height_rule = rule;
    World w = one_plane(r);
    w.targets.push_back(drone_at(1440.0f + side, 2150.0f, 100.0f + dz));
    run(w, fire(true), 1, r);
    run(w, fire(false), 50, r);
    return w.targets[0].hits;
}
} // namespace

TEST_CASE("Holding F fires 12 rounds a second from the two guns in turn") {
    const Rules r;
    World w = one_plane(r);
    run(w, fire(true), 1, r);
    REQUIRE(w.rounds.size() == 1u);
    CHECK(w.rounds[0].x < 1440.0f); // the left gun first
    run(w, fire(true), 5, r);
    REQUIRE(w.rounds.size() == 2u);
    CHECK(w.rounds[1].x > 1440.0f); // then the right
    run(w, fire(true), 54, r);
    CHECK(w.bodies[0].loadout.rounds_fired == 12);
    CHECK(w.bodies[0].loadout.rounds == 600 - 12);
}

TEST_CASE("The guns stop when the rounds run out") {
    const Rules r;
    World w = one_plane(r);
    w.bodies[0].loadout.rounds = 3;
    run(w, fire(true), 60, r);
    CHECK(w.bodies[0].loadout.rounds_fired == 3);
    CHECK(w.bodies[0].loadout.rounds == 0);
}

TEST_CASE("Every 3rd round is a tracer") {
    const Rules r;
    World w = one_plane(r);
    run(w, fire(true), 31, r); // 7 rounds, none expired yet
    REQUIRE(w.rounds.size() == 7u);
    int tracers = 0;
    for (std::size_t i = 0; i < w.rounds.size(); ++i) {
        CAPTURE(i);
        CHECK(w.rounds[i].tracer == (i % 3 == 0));
        tracers += w.rounds[i].tracer ? 1 : 0;
    }
    CHECK(tracers == 3);
}

TEST_CASE("Rounds fly along the nose at round speed plus the plane's, and last 0.8 s") {
    Rules r;
    World w = one_plane(r);
    w.bodies[0].pitch_deg = 20.0f;
    run(w, fire(true, false, 127), 1, r); // S held, so the nose stays up
    REQUIRE(w.rounds.size() == 1u);
    const Round& round = w.rounds[0];
    const float speed = std::sqrt(round.vx * round.vx + round.vy * round.vy + round.vz * round.vz);
    CHECK(speed == doctest::Approx(r.weapon.round_speed_u_s + w.bodies[0].speed_u_s).epsilon(0.01));
    CHECK(round.vz / speed ==
          doctest::Approx(std::sin(w.bodies[0].pitch_deg * 3.14159265f / 180.0f)).epsilon(0.01));
    run(w, fire(false), 44, r);
    CHECK(w.rounds.size() == 1u);
    run(w, fire(false), 6, r);
    CHECK(w.rounds.empty());
}

TEST_CASE("Forgiving band: hits within 10 u of the target's height, not beyond") {
    CHECK(hits_on_drone_ahead(HeightRule::Forgiving, 8.0f) == 1);
    CHECK(hits_on_drone_ahead(HeightRule::Forgiving, -8.0f) == 1);
    CHECK(hits_on_drone_ahead(HeightRule::Forgiving, 12.0f) == 0);
}

TEST_CASE("Strict: only within 3 u") {
    CHECK(hits_on_drone_ahead(HeightRule::Strict, 2.0f) == 1);
    CHECK(hits_on_drone_ahead(HeightRule::Strict, 5.0f) == 0);
}

TEST_CASE("Auto-aim angles rounds toward a target near the nose and near your height") {
    CHECK(hits_on_drone_ahead(HeightRule::AutoAim, 30.0f) == 1);
    CHECK(hits_on_drone_ahead(HeightRule::AutoAim, -30.0f) == 1);
    CHECK(hits_on_drone_ahead(HeightRule::AutoAim, 50.0f) == 0); // outside the height window
    CHECK(hits_on_drone_ahead(HeightRule::Forgiving, 30.0f) == 0);

    Rules r;
    r.height_rule = HeightRule::AutoAim;
    World inside = one_plane(r);
    inside.targets.push_back(drone_at(1440.0f + 150.0f * std::tan(0.15f), 2150.0f, 130.0f)); // ~9°
    run(inside, fire(true), 1, r);
    CHECK(inside.rounds[0].vz > 0.0f);
    World outside = one_plane(r);
    outside.targets.push_back(
        drone_at(1440.0f + 150.0f * std::tan(0.35f), 2150.0f, 130.0f)); // ~20°
    run(outside, fire(true), 1, r);
    CHECK(outside.rounds[0].vz == doctest::Approx(0.0f));
}

TEST_CASE("A fast round can't skip past a small target between ticks") {
    Rules r;
    r.weapon.hit_radius_u = 0.5f; // far less than the 8.5 u a round moves each tick
    World w = one_plane(r);
    w.targets.push_back(drone_at(1440.0f - r.weapon.gun_spacing_u, 2150.0f, 100.0f));
    run(w, fire(true), 1, r);
    run(w, fire(false), 50, r);
    CHECK(w.targets[0].hits == 1);
}

TEST_CASE("A plane's own rounds never hit it, but they hit other planes") {
    const Rules r;
    World w = one_plane(r);
    Body other = w.bodies[0];
    other.y = 2150.0f;
    other.speed_u_s = 0.0f; // hanging still for the test
    other.throttle = 0.0f;
    w.bodies.push_back(other);
    Event hit;
    REQUIRE(run_until(w, fire(true), 20, r, EventKind::RoundHitPlane, hit));
    CHECK(hit.who == 1);
    CHECK(hit.by == 0);
    CHECK(hit.damage == doctest::Approx(2.0f));
}

TEST_CASE("Rounds that reach the ground make impacts, and strafe ground targets near them") {
    const Rules r;
    World miss = one_plane(r, 30.0f);
    miss.bodies[0].pitch_deg = -30.0f;
    Event impact;
    REQUIRE(run_until(miss, fire(true, false, -127), 30, r, EventKind::RoundGround, impact));

    World strafe = one_plane(r, 30.0f);
    strafe.bodies[0].pitch_deg = -30.0f;
    strafe.targets.push_back(ground_at(impact.x + 5.0f, impact.y + 5.0f));
    run(strafe, fire(true, false, -127), 12, r);
    CHECK(strafe.targets[0].hits >= 1);
}

TEST_CASE("A bomb falls from the plane's height and speed: from 120 u at 150 u/s, ~245 u ahead") {
    const Rules r;
    World w = one_plane(r, 120.0f);
    run(w, fire(false, true), 1, r);
    const float released_at = w.bodies[0].y;
    CHECK(w.bodies[0].loadout.bombs == 3);
    Event blast;
    REQUIRE(run_until(w, fire(false), 300, r, EventKind::Blast, blast));
    CHECK(blast.y - released_at == doctest::Approx(245.0f).epsilon(0.02));
    CHECK(blast.x == doctest::Approx(1440.0f));
}

TEST_CASE("Holding Space ripples the bombs, one every 0.3 s") {
    const Rules r;
    World w = one_plane(r, 200.0f);
    run(w, fire(false, true), 1, r);
    CHECK(w.bodies[0].loadout.bombs == 3);
    run(w, fire(false, true), 17, r);
    CHECK(w.bodies[0].loadout.bombs == 3);
    run(w, fire(false, true), 1, r);
    CHECK(w.bodies[0].loadout.bombs == 2);
    run(w, fire(false, true), 41, r);
    CHECK(w.bodies[0].loadout.bombs == 0);
    // Letting go and pressing again quickly doesn't drop two at once.
    World tap = one_plane(r, 200.0f);
    run(tap, fire(false, true), 1, r);
    run(tap, fire(false), 3, r);
    run(tap, fire(false, true), 1, r);
    CHECK(tap.bodies[0].loadout.bombs == 3);
}

TEST_CASE("Blast damage falls from 40 at the centre to 0 at 20 u") {
    const WeaponParams p;
    CHECK(blast_damage_at(p, 0.0f) == doctest::Approx(40.0f));
    CHECK(blast_damage_at(p, 10.0f) == doctest::Approx(20.0f));
    CHECK(blast_damage_at(p, 20.0f) == doctest::Approx(0.0f));
    CHECK(blast_damage_at(p, 35.0f) == doctest::Approx(0.0f));
}

TEST_CASE("Bombing from low catches your own plane; from higher it doesn't") {
    const Rules r;
    World low = one_plane(r, 10.0f);
    run(low, fire(false, true), 1, r);
    Event caught;
    REQUIRE(run_until(low, fire(false), 120, r, EventKind::BlastHitPlane, caught));
    CHECK(caught.who == 0);
    CHECK(caught.by == 0);
    CHECK(caught.damage == doctest::Approx(20.0f).epsilon(0.15));

    World high = one_plane(r, 25.0f);
    run(high, fire(false, true), 1, r);
    Event none;
    CHECK_FALSE(run_until(high, fire(false), 120, r, EventKind::BlastHitPlane, none));
}

TEST_CASE(
    "The bomb marker predicts exactly where the bomb lands, and the blast hits targets there") {
    const Rules r;
    World w = one_plane(r, 120.0f);
    run(w, fire(false, true), 1, r);
    // The bomb left this tick, with the plane as it is now.
    const GroundSpot spot = bomb_impact(w.bodies[0], r.weapon, r.flight.gravity);
    w.targets.push_back(ground_at(spot.x + 15.0f, spot.y));
    w.targets.push_back(drone_at(spot.x, spot.y, 12.0f));
    w.targets.push_back(ground_at(spot.x + 60.0f, spot.y)); // too far
    Event blast;
    REQUIRE(run_until(w, fire(false), 300, r, EventKind::Blast, blast));
    CHECK(blast.x == doctest::Approx(spot.x).epsilon(1e-5));
    CHECK(blast.y == doctest::Approx(spot.y).epsilon(1e-5));
    CHECK(w.targets[0].hits == 1);
    CHECK(w.targets[1].hits == 1);
    CHECK(w.targets[2].hits == 0);
}

TEST_CASE("Tab keeps bombs selected: there's nothing else to switch to yet") {
    const Rules r;
    World w = one_plane(r);
    InputCommand c;
    c.switch_heavy = true;
    run(w, c, 1, r);
    CHECK(w.bodies[0].loadout.heavy == Heavy::Bombs);
}

TEST_CASE("The same commands give exactly the same rounds, bombs and hits") {
    Rules r;
    r.height_rule = HeightRule::AutoAim;
    World a = one_plane(r);
    World b = one_plane(r);
    a.targets = practice_targets(r.targets);
    b.targets = practice_targets(r.targets);
    for (int i = 0; i < 400; ++i) {
        InputCommand c = fire(i % 50 < 30, i % 90 == 0, i % 70 < 20 ? 127 : -40);
        c.turn = static_cast<std::int8_t>(i % 120 < 60 ? 60 : -60);
        step(a, std::vector<InputCommand>{c}, r);
        step(b, std::vector<InputCommand>{c}, r);
        REQUIRE(a.events == b.events);
    }
    CHECK(a.bodies == b.bodies);
    CHECK(a.rounds == b.rounds);
    CHECK(a.bombs == b.bombs);
    CHECK(a.targets == b.targets);
}

TEST_CASE("Height rules have names for the option and the command line") {
    HeightRule h = HeightRule::Forgiving;
    CHECK(parse_height_rule("strict", h));
    CHECK(h == HeightRule::Strict);
    CHECK(parse_height_rule("auto", h));
    CHECK(h == HeightRule::AutoAim);
    CHECK_FALSE(parse_height_rule("sometimes", h));
    CHECK(std::string(height_rule_name(HeightRule::Forgiving)) == "forgiving");
}
