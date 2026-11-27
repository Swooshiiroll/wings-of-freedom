#include "core/planes.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "core/camera.hpp"
#include "core/plane_rules.hpp"
#include "core/tuning.hpp"
#include "core/world.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {
// Flies a plane level at full throttle for long enough to settle at its top speed.
float settled_top_speed(const PlaneRules& pr) {
    Body b = start_body(pr.flight, 0.0f, 0.0f, 0.0f);
    b.z = 300.0f;
    b.throttle = 1.0f;
    b.speed_u_s = pr.flight.max_level_u_s * 0.6f;
    b.fuel = 1.0f;
    FlightParams long_tank = pr.flight;
    long_tank.tank_s = 1e6f; // so the fuel lasts the test
    for (int i = 0; i < 200 * kTicksPerSecond; ++i) {
        fly(b, InputCommand{}, long_tank, pr.aero, kTickSeconds);
    }
    return b.speed_u_s;
}
} // namespace

TEST_CASE("The roster holds the v0.1 planes, and each nation has its fighter") {
    CHECK(plane_type(PlaneId::P26A).nation == Nation::US);
    CHECK(plane_type(PlaneId::He51B).nation == Nation::Germany);
    CHECK(plane_type(PlaneId::B10B).role == Role::Bomber);
    CHECK(plane_type(PlaneId::He111B2).engines == 2);
    PlaneId id = PlaneId::P26A;
    CHECK(find_plane("he51b", id));
    CHECK(id == PlaneId::He51B);
    CHECK_FALSE(find_plane("spitfire", id));
    CHECK(fighter_of(Nation::US) == PlaneId::P26A);
    CHECK(fighter_of(Nation::Germany) == PlaneId::He51B);
    // The P-26A carries one .30 and one .50 (owner, 2026-11-26).
    const PlaneType& p26 = plane_type(PlaneId::P26A);
    REQUIRE(p26.gun_count == 2);
    CHECK(p26.guns[0].calibre == Calibre::Rifle);
    CHECK(p26.guns[1].calibre == Calibre::Heavy);
}

TEST_CASE("Each plane flies by its real stats, scaled: top speed, climb, stall, turn, dive, tank") {
    const Rules rules;
    const PlaneScale& s = rules.scale;
    for (int i = 0; i < kPlaneTypes; ++i) {
        const PlaneType& t = plane_type(i);
        const PlaneRules& pr = rules.planes[static_cast<std::size_t>(i)];
        CAPTURE(std::string(t.id));
        CHECK(pr.flight.max_level_u_s == doctest::Approx(t.top_mph * s.u_per_mph));
        CHECK(settled_top_speed(pr) == doctest::Approx(t.top_mph * s.u_per_mph).epsilon(0.03));
        CHECK(best_climb(pr.flight) == doctest::Approx(t.climb_fpm * s.u_per_fpm).epsilon(0.03));
        CHECK(pr.flight.stall_u_s == doctest::Approx(t.stall_mph * s.u_per_mph));
        CHECK(pr.flight.turn_rate_deg_s == doctest::Approx(360.0f / t.turn_s * s.turn_factor));
        CHECK(pr.flight.dive_limit_u_s == doctest::Approx(pr.flight.max_level_u_s * 1.6f));
        CHECK(pr.flight.tank_s == doctest::Approx(t.endurance_h * s.tank_s_per_hour));
        CHECK(pr.flight.ceiling_u == doctest::Approx(600.0f)); // 30,000 ft for every plane
    }
    // The design's game values (design/planes.md).
    CHECK(rules.planes[0].flight.max_level_u_s == doctest::Approx(150.0f).epsilon(0.01));
    CHECK(rules.planes[0].flight.turn_rate_deg_s == doctest::Approx(100.0f).epsilon(0.01));
    CHECK(rules.planes[1].flight.turn_rate_deg_s == doctest::Approx(123.0f).epsilon(0.01));
    CHECK(mph_of(rules.planes[0].flight.max_level_u_s, s) ==
          doctest::Approx(234.0f).epsilon(0.001));
}

