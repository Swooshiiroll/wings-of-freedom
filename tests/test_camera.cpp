#include "core/camera.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {
Body plane_at(float x, float y, float z, float heading) {
    Body b;
    b.x = x;
    b.y = y;
    b.z = z;
    b.heading_deg = heading;
    return b;
}
} // namespace

TEST_CASE("The camera centres halfway between the plane and its shadow") {
    CameraParams p;
    p.lead_u = 0.0f;
    Camera cam;
    const Body b = plane_at(1000.0f, 2000.0f, 100.0f, 0.0f);
    cam.snap_to(b, p);
    const ScreenPt plane = to_screen(b.x, b.y, b.z);
    const ScreenPt shadow = to_screen(b.x, b.y, 0.0f);
    CHECK(cam.focus().x == doctest::Approx(plane.x));
    CHECK(cam.focus().y == doctest::Approx((plane.y + shadow.y) * 0.5f));
}

TEST_CASE("The camera leads along the heading, so north leads up-right") {
    CameraParams p; // lead 40 u
    Camera cam;
    const Body b = plane_at(1000.0f, 2000.0f, 0.0f, 0.0f);
    cam.snap_to(b, p);
    CHECK(cam.lead().x == doctest::Approx(0.0f));
    CHECK(cam.lead().y == doctest::Approx(40.0f));
    const ScreenPt centre = to_screen(b.x, b.y);
    CHECK(cam.focus().x > centre.x); // right
    CHECK(cam.focus().y < centre.y); // and up
}

TEST_CASE("After a turn the lead eases round instead of jumping") {
    CameraParams p;
    Camera cam;
    Body b = plane_at(0.0f, 0.0f, 0.0f, 0.0f);
    cam.snap_to(b, p);     // leading north
    b.heading_deg = 90.0f; // now east
    cam.follow(b, p);
    CHECK(cam.lead().x > 0.0f);
    CHECK(cam.lead().x < 5.0f); // one tick in: only a little way round
    for (int i = 0; i < 5 * 60; ++i) {
        cam.follow(b, p);
    }
    CHECK(cam.lead().x == doctest::Approx(40.0f).epsilon(0.01));
    CHECK(cam.lead().y == doctest::Approx(0.0f).epsilon(0.01));
}

TEST_CASE("focus interpolates between the last two ticks") {
    CameraParams p;
    p.lead_u = 0.0f;
    Camera cam;
    Body b = plane_at(0.0f, 0.0f, 0.0f, 0.0f);
    cam.snap_to(b, p);
    const ScreenPt before = cam.focus();
    b.y = 10.0f;
    cam.follow(b, p);
    const ScreenPt after = cam.focus(1.0f);
    const ScreenPt half = cam.focus(0.5f);
    CHECK(cam.focus(0.0f).x == doctest::Approx(before.x));
    CHECK(half.x == doctest::Approx((before.x + after.x) * 0.5f));
    CHECK(half.y == doctest::Approx((before.y + after.y) * 0.5f));
}

TEST_CASE("view_origin snaps the view to whole art pixels") {
    CHECK(view_origin({100.4f, 50.6f}, 480, 270) == PixelPt{-140, -84});
    CHECK(view_origin({0.0f, 0.0f}, 512, 288) == PixelPt{-256, -144});
    CHECK(view_origin({-0.6f, 0.6f}, 2, 2) == PixelPt{-2, 0});
}
