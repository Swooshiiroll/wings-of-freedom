#include "core/view.hpp"

#include "core/camera.hpp"
#include "core/view_xf.hpp"

#include <doctest/doctest.h>

using namespace wof;

TEST_CASE("fit_view uses the largest whole scale and shows more world on uneven screens") {
    CHECK(fit_view(1920, 1080) == ViewFit{4, 480, 270});
    CHECK(fit_view(2560, 1440) == ViewFit{5, 512, 288});
    CHECK(fit_view(1280, 800) == ViewFit{2, 640, 400}); // Steam Deck
    CHECK(fit_view(3840, 2160) == ViewFit{8, 480, 270});
    CHECK(fit_view(1366, 768) == ViewFit{2, 683, 384}); // the partial last pixel is cropped
    CHECK(fit_view(960, 540) == ViewFit{2, 480, 270});
    CHECK(fit_view(1440, 810) == ViewFit{3, 480, 270});
}

TEST_CASE("fit_view never goes below 1x") {
    CHECK(fit_view(400, 200) == ViewFit{1, 400, 200}); // smaller than the art: shows less
    CHECK(fit_view(0, 0) == ViewFit{1, 1, 1});
    CHECK(fit_view(5000, 100) == ViewFit{1, 5000, 100});
}

TEST_CASE("start_window is the largest 480x270 multiple that fits with a title bar") {
    CHECK(start_window(1920, 1040) == WindowSize{1440, 810}); // 1080p minus a taskbar
    CHECK(start_window(2560, 1400) == WindowSize{2400, 1350});
    CHECK(start_window(1280, 800) == WindowSize{960, 540});
    CHECK(start_window(3840, 2120) == WindowSize{3360, 1890});
    CHECK(start_window(600, 300) == WindowSize{480, 270}); // at least 1x
}

TEST_CASE("Depth: full size at your height, a quarter for the deck from the ceiling, 1.5x above") {
    CHECK(depth_scale(300.0f, 300.0f, 600.0f, 0.25f) == doctest::Approx(1.0f));
    CHECK(depth_scale(0.0f, 600.0f, 600.0f, 0.25f) == doctest::Approx(0.25f));
    CHECK(depth_scale(300.0f, 600.0f, 600.0f, 0.25f) == doctest::Approx(0.625f));
    CHECK(depth_scale(600.0f, 0.0f, 600.0f, 0.25f) == doctest::Approx(1.5f));
    CHECK(depth_scale(2000.0f, 0.0f, 600.0f, 0.25f) == doctest::Approx(1.5f));
    CHECK(depth_scale(-500.0f, 600.0f, 600.0f, 0.25f) == doctest::Approx(0.25f));
    for (const float eye : {0.0f, 150.0f, 450.0f, 600.0f}) {
        CHECK(depth_scale(0.0f, eye, 600.0f, 0.25f) ==
              doctest::Approx(ground_zoom(eye, 600.0f, 0.25f)));
    }
}

TEST_CASE("Sprite sizes round to the nearest eighth, from a quarter to 1.5x") {
    CHECK(size_step(1.0f) == 8);
    CHECK(size_step(0.93f) == 7);
    CHECK(size_step(0.25f) == 2);
    CHECK(size_step(0.1f) == 2);
    CHECK(size_step(1.5f) == 12);
    CHECK(size_step(3.0f) == 12);
}

TEST_CASE("The view scales the world about its centre, by each point's depth") {
    const ScreenPt focus = to_screen(1000.0f, 2000.0f, 600.0f * 0.6f);
    const ViewXf xf = make_view(focus, 600.0f, 600.0f, 0.25f, 480, 270);
    const ScreenPt centre = xf.at(1000.0f, 2000.0f, 360.0f);
    CHECK(centre.x == doctest::Approx(240.0f).epsilon(0.005));
    // A ground point is drawn a quarter as far from the centre as it is in iso pixels.
    const ScreenPt s = to_screen(1100.0f, 2000.0f, 0.0f);
    const ScreenPt g = xf.at(1100.0f, 2000.0f, 0.0f);
    CHECK(g.x - xf.cx == doctest::Approx((s.x - xf.focus.x) * 0.25f));
    CHECK(g.y - xf.cy == doctest::Approx((s.y - xf.focus.y) * 0.25f));
    // A point at your height isn't scaled.
    const ScreenPt s2 = to_screen(1100.0f, 2000.0f, 600.0f);
    CHECK(xf.at(1100.0f, 2000.0f, 600.0f).x - xf.cx == doctest::Approx(s2.x - xf.focus.x));
    // A length at a height scales with that height's depth.
    CHECK(xf.at(1010.0f, 2000.0f, 300.0f).x - xf.at(1000.0f, 2000.0f, 300.0f).x ==
          doctest::Approx(10.0f * 0.625f));
}