TEST_CASE("Each plane carries its real guns and bombs, and its own HP") {
    const Rules rules;
    const Body p26 = new_plane(rules, PlaneId::P26A, 0.0f, 0.0f, 0.0f);
    CHECK(p26.loadout.guns[0].rounds == 500);
    CHECK(p26.loadout.guns[1].rounds == 200);
    CHECK(p26.loadout.bombs == 2);
    CHECK(p26.damage.engines == 1);
    const Body he51 = new_plane(rules, PlaneId::He51B, 0.0f, 0.0f, 0.0f);
    CHECK(he51.loadout.bombs == 6);
    CHECK(rounds_left(he51, plane_of(rules, he51), Calibre::Rifle) == 1000);
    const Body b10 = new_plane(rules, PlaneId::B10B, 0.0f, 0.0f, 0.0f);
    CHECK(b10.damage.engines == 2);
    CHECK(b10.damage.engine == doctest::Approx(60.0f));
    CHECK(b10.damage.engine2 == doctest::Approx(60.0f));
    CHECK(b10.damage.fuselage == doctest::Approx(160.0f));
    CHECK(rounds_left(b10, plane_of(rules, b10), Calibre::Rifle) == 0); // gunners', not the pilot's
}

TEST_CASE("A twin keeps flying on one engine, at about 79% of its top speed") {
    const Rules rules;
    Body b = new_plane(rules, PlaneId::He111B2, 0.0f, 0.0f, 0.0f);
    const DamageParams& d = plane_of(rules, b).damage;
    apply_hit(b, Part::Engine, 60.0f, d);
    CHECK(b.engine_on);
    CHECK(part_stage(b, Part::Engine, d) == Stage::Destroyed);
    CHECK(part_stage(b, Part::Engine2, d) == Stage::Ok);
    CHECK(handling_of(b, d).top_speed_share == doctest::Approx(std::cbrt(0.5f)).epsilon(0.01));
    apply_hit(b, Part::Engine2, 60.0f, d);
    CHECK_FALSE(b.engine_on); // both gone: it glides
}

TEST_CASE("A twin's engine hits go to the engine on the hit's side, or either") {
    const Rules rules;
    Body b = new_plane(rules, PlaneId::B10B, 0.0f, 0.0f, 0.0f);
    const DamageParams& d = plane_of(rules, b).damage;
    Rng rng(9);
    std::array<int, kParts> ahead{};
    std::array<int, kParts> left{};
    for (int i = 0; i < 20000; ++i) {
        ++ahead[static_cast<std::size_t>(pick_part(b, HitFrom::Ahead, d, rng))];
        ++left[static_cast<std::size_t>(pick_part(b, HitFrom::Left, d, rng))];
    }
    const auto at = [](const std::array<int, kParts>& c, Part p) {
        return static_cast<float>(c[static_cast<std::size_t>(p)]) / 200.0f; // percent
    };
    CHECK(at(ahead, Part::Engine) + at(ahead, Part::Engine2) ==
          doctest::Approx(45.0f).epsilon(0.04));
    CHECK(std::abs(at(ahead, Part::Engine) - at(ahead, Part::Engine2)) < 1.5f);
    CHECK(left[static_cast<std::size_t>(Part::Engine2)] == 0);
    CHECK(at(left, Part::Engine) == doctest::Approx(10.0f).epsilon(0.15));
}

TEST_CASE("The ground zooms out as you climb: full size on the deck, half at 30,000 ft") {
    CHECK(ground_zoom(0.0f, 600.0f, 0.5f) == doctest::Approx(1.0f));
    CHECK(ground_zoom(300.0f, 600.0f, 0.5f) == doctest::Approx(0.75f));
    CHECK(ground_zoom(600.0f, 600.0f, 0.5f) == doctest::Approx(0.5f));
    CHECK(ground_zoom(900.0f, 600.0f, 0.5f) == doctest::Approx(0.5f));
    CHECK(ground_zoom(-5.0f, 600.0f, 0.5f) == doctest::Approx(1.0f));
}

TEST_CASE("Changing the scale in the tuning file re-derives every plane") {
    Tuning t;
    std::string errors;
    CHECK(from_tuning("scale.u_per_mph = 0.5\n", t, errors));
    CHECK(t.rules.planes[0].flight.max_level_u_s == doctest::Approx(117.0f)); // 234 x 0.5
    CHECK(from_tuning("damage.wound_damage = 7\n", t, errors));
    CHECK(t.rules.planes[1].damage.wound_damage == doctest::Approx(7.0f));
}
