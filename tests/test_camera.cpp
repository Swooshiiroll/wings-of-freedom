#include "core/camera.hpp"
#include "core/view_xf.hpp"

#include <cmath>

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

TEST_CASE("Low down, the camera aims a little above halfway up from the shadow to the plane") {
    CameraParams p;
    p.lead_u = 0.0f;
    Camera cam;
    const Body b = plane_at(1000.0f, 2000.0f, 100.0f, 0.0f);
    cam.snap_to(b, p);
    const ScreenPt plane = to_screen(b.x, b.y, b.z);
    const ScreenPt shadow = to_screen(b.x, b.y, 0.0f);
    CHECK(cam.focus().x == doctest::Approx(plane.x));
    CHECK(cam.focus().y == doctest::Approx(shadow.y + (plane.y - shadow.y) * 0.6f));
}

TEST_CASE("The camera leads along the heading, so north leads straight up") {
    CameraParams p; // lead 10 u
    Camera cam;
    const Body b = plane_at(1000.0f, 2000.0f, 0.0f, 0.0f);
    cam.snap_to(b, p);
    CHECK(cam.lead().x == doctest::Approx(0.0f));
    CHECK(cam.lead().y == doctest::Approx(10.0f));
    const ScreenPt centre = to_screen(b.x, b.y);
    CHECK(cam.focus().x == doctest::Approx(centre.x));
    CHECK(cam.focus().y < centre.y); // up
}

TEST_CASE("After a turn the lead eases round instead of jumping") {
    CameraParams p;
    Camera cam;
    Body b = plane_at(0.0f, 0.0f, 0.0f, 0.0f);
    cam.snap_to(b, p);     // leading north
    b.heading_deg = 90.0f; // now east
    cam.follow(b, p);
    CHECK(cam.lead().x > 0.0f);
    CHECK(cam.lead().x < 1.0f); // one tick in: only a little way round
    for (int i = 0; i < 5 * 60; ++i) {
        cam.follow(b, p);
    }
    CHECK(cam.lead().x == doctest::Approx(10.0f).epsilon(0.01));
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

TEST_CASE("Your plane stays near the middle as you climb, the shadow gap growing then holding") {
    CameraParams p;
    p.lead_u = 0.0f;
    // The whole view, and above each dashboard size.
    for (const int view_h : {270, 174, 190, 206}) {
        const auto h = static_cast<float>(view_h);
        float last_gap = -1.0f;
        for (float z = 0.0f; z <= 600.0f; z += 10.0f) {
            CAPTURE(view_h);
            CAPTURE(z);
            Camera cam;
            const Body b = plane_at(1000.0f, 2000.0f, z, 0.0f);
            cam.snap_to(b, p, h, ground_zoom(z, 600.0f, 0.25f));
            const ViewXf xf = make_view(cam.focus(), z, 600.0f, 0.25f, 480, view_h, cam.drop());
            const ScreenPt plane = xf.at(b.x, b.y, z);
            const ScreenPt shadow = xf.at(b.x, b.y, 0.0f);
            const float gap = shadow.y - plane.y;
            CHECK(plane.x == doctest::Approx(240.0f).epsilon(0.01));
            CHECK(plane.y <= h * 0.5f + 1.0f);
            CHECK(plane.y >= h * 0.5f - kCameraRise - 1.0f);
            CHECK(gap >= last_gap - 1.0f); // whole-pixel snapping wobbles by a pixel
            CHECK(gap <= kCameraGap + 1.0f);
            CHECK(shadow.y < h);
            last_gap = gap;
        }
        CHECK(last_gap == doctest::Approx(kCameraGap).epsilon(0.02));
    }
}

TEST_CASE("The gap is what it was low down, and the default lead moves the plane only a little") {
    CameraParams p;
    Camera cam;
    const Body b = plane_at(1000.0f, 2000.0f, 160.0f, 90.0f); // heading east
    cam.snap_to(b, p, 174.0f, ground_zoom(160.0f, 600.0f, 0.25f));
    const ViewXf xf = make_view(cam.focus(), 160.0f, 600.0f, 0.25f, 480, 174, cam.drop());
    const ScreenPt plane = xf.at(b.x, b.y, b.z);
    CHECK(std::abs(plane.x - 240.0f) <= 15.0f);
    CHECK(plane.x < 240.0f); // behind the middle, looking ahead
    CHECK(xf.at(b.x, b.y, 0.0f).y - plane.y == doctest::Approx(56.0f).epsilon(0.05));
}
