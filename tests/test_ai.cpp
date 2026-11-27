#include "core/ai.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "core/map.hpp"
#include "core/world.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {

constexpr float kPi = 3.14159265f;

// Rules where nothing rams (so a test can watch positions), and pilots are perfect shots.
Rules sharp_rules() {
    Rules r;
    r.damage.collision_radius_u = 0.0f;
    r.rederive();
    return r;
}

// The player (a P-26A) at the middle of the map, heading north, level at 150 u/s.
World with_player(const Rules& r, float x = 1440.0f, float y = 1200.0f, float z = 200.0f) {
    World w;
    Body b = new_plane(r, PlaneId::P26A, x, y, 0.0f);
    b.z = z;
    b.speed_u_s = 150.0f;
    b.throttle = 1.0f;
    w.bodies.push_back(b);
    return w;
}

// An enemy He 51 with a pilot of `style`, with no aim error and quick looks.
Body& add_enemy(World& w, const Rules& r, Style style, float x, float y, float z, float heading) {
    Body b = new_plane(r, PlaneId::He51B, x, y, heading);
    b.z = z;
    b.speed_u_s = 120.0f;
    b.throttle = 1.0f;
    w.pilots.push_back(
        new_pilot(static_cast<int>(w.bodies.size()), style, Skill{0.0f, 0.1f}, w.rng));
    w.bodies.push_back(b);
    return w.bodies.back();
}

void run(World& w, const Rules& r, float seconds, const InputCommand& player = {}) {
    const int ticks = static_cast<int>(std::lround(seconds * kTicksPerSecond));
    for (int i = 0; i < ticks; ++i) {
        step(w, std::vector<InputCommand>{player}, r);
    }
}

// How far (degrees) body `a` is off the tail of body `b`: 0 dead astern.
float off_tail(const Body& a, const Body& b) {
    const float to_a = std::atan2(a.x - b.x, a.y - b.y) * 180.0f / kPi;
    return std::abs(std::remainder(to_a - (b.heading_deg + 180.0f), 360.0f));
}

} // namespace

TEST_CASE("Later waves aim better and react faster, down to the best") {
    const AiParams p;
    CHECK(skill_of(p, 1).aim_error_deg == doctest::Approx(8.0f));
    CHECK(skill_of(p, 1).reaction_s == doctest::Approx(0.5f));
    CHECK(skill_of(p, 4).aim_error_deg < skill_of(p, 1).aim_error_deg);
    CHECK(skill_of(p, 4).reaction_s < skill_of(p, 1).reaction_s);
    CHECK(skill_of(p, 50).aim_error_deg == doctest::Approx(2.0f));
    CHECK(skill_of(p, 50).reaction_s == doctest::Approx(0.15f));
}

TEST_CASE("A pilot turns toward its target, and only fires close and nose on") {
    const Rules r = sharp_rules();
    World w = with_player(r);
    // East of the player, heading north: the player is to the west, so it turns left.
    add_enemy(w, r, Style::Turn, 1900.0f, 1200.0f, 200.0f, 0.0f);
    Pilot p = w.pilots[0];
    InputCommand c = ai_command(p, w, r, w.rng);
    CHECK(c.turn < 0);
    CHECK_FALSE(c.guns); // 460 u away
    // Right behind it, close, nose on.
    w.bodies[1].x = 1440.0f;
    w.bodies[1].y = 1080.0f;
    p = w.pilots[0];
    c = ai_command(p, w, r, w.rng);
    CHECK(c.guns);
    // Same spot, looking the other way: no.
    w.bodies[1].heading_deg = 180.0f;
    p = w.pilots[0];
    CHECK_FALSE(ai_command(p, w, r, w.rng).guns);
}

TEST_CASE("A pilot pulls out of a low dive instead of hitting the ground") {
    const Rules r = sharp_rules();
    World w = with_player(r, 1440.0f, 1200.0f, 10.0f);
    Body& e = add_enemy(w, r, Style::Turn, 1440.0f, 900.0f, 70.0f, 0.0f);
    e.pitch_deg = -40.0f;
    e.speed_u_s = 180.0f;
    float lowest = 1000.0f;
    for (int i = 0; i < 4 * kTicksPerSecond; ++i) {
        step(w, std::vector<InputCommand>{InputCommand{}}, r);
        lowest = std::min(lowest, w.bodies[1].z);
    }
    CHECK(w.bodies[1].outcome == Outcome::Flying);
    CHECK(lowest > 20.0f);
}

