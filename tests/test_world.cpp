#include "core/world.hpp"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include <doctest/doctest.h>

using namespace wof;

namespace {
InputCommand cmd(int turn, int pitch) {
    InputCommand c;
    c.turn = static_cast<std::int8_t>(turn);
    c.pitch = static_cast<std::int8_t>(pitch);
    return c;
}

void run(World& w, const std::vector<InputCommand>& cmds, int ticks, const StandInParams& p) {
    for (int i = 0; i < ticks; ++i) {
        step(w, cmds, p);
    }
}
} // namespace

TEST_CASE("The stand-in turns at its turn rate and flies at its speed") {
    const StandInParams p;
    World w;
    w.bodies.push_back(Body{0.0f, 0.0f, 100.0f, 0.0f, 0.0f});
    run(w, {cmd(127, 0)}, 60, p);
    CHECK(w.bodies[0].heading_deg == doctest::Approx(100.0f).epsilon(0.001));
    CHECK(w.bodies[0].speed_u_s == doctest::Approx(110.0f));
    CHECK(w.tick == 60);

    World straight;
    straight.bodies.push_back(Body{0.0f, 0.0f, 100.0f, 0.0f, 0.0f});
    run(straight, {cmd(0, 0)}, 60, p);
    CHECK(straight.bodies[0].x == doctest::Approx(0.0f));
    CHECK(straight.bodies[0].y == doctest::Approx(110.0f).epsilon(0.001)); // north
}

TEST_CASE("Turning left wraps the heading below north") {
    World w;
    w.bodies.push_back(Body{});
    run(w, {cmd(-127, 0)}, 30, StandInParams{});
    CHECK(w.bodies[0].heading_deg == doctest::Approx(310.0f).epsilon(0.001));
}

TEST_CASE("The stand-in climbs and dives at its climb rate, between the ground and the ceiling") {
    const StandInParams p;
    World w;
    w.bodies.push_back(Body{0.0f, 0.0f, 100.0f, 0.0f, 0.0f});
    run(w, {cmd(0, 127)}, 60, p); // S: climb
    CHECK(w.bodies[0].z == doctest::Approx(118.0f).epsilon(0.001));
    run(w, {cmd(0, 127)}, 60 * 20, p);
    CHECK(w.bodies[0].z == doctest::Approx(240.0f));
    run(w, {cmd(0, -127)}, 60 * 20, p); // W: dive
    CHECK(w.bodies[0].z == doctest::Approx(0.0f));
}

TEST_CASE("Each body takes its own command, and one without a command flies on") {
    World w;
    w.bodies.push_back(Body{0.0f, 0.0f, 50.0f, 0.0f, 0.0f});
    w.bodies.push_back(Body{500.0f, 0.0f, 50.0f, 90.0f, 0.0f});
    w.bodies.push_back(Body{900.0f, 0.0f, 50.0f, 180.0f, 0.0f});
    run(w, {cmd(127, 0), cmd(0, 127)}, 60, StandInParams{}); // no command for the third
    CHECK(w.bodies[0].heading_deg == doctest::Approx(100.0f).epsilon(0.001));
    CHECK(w.bodies[0].z == doctest::Approx(50.0f));
    CHECK(w.bodies[1].heading_deg == doctest::Approx(90.0f));
    CHECK(w.bodies[1].z == doctest::Approx(68.0f).epsilon(0.001));
    CHECK(w.bodies[2].heading_deg == doctest::Approx(180.0f));
    CHECK(w.bodies[2].y == doctest::Approx(-110.0f).epsilon(0.001));
}

TEST_CASE("The same commands give exactly the same flight") {
    const std::vector<InputCommand> script = {cmd(127, -60), cmd(-40, 127), cmd(0, 0)};
    World a;
    World b;
    for (World* w : {&a, &b}) {
        w->bodies.push_back(Body{1440.0f, 400.0f, 120.0f, 0.0f, 0.0f});
    }
    for (int i = 0; i < 600; ++i) {
        const std::vector<InputCommand> now = {script[static_cast<std::size_t>(i / 37 % 3)]};
        step(a, now, StandInParams{});
        step(b, now, StandInParams{});
    }
    CHECK(a.bodies[0] == b.bodies[0]);
}
