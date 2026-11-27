#include "core/world.hpp"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

#include "core/map.hpp"

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

void run(World& w, const std::vector<InputCommand>& cmds, int ticks, const Rules& r) {
    for (int i = 0; i < ticks; ++i) {
        step(w, cmds, r);
    }
}

Body plane(float x, float y, float heading) {
    return start_body(FlightParams{}, x, y, heading);
}
} // namespace

TEST_CASE("Touching the ground is a crash, and a crashed plane stays put") {
    const Rules r;
    World w;
    Body b = plane(1440.0f, 2000.0f, 0.0f);
    b.z = 3.0f;
    b.pitch_deg = -40.0f;
    w.bodies.push_back(b);
    run(w, {cmd(0, -127)}, 30, r);
    CHECK(w.bodies[0].outcome == Outcome::Crashed);
    CHECK(w.bodies[0].z == doctest::Approx(0.0f));
    const Body crashed = w.bodies[0];
    run(w, {cmd(127, 127, 127)}, 30, r);
    CHECK(w.bodies[0] == crashed);
}

TEST_CASE("Flying off the south edge is a return to base") {
    const Rules r;
    World w;
    w.bodies.push_back(plane(1440.0f, 20.0f, 180.0f));
    run(w, {}, 60, r);
    CHECK(w.bodies[0].outcome == Outcome::ReturnedHome);
}

TEST_CASE("A warning shows near the east, west and north edges, not the south") {
    const EdgeParams e;
    const auto check_warning = [&](float x, float y, bool expected) {
        Body b = plane(x, y, 0.0f);
        apply_edges(b, e, kTickSeconds);
        CAPTURE(x);
        CAPTURE(y);
        CHECK(b.edge_warning == expected);
        CHECK(b.outcome == Outcome::Flying);
    };
    check_warning(150.0f, 2000.0f, true);                                 // west
    check_warning(static_cast<float>(kMapWidthU) - 50.0f, 2000.0f, true); // east
    check_warning(1440.0f, static_cast<float>(kMapDepthU) - 10.0f, true); // north
    check_warning(1440.0f, 2000.0f, false);
    check_warning(1440.0f, 100.0f, false); // south is the way home
}

TEST_CASE("Past a soft edge a countdown runs; back inside it stops, and it starts again from 10") {
    const EdgeParams e; // 10 s
    Body b = plane(-5.0f, 2000.0f, 270.0f);
    apply_edges(b, e, 1.0f);
    CHECK(b.edge_countdown_s == doctest::Approx(9.0f));
    apply_edges(b, e, 3.0f);
    CHECK(b.edge_countdown_s == doctest::Approx(6.0f));
    b.x = 10.0f; // back inside
    apply_edges(b, e, 1.0f);
    CHECK(b.edge_countdown_s < 0.0f);
    CHECK(b.edge_warning);
    b.y = static_cast<float>(kMapDepthU) + 1.0f; // out over the north edge this time
    apply_edges(b, e, 1.0f);
    CHECK(b.edge_countdown_s == doctest::Approx(9.0f));
    apply_edges(b, e, 8.5f);
    CHECK(b.outcome == Outcome::Flying);
    apply_edges(b, e, 0.5f);
    CHECK(b.outcome == Outcome::LeftArea); // self-destructed
}

TEST_CASE("The countdown runs out in flight too") {
    const Rules r;
    World w;
    w.bodies.push_back(plane(static_cast<float>(kMapWidthU) + 50.0f, 2000.0f, 90.0f)); // past east
    run(w, {}, 9 * kTicksPerSecond, r);
    CHECK(w.bodies[0].outcome == Outcome::Flying);
    run(w, {}, 2 * kTicksPerSecond, r);
    CHECK(w.bodies[0].outcome == Outcome::LeftArea);
}

TEST_CASE("Each body takes its own command, and one without a command flies on") {
    const Rules r;
    World w;
    w.bodies.push_back(plane(1000.0f, 2000.0f, 0.0f));
    w.bodies.push_back(plane(1500.0f, 2000.0f, 90.0f));
    w.bodies.push_back(plane(2000.0f, 2000.0f, 180.0f));
    run(w, {cmd(127, 0), cmd(0, 127)}, 30, r); // no command for the third
    CHECK(w.bodies[0].heading_deg > 30.0f);
    CHECK(w.bodies[0].pitch_deg == doctest::Approx(0.0f));
    CHECK(w.bodies[1].heading_deg == doctest::Approx(90.0f));
    CHECK(w.bodies[1].pitch_deg == doctest::Approx(30.0f));
    CHECK(w.bodies[2].heading_deg == doctest::Approx(180.0f));
    CHECK(w.bodies[2].y < 2000.0f);
    CHECK(w.tick == 30);
}

TEST_CASE("The same commands give exactly the same flight") {
    const std::vector<InputCommand> script = {cmd(127, -60, 127), cmd(-40, 127, 0),
                                              cmd(0, 0, -127)};
    const Rules r;
    World a;
    World b;
    for (World* w : {&a, &b}) {
        w->bodies.push_back(plane(1440.0f, 400.0f, 0.0f));
    }
    for (int i = 0; i < 900; ++i) {
        const std::vector<InputCommand> now = {script[static_cast<std::size_t>(i / 37 % 3)]};
        step(a, now, r);
        step(b, now, r);
    }
    CHECK(a.bodies[0] == b.bodies[0]);
}