TEST_CASE("Pilots stay inside the battle area, even chasing a player along its edge") {
    Rules r = sharp_rules();
    r.ai.fire_range_u = 0.0f; // watch the flying, not the shooting
    World w = with_player(r, 120.0f, 4500.0f);
    add_enemy(w, r, Style::Turn, 600.0f, 4400.0f, 200.0f, 270.0f);
    add_enemy(w, r, Style::HeadOn, 400.0f, 4300.0f, 200.0f, 300.0f);
    add_enemy(w, r, Style::DiveClimb, 500.0f, 4200.0f, 200.0f, 320.0f);
    InputCommand circle;
    circle.turn = 127; // the player circles by the corner
    for (int i = 0; i < 40 * kTicksPerSecond; ++i) {
        step(w, std::vector<InputCommand>{circle}, r);
        for (std::size_t k = 1; k < w.bodies.size(); ++k) {
            const Body& b = w.bodies[k];
            REQUIRE(b.outcome == Outcome::Flying);
            REQUIRE(b.x > 0.0f);
            REQUIRE(b.x < static_cast<float>(kMapWidthU));
            REQUIRE(b.y > 0.0f);
            REQUIRE(b.y < static_cast<float>(kMapDepthU));
        }
    }
}

TEST_CASE("A turn-fighter gets onto a straight-flying plane's tail and stays near it") {
    Rules r = sharp_rules();
    r.ai.fire_range_u = 0.0f;
    World w = with_player(r, 1440.0f, 600.0f);
    // Starting ahead and to the side, coming the other way.
    add_enemy(w, r, Style::Turn, 1650.0f, 1100.0f, 200.0f, 180.0f);
    InputCommand slow;
    slow.throttle = -127; // so the He 51 can keep up
    run(w, r, 6.0f, slow);
    run(w, r, 14.0f);
    CHECK(off_tail(w.bodies[1], w.bodies[0]) < 50.0f);
    CHECK(std::hypot(w.bodies[1].x - w.bodies[0].x, w.bodies[1].y - w.bodies[0].y) < 300.0f);
}

TEST_CASE("Dive-and-climb gets above its target before it attacks") {
    Rules r = sharp_rules();
    r.ai.fire_range_u = 0.0f;
    World w = with_player(r);
    add_enemy(w, r, Style::DiveClimb, 1700.0f, 1400.0f, 200.0f, 270.0f);
    InputCommand circle;
    circle.turn = 127;
    bool attacked = false;
    float height_at_attack = 0.0f;
    for (int i = 0; i < 40 * kTicksPerSecond && !attacked; ++i) {
        step(w, std::vector<InputCommand>{circle}, r);
        if (w.pilots[0].phase == Phase::Attack) {
            attacked = true;
            height_at_attack = w.bodies[1].z - w.bodies[0].z;
        }
    }
    CHECK(attacked);
    CHECK(height_at_attack > 50.0f);
}

TEST_CASE("Enemy fighters can hit a plane that flies straight") {
    Rules r = sharp_rules();
    World w = with_player(r, 1440.0f, 600.0f);
    add_enemy(w, r, Style::Turn, 1440.0f, 450.0f, 200.0f, 0.0f);
    InputCommand slow;
    slow.throttle = -127;
    int hits = 0;
    for (int i = 0; i < 12 * kTicksPerSecond; ++i) {
        step(w, std::vector<InputCommand>{slow}, r);
        for (const Event& e : w.events) {
            hits += e.kind == EventKind::RoundHitPlane && e.who == 0 && e.by == 1 ? 1 : 0;
        }
    }
    CHECK(hits > 3);
}

TEST_CASE("Enemy fighters never run out of ammunition or fuel") {
    const Rules r = sharp_rules();
    World w = with_player(r);
    add_enemy(w, r, Style::Turn, 1440.0f, 1080.0f, 200.0f, 0.0f);
    w.bodies[1].fuel = 0.1f;
    w.bodies[1].loadout.guns[0].rounds = 3;
    run(w, r, 2.0f);
    const PlaneRules& plane = plane_of(r, w.bodies[1]);
    CHECK(w.bodies[1].loadout.guns[0].rounds == plane.guns[0].rounds);
    CHECK(w.bodies[1].fuel == doctest::Approx(1.0f));
    CHECK(w.bodies[1].engine_on);
}

TEST_CASE("A kill is an enemy going down after the player has hit or rammed it") {
    const Rules r = sharp_rules();
    World w = with_player(r);
    add_enemy(w, r, Style::Turn, 1440.0f, 2500.0f, 200.0f, 0.0f);
    add_enemy(w, r, Style::Turn, 2000.0f, 2500.0f, 200.0f, 0.0f);
    // The first is hit by the player's round, then goes down; the second goes down on its own.
    Round round;
    round.x = w.bodies[1].x;
    round.y = w.bodies[1].y - 4.0f;
    round.z = w.bodies[1].z;
    round.vy = 500.0f;
    round.life_s = 0.5f;
    round.owner = 0;
    w.rounds.push_back(round);
    run(w, r, 0.1f);
    CHECK(w.pilots[0].hurt);
    CHECK(w.waves.kills == 0);
    w.bodies[1].damage.going_down = true;
    w.bodies[2].damage.going_down = true;
    step(w, std::vector<InputCommand>{InputCommand{}}, r);
    CHECK(w.waves.kills == 1);
    CHECK(std::count_if(w.events.begin(), w.events.end(),
                        [](const Event& e) { return e.kind == EventKind::EnemyDown; }) == 1);
    run(w, r, 1.0f);
    CHECK(w.waves.kills == 1); // counted once
}
