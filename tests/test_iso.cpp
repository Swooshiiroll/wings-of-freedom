#include "core/iso.hpp"

#include <cmath>
#include <initializer_list>

#include <doctest/doctest.h>

using namespace wof;

TEST_CASE("North is straight up the screen, east to the right") {
    const ScreenPt north = to_screen(0.0f, 10.0f);
    CHECK(north.x == doctest::Approx(0.0f));
    CHECK(north.y < 0.0f);
    const ScreenPt south = to_screen(0.0f, -10.0f);
    CHECK(south.y > 0.0f);
    const ScreenPt east = to_screen(10.0f, 0.0f);
    CHECK(east.x > 0.0f);
    CHECK(east.y == doctest::Approx(0.0f));
}

TEST_CASE("The ground is seen from 30 degrees up, about a pixel to the unit") {
    CHECK(to_screen(100.0f, 0.0f).x == doctest::Approx(141.4214f));
    CHECK(to_screen(0.0f, 100.0f).y == doctest::Approx(-70.7107f));
}

TEST_CASE("The farmland's axes run diagonally, so its fields look isometric") {
    // Its east is down-right on screen and its north up-right, both at 2:1.
    const GroundCell here = ground_of_pixel(0, 0);
    CHECK(ground_of_pixel(100, 50).x - here.x == 100);
    CHECK(ground_of_pixel(100, 50).y - here.y == 0);
    CHECK(ground_of_pixel(100, -50).x - here.x == 0);
    CHECK(ground_of_pixel(100, -50).y - here.y == 100);
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

TEST_CASE("ground_of_pixel finds the farmland unit under each pixel's centre") {
    for (int sx = -40; sx <= 40; sx += 3) {
        for (int sy = -30; sy <= 30; sy += 7) {
            const float cx = static_cast<float>(sx) + 0.5f;
            const float cy = static_cast<float>(sy) + 0.5f;
            const GroundCell c = ground_of_pixel(sx, sy);
            CAPTURE(sx);
            CAPTURE(sy);
            CHECK(c.x == static_cast<int>(std::floor(cx * 0.5f + cy)));
            CHECK(c.y == static_cast<int>(std::floor(cx * 0.5f - cy)));
        }
    }
}

TEST_CASE("farmland_of is the unit drawn under a world point") {
    for (const float x : {-300.0f, 0.0f, 1440.0f, 14400.0f}) {
        for (const float y : {0.0f, 480.0f, 9000.0f}) {
            const ScreenPt s = to_screen(x, y);
            CHECK(farmland_of(x, y) == ground_of_pixel(static_cast<int>(std::floor(s.x)),
                                                       static_cast<int>(std::floor(s.y))));
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
