#include "core/targets.hpp"

#include <cmath>

#include <doctest/doctest.h>

using namespace wof;

TEST_CASE("The practice layout has 4 drones at 40, 90, 140 and 200 u, and 8 ground targets") {
    const auto targets = practice_targets(TargetParams{});
    int drones = 0;
    int ground = 0;
    for (const Target& t : targets) {
        if (t.kind == TargetKind::Drone) {
            ++drones;
            CHECK(t.orbit_u > 0.0f);
        } else {
            ++ground;
            CHECK(t.z == doctest::Approx(0.0f));
        }
    }
    CHECK(drones == 4);
    CHECK(ground == 8);
    CHECK(targets[0].z == doctest::Approx(40.0f));
    CHECK(targets[3].z == doctest::Approx(200.0f));
}

TEST_CASE("Drones keep to their orbits and heights, flying along them") {
    const TargetParams p;
    auto targets = practice_targets(p);
    const Target start = targets[1];
    for (int i = 0; i < 600; ++i) {
        move_targets(targets, p, 1.0f / 60.0f);
    }
    const Target& d = targets[1];
    CHECK(std::hypot(d.x - d.cx, d.y - d.cy) == doctest::Approx(d.orbit_u).epsilon(0.001));
    CHECK(d.z == doctest::Approx(start.z));
    // 10 s at 70 u/s round a 220 u orbit is 700 u of arc: about 182 degrees.
    float moved = std::fabs(d.angle_deg - start.angle_deg);
    moved = moved > 180.0f ? 360.0f - moved : moved;
    CHECK(moved == doctest::Approx(177.7f).epsilon(0.02));
    // Ground targets don't move.
    CHECK(targets[5].x == doctest::Approx(targets[5].cx));
}

TEST_CASE("A hit counts up and restarts the flash timer") {
    const TargetParams p;
    auto targets = practice_targets(p);
    hit_target(targets[0]);
    hit_target(targets[0]);
    CHECK(targets[0].hits == 2);
    CHECK(targets[0].since_hit_s == doctest::Approx(0.0f));
    move_targets(targets, p, 0.5f);
    CHECK(targets[0].since_hit_s == doctest::Approx(0.5f));
}
