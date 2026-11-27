#include "core/iso.hpp"

#include <cmath>
#include <initializer_list>

#include <doctest/doctest.h>

using namespace wof;

TEST_CASE("North is up-right on screen, south down-left, east down-right") {
    const ScreenPt north = to_screen(0.0f, 10.0f);
    CHECK(north.x > 0.0f);
    CHECK(north.y < 0.0f);
    const ScreenPt south = to_screen(0.0f, -10.0f);
    CHECK(south.x < 0.0f);
    CHECK(south.y > 0.0f);
    const ScreenPt east = to_screen(10.0f, 0.0f);
    CHECK(east.x > 0.0f);
    CHECK(east.y > 0.0f);
}

TEST_CASE("Ground lines run at 2:1") {
    const ScreenPt e = to_screen(100.0f, 0.0f);
    CHECK(e.x == doctest::Approx(100.0f));
    CHECK(e.y == doctest::Approx(50.0f));
    const ScreenPt n = to_screen(0.0f, 100.0f);
    CHECK(n.x == doctest::Approx(100.0f));
    CHECK(n.y == doctest::Approx(-50.0f));
}

TEST_CASE("Height is a 0.4 px per u gap above the shadow") {
    const ScreenPt shadow = to_screen(300.0f, 700.0f, 0.0f);
    const ScreenPt plane = to_screen(300.0f, 700.0f, 240.0f);
    CHECK(plane.x == doctest::Approx(shadow.x));
    CHECK(shadow.y - plane.y == doctest::Approx(96.0f)); // the ceiling
    CHECK(shadow.y - to_screen(300.0f, 700.0f, 120.0f).y == doctest::Approx(48.0f));
}

TEST_CASE("to_ground undoes to_screen on the ground") {
    for (const float x : {-500.0f, 0.0f, 37.5f, 2880.0f}) {
        for (const float y : {-200.0f, 0.0f, 911.0f, 4800.0f}) {
            const ScreenPt s = to_screen(x, y);
            const GroundPt g = to_ground(s.x, s.y);
            CHECK(g.x == doctest::Approx(x));
            CHECK(g.y == doctest::Approx(y));
        }
    }
}

TEST_CASE("floor_div rounds toward minus infinity") {
    CHECK(floor_div(7, 4) == 1);
    CHECK(floor_div(-1, 4) == -1);
    CHECK(floor_div(-4, 4) == -1);
    CHECK(floor_div(-5, 4) == -2);
    CHECK(floor_div(0, 4) == 0);
}

TEST_CASE("ground_of_pixel finds the unit under each pixel's centre") {
    for (int sx = -40; sx <= 40; sx += 3) {
        for (int sy = -30; sy <= 30; sy += 7) {
            const GroundPt exact =
                to_ground(static_cast<float>(sx) + 0.5f, static_cast<float>(sy) + 0.5f);
            const GroundCell c = ground_of_pixel(sx, sy);
            CAPTURE(sx);
            CAPTURE(sy);
            CHECK(c.x == static_cast<int>(std::floor(exact.x)));
            CHECK(c.y == static_cast<int>(std::floor(exact.y)));
        }
    }
}

TEST_CASE("Headings wrap and pick one of 32 sprites") {
    CHECK(wrap_heading(370.0f) == doctest::Approx(10.0f));
    CHECK(wrap_heading(-90.0f) == doctest::Approx(270.0f));
    CHECK(heading_index(0.0f) == 0);
    CHECK(heading_index(11.25f) == 1);
    CHECK(heading_index(90.0f) == 8);
    CHECK(heading_index(185.0f) == 16);
    CHECK(heading_index(359.0f) == 0);
    CHECK(heading_index(-11.25f) == 31);
}
