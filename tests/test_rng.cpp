#include "core/rng.hpp"

#include <array>
#include <cstdint>

#include <doctest/doctest.h>

using wof::Rng;

TEST_CASE("Rng matches the PCG32 reference output") {
    // pcg32-demo from pcg-random.org: pcg32_srandom_r(&rng, 42u, 54u).
    Rng rng(42u, 54u);
    const std::array<std::uint32_t, 6> expected = {0xa15c02b7u, 0x7b47f409u, 0xba1d3330u,
                                                   0x83d2f293u, 0xbfa4784bu, 0xcbed606eu};
    for (const std::uint32_t value : expected) {
        CHECK(rng.next_u32() == value);
    }
}

TEST_CASE("Rng replays the same sequence from the same seed") {
    Rng a(123456789u);
    Rng b(123456789u);
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(a.next_u32() == b.next_u32());
    }
}

TEST_CASE("Rng streams with the same seed differ") {
    Rng a(7u, 1u);
    Rng b(7u, 2u);
    int same = 0;
    for (int i = 0; i < 100; ++i) {
        same += a.next_u32() == b.next_u32() ? 1 : 0;
    }
    CHECK(same < 5);
}

TEST_CASE("Rng ranges stay in bounds") {
    Rng rng(99u);
    for (int i = 0; i < 10000; ++i) {
        const std::uint32_t r = rng.next_below(7u);
        REQUIRE(r < 7u);
        const float f = rng.next_float();
        REQUIRE(f >= 0.0f);
        REQUIRE(f < 1.0f);
    }
}
