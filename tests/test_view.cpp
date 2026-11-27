#include "core/view.hpp"

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
