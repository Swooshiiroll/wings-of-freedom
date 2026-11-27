#include "core/flight.hpp"

#include <cmath>
#include <cstdint>

#include "core/world.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {
InputCommand cmd(int turn, int pitch, int throttle = 0) {
    InputCommand c;
    c.turn = static_cast<std::int8_t>(turn);
    c.pitch = static_cast<std::int8_t>(pitch);
    c.throttle = static_cast<std::int8_t>(throttle);
    return c;
}

// High up, so a test can dive without meeting the ground (fly() doesn't apply it anyway).
Body airborne(float speed, float throttle) {
    Body b = start_body(FlightParams{}, 0.0f, 0.0f, 0.0f);
    b.z = 200.0f;
    b.speed_u_s = speed;
    b.throttle = throttle;
    return b;
}

void fly_for(Body& b, const InputCommand& c, float seconds, const FlightParams& p) {
    const Aero a = aero_of(p);
    const int ticks = static_cast<int>(std::lround(seconds * kTicksPerSecond));
    for (int i = 0; i < ticks; ++i) {
        fly(b, c, p, a, kTickSeconds);
    }
}
} // namespace

TEST_CASE("Full throttle levels off at the max level speed") {
    const FlightParams p;
    Body slow = airborne(80.0f, 1.0f);
    fly_for(slow, cmd(0, 0), 120.0f, p);
    CHECK(slow.speed_u_s == doctest::Approx(150.0f).epsilon(0.015));
    Body fast = airborne(220.0f, 1.0f);
    fly_for(fast, cmd(0, 0), 120.0f, p);
    CHECK(fast.speed_u_s == doctest::Approx(150.0f).epsilon(0.015));
    CHECK(fast.z == doctest::Approx(200.0f)); // level: no height change
}

TEST_CASE("Climbing costs speed and diving gains it, up to the dive limit") {
    const FlightParams p;
    Body climb = airborne(110.0f, 1.0f);
    fly_for(climb, cmd(0, 127), 1.0f, p);
    CHECK(climb.speed_u_s < 100.0f);
    CHECK(climb.z > 200.0f);
    Body dive = airborne(110.0f, 1.0f);
    fly_for(dive, cmd(0, -127), 1.0f, p);
    CHECK(dive.speed_u_s > 130.0f);
    CHECK(dive.z < 200.0f);
    fly_for(dive, cmd(0, -127), 10.0f, p);
    CHECK(dive.speed_u_s == doctest::Approx(p.dive_limit_u_s));
}

TEST_CASE("The best sustained climb is about 18 u/s at full throttle") {
    const FlightParams p;
    const Aero a = aero_of(p);
    float best = 0.0f;
    for (float v = 60.0f; v <= 150.0f; v += 1.0f) {
        const float excess = thrust(a, v, 1.0f) - drag(a, p, v);
        best = std::fmax(best, v * excess / p.gravity); // v x sin(the steady climb angle)
    }
    CHECK(best > 16.0f);
    CHECK(best < 22.0f);
}

TEST_CASE("With the engine off the plane glides about 6:1 at its best glide speed") {
    const FlightParams p;
    const Aero a = aero_of(p);
    const float sin_glide = drag(a, p, p.best_glide_u_s) / p.gravity;
    const float ratio = std::sqrt(1.0f - sin_glide * sin_glide) / sin_glide;
    CHECK(ratio == doctest::Approx(6.0f).epsilon(0.01));
    // And it's the best: a little faster or slower glides worse.
    CHECK(drag(a, p, 75.0f) > drag(a, p, 90.0f));
    CHECK(drag(a, p, 110.0f) > drag(a, p, 90.0f));
}

TEST_CASE("W and S move the pitch at its rate to its limit, and it eases back to level") {
    const FlightParams p; // 60 deg/s, 40 deg
    Body b = airborne(150.0f, 1.0f);
    b.z = 60.0f; // well below the ceiling, which would push the nose down
    fly_for(b, cmd(0, 127), 0.5f, p);
    CHECK(b.pitch_deg == doctest::Approx(30.0f).epsilon(0.01));
    fly_for(b, cmd(0, 127), 0.5f, p);
    CHECK(b.pitch_deg == doctest::Approx(40.0f));
    fly_for(b, cmd(0, 0), 0.5f, p);
    CHECK(b.pitch_deg == doctest::Approx(10.0f).epsilon(0.02));
    fly_for(b, cmd(0, 0), 0.5f, p);
    CHECK(b.pitch_deg == doctest::Approx(0.0f));
    fly_for(b, cmd(0, -127), 2.0f, p);
    CHECK(b.pitch_deg == doctest::Approx(-40.0f));
}

TEST_CASE("Turning is best at the best turn speed and slower either side") {
    const FlightParams p;
    CHECK(turn_rate(p, 110.0f) == doctest::Approx(100.0f));
    CHECK(turn_rate(p, 55.0f) == doctest::Approx(50.0f));
    CHECK(turn_rate(p, 220.0f) == doctest::Approx(50.0f));
    CHECK(turn_rate(p, 150.0f) == doctest::Approx(73.33f).epsilon(0.01));
}

TEST_CASE("The throttle moves at its rate and stays between idle and full") {
    const FlightParams p;
    Body b = airborne(110.0f, 0.8f);
    fly_for(b, cmd(0, 0, 127), 1.0f, p);
    CHECK(b.throttle == doctest::Approx(1.0f));
    fly_for(b, cmd(0, 0, -127), 1.0f, p);
    CHECK(b.throttle == doctest::Approx(0.5f).epsilon(0.01));
    fly_for(b, cmd(0, 0, -127), 2.0f, p);
    CHECK(b.throttle == doctest::Approx(0.0f));
}

