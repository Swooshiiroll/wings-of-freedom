#include "core/waves.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

#include "core/tuning.hpp"
#include "core/world.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {

// Pilots that never fire and planes that never ram, so a player circling survives any number of
// waves.
Rules quiet_rules() {
    Rules r;
    r.ai.fire_range_u = 0.0f;
    r.damage.collision_radius_u = 0.0f;
    r.rederive();
    return r;
}

World fresh(const Rules& r, int first = 1) {
    World w;
    Body b = new_plane(r, PlaneId::P26A, 1440.0f, 1500.0f, 0.0f);
    w.bodies.push_back(b);
    w.waves = start_waves(r, PlaneId::He51B, first);
    return w;
}

void run(World& w, const Rules& r, float seconds) {
    InputCommand circle;
    circle.turn = 127;
    const int ticks = static_cast<int>(std::lround(seconds * kTicksPerSecond));
    for (int i = 0; i < ticks; ++i) {
        step(w, std::vector<InputCommand>{circle}, r);
    }
}

} // namespace

TEST_CASE("Wave n brings 1 + n/2 fighters") {
    CHECK(wave_size(1) == 1);
    CHECK(wave_size(2) == 2);
    CHECK(wave_size(3) == 2);
    CHECK(wave_size(4) == 3);
    CHECK(wave_size(9) == 5);
}

TEST_CASE("The first wave comes at 10 s, then one every 40 s, each announced") {
    const Rules r = quiet_rules();
    World w = fresh(r);
    run(w, r, 9.9f);
    CHECK(w.pilots.empty());
    int announced = 0;
    InputCommand circle;
    circle.turn = 127;
    for (int i = 0; i < 82 * kTicksPerSecond; ++i) {
        step(w, std::vector<InputCommand>{circle}, r);
        for (const Event& e : w.events) {
            if (e.kind == EventKind::WaveArrived) {
                ++announced;
                CHECK(e.who == announced);
                CHECK(static_cast<int>(e.damage) == wave_size(announced));
            }
        }
    }
    CHECK(announced == 3);        // 10, 50 and 90 s
    CHECK(w.pilots.size() == 5u); // 1 + 2 + 2
    CHECK(w.waves.current == 3);
}

TEST_CASE("Enemies arrive from the north near your height, in the enemy's fighter") {
    const Rules r = quiet_rules();
    for (const float height : {150.0f, 400.0f}) {
        World w = fresh(r, 5); // three of them
        w.bodies[0].z = height;
        run(w, r, 10.0f + kTickSeconds);
        REQUIRE(w.pilots.size() == 3u);
        for (const Pilot& p : w.pilots) {
            const Body& b = w.bodies[static_cast<std::size_t>(p.body)];
            CHECK(std::abs(b.z - w.bodies[0].z) <= 65.0f);
            CHECK(b.y > 4600.0f);
            CHECK(b.type == static_cast<std::uint8_t>(PlaneId::He51B));
        }
    }
}

TEST_CASE("No more than six are in the air; the rest wait for a place") {
    Rules r = quiet_rules();
    r.wave.every_s = 5.0f;
    World w = fresh(r);
    run(w, r, 40.0f); // seven waves: 18 fighters in all
    CHECK(enemies_alive(w) == 6);
    CHECK(w.waves.waiting > 0);
    // One goes down: a waiting one takes its place.
    w.bodies[1].damage.going_down = true;
    const std::size_t before = w.pilots.size();
    run(w, r, kTickSeconds);
    CHECK(w.pilots.size() == before + 1);
    CHECK(enemies_alive(w) == 6);
}

TEST_CASE("No waves when they're off, and none after the player is down") {
    const Rules r = quiet_rules();
    World off = fresh(r);
    off.waves.enabled = false;
    run(off, r, 20.0f);
    CHECK(off.pilots.empty());
    World down = fresh(r);
    down.bodies[0].outcome = Outcome::Crashed;
    run(down, r, 20.0f);
    CHECK(down.pilots.empty());
}

TEST_CASE("The same seed gives the same fight") {
    const Rules r;
    World a = fresh(r, 4);
    World b = fresh(r, 4);
    run(a, r, 30.0f);
    run(b, r, 30.0f);
    REQUIRE(a.bodies.size() == b.bodies.size());
    CHECK(a.bodies == b.bodies);
    CHECK(a.waves == b.waves);
}

TEST_CASE("The ai and wave numbers are in the tuning file") {
    Tuning t;
    std::string errors;
    CHECK(from_tuning("ai.aim_error_first_deg = 12\nwave.max_alive = 3\nwave.first_s = 2\n", t,
                      errors));
    CHECK(t.rules.ai.aim_error_first_deg == doctest::Approx(12.0f));
    CHECK(t.rules.wave.max_alive == 3);
    CHECK(t.rules.wave.first_s == doctest::Approx(2.0f));
    Tuning back;
    CHECK(from_tuning(to_tuning(t), back, errors));
    CHECK(back == t);
}
