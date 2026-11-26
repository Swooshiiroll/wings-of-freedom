#include "core/fixed_step.hpp"

#include <cstdint>
#include <initializer_list>

#include <doctest/doctest.h>

using wof::FixedStep;

namespace {
constexpr std::int64_t kTick = 1'000'000'000 / 60; // 16 666 666 ns
}

TEST_CASE("FixedStep runs whole ticks and carries the remainder") {
    FixedStep step(kTick, 8);
    CHECK(step.advance(kTick / 2) == 0);
    CHECK(step.alpha() == doctest::Approx(0.5f).epsilon(0.001));
    CHECK(step.advance(kTick / 2) == 1);
    CHECK(step.advance(kTick * 2) == 2);
    CHECK(step.total_ticks() == 3u);
}

TEST_CASE("FixedStep is frame-rate independent") {
    // One simulated second at 30, 60, 144 and 240 fps gives the same tick count.
    for (const int fps : {30, 60, 144, 240}) {
        FixedStep step(kTick, 8);
        const std::int64_t frame = 1'000'000'000 / fps;
        int ticks = 0;
        for (int i = 0; i < fps; ++i) {
            ticks += step.advance(frame);
        }
        CAPTURE(fps);
        CHECK(ticks >= 59);
        CHECK(ticks <= 60);
    }
}

TEST_CASE("FixedStep caps a long stall instead of spiralling") {
    FixedStep step(kTick, 4);
    CHECK(step.advance(kTick * 100 + kTick / 4) == 4);
    // The backlog is dropped; only the fraction of the current tick is kept.
    CHECK(step.alpha() == doctest::Approx(0.25f).epsilon(0.001));
    CHECK(step.advance(0) == 0);
}

TEST_CASE("FixedStep ignores negative frame times") {
    FixedStep step(kTick, 4);
    CHECK(step.advance(-kTick) == 0);
    CHECK(step.alpha() == doctest::Approx(0.0f));
}