TEST_CASE("Below the stall speed the nose drops on its own and the controls go soft") {
    const FlightParams p;
    Body b = airborne(50.0f, 0.0f);
    const Aero a = aero_of(p);
    fly(b, cmd(0, 127), p, a, kTickSeconds);
    CHECK(b.stalled);
    // Even with S held, the nose goes down: 40 deg/s of drop against 30% of 60 deg/s.
    fly_for(b, cmd(0, 127), 0.5f, p);
    CHECK(b.pitch_deg < -5.0f);
    // Turning works at 30%.
    const float before = b.heading_deg;
    fly(b, cmd(127, 0), p, a, kTickSeconds);
    const float turned = b.heading_deg - before;
    CHECK(
        turned ==
        doctest::Approx(p.stall_control * turn_rate(p, b.speed_u_s) * kTickSeconds).epsilon(0.05));
    // Diving builds speed back up, and the stall ends above the recovery speed.
    fly_for(b, cmd(0, -127), 3.0f, p);
    CHECK_FALSE(b.stalled);
    CHECK(b.speed_u_s > p.stall_recover_u_s);
}

TEST_CASE("A tank lasts 240 s at full throttle and 800 s at idle, then the engine stops") {
    const FlightParams p;
    Body full = airborne(150.0f, 1.0f);
    fly_for(full, cmd(0, 0), 239.0f, p);
    CHECK(full.engine_on);
    CHECK(full.fuel == doctest::Approx(1.0f / 240.0f).epsilon(0.05));
    fly_for(full, cmd(0, 0), 2.0f, p);
    CHECK_FALSE(full.engine_on);
    CHECK(full.fuel == doctest::Approx(0.0f));

    Body idle = airborne(150.0f, 0.0f);
    fly_for(idle, cmd(0, 0), 790.0f, p);
    CHECK(idle.engine_on);
    fly_for(idle, cmd(0, 0), 20.0f, p);
    CHECK_FALSE(idle.engine_on);
}

TEST_CASE("With the engine stopped the plane has no thrust and comes down") {
    const FlightParams p;
    Body b = airborne(110.0f, 1.0f);
    b.engine_on = false;
    b.fuel = 0.0f;
    fly_for(b, cmd(0, 0), 20.0f, p);
    CHECK(b.z < 150.0f);
    CHECK(b.speed_u_s < p.dive_limit_u_s);
    CHECK(thrust(aero_of(p), 100.0f, 0.0f) == doctest::Approx(0.0f));
}

TEST_CASE("The plane can't climb above the ceiling") {
    const FlightParams p;
    Body b = airborne(150.0f, 1.0f);
    b.z = p.ceiling_u - 1.0f;
    fly_for(b, cmd(0, 127), 2.0f, p);
    CHECK(b.z <= p.ceiling_u);
    CHECK(b.z == doctest::Approx(p.ceiling_u));
}

TEST_CASE("The plane banks into turns and levels its wings after") {
    const FlightParams p;
    Body b = airborne(110.0f, 1.0f);
    fly_for(b, cmd(127, 0), 0.5f, p);
    CHECK(b.bank_deg == doctest::Approx(30.0f));
    fly_for(b, cmd(-127, 0), 1.0f, p);
    CHECK(b.bank_deg == doctest::Approx(-30.0f));
    fly_for(b, cmd(0, 0), 0.5f, p);
    CHECK(b.bank_deg == doctest::Approx(0.0f));
}

TEST_CASE("A new run starts in the air with the start values") {
    const FlightParams p;
    const Body b = start_body(p, 1440.0f, 480.0f, 370.0f);
    CHECK(b.z == doctest::Approx(120.0f));
    CHECK(b.speed_u_s == doctest::Approx(110.0f));
    CHECK(b.throttle == doctest::Approx(0.8f));
    CHECK(b.heading_deg == doctest::Approx(10.0f));
    CHECK(b.fuel == doctest::Approx(1.0f));
    CHECK(b.outcome == Outcome::Flying);
}

TEST_CASE(
    "The height share slows only the up and down: same speed and ground, a third the height") {
    const FlightParams third; // the default
    FlightParams whole = third;
    whole.height_share = 1.0f;
    CHECK(third.height_share == doctest::Approx(1.0f / 3.0f));
    Body slow = airborne(150.0f, 1.0f);
    Body quick = slow;
    fly_for(slow, cmd(0, 127), 1.0f, third);
    fly_for(quick, cmd(0, 127), 1.0f, whole);
    CHECK(slow.speed_u_s == doctest::Approx(quick.speed_u_s));
    CHECK(slow.x == doctest::Approx(quick.x));
    CHECK(slow.y == doctest::Approx(quick.y));
    CHECK(slow.z - 200.0f == doctest::Approx((quick.z - 200.0f) / 3.0f).epsilon(0.001));
    Body down = airborne(150.0f, 1.0f);
    fly_for(down, cmd(0, -127), 1.0f, third);
    Body fall = airborne(150.0f, 1.0f);
    fly_for(fall, cmd(0, -127), 1.0f, whole);
    CHECK(200.0f - down.z == doctest::Approx((200.0f - fall.z) / 3.0f).epsilon(0.001));
}
