#include "core/ground.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <utility>

#include "core/hash.hpp"
#include "core/map.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {
constexpr std::uint64_t kSeed = 0x5eed2026ull;

// Hashes every sample of a square, so any change to the generator shows.
std::uint64_t hash_area(const GroundParams& p, std::uint64_t seed, int x0, int y0, int size) {
    Fnv1a h;
    for (int y = y0; y < y0 + size; ++y) {
        for (int x = x0; x < x0 + size; ++x) {
            const GroundSample s = sample_ground(p, seed, x, y);
            h.add_byte(static_cast<std::uint8_t>(s.kind));
            h.add_byte(s.row ? 1 : 0);
            h.add_u32(s.tint);
        }
    }
    return h.value();
}
} // namespace

TEST_CASE("The ground is the same on every compiler (golden hash)") {
    // Pinned on MSVC and GCC. If the generator changes on purpose, update this in the same pull
    // request and say why.
    const GroundParams p;
    CHECK(hash_area(p, kSeed, 2100, 2200, 512) == 0x2e1151cdb513e63bull); // river 0 runs through it
}

TEST_CASE("The same seed always gives the same ground, and another seed different ground") {
    const GroundParams p;
    CHECK(hash_area(p, kSeed, 0, 0, 128) == hash_area(p, kSeed, 0, 0, 128));
    CHECK(hash_area(p, kSeed, 0, 0, 128) != hash_area(p, kSeed + 1, 0, 0, 128));
}

TEST_CASE("The farmland has every kind of ground") {
    const GroundParams p;
    std::array<int, kGroundKinds> counts{};
    for (int y = 0; y < 2048; y += 2) {
        for (int x = 0; x < kRiverEveryU; x += 2) {
            ++counts[static_cast<std::size_t>(sample_ground(p, kSeed, x, y).kind)];
        }
    }
    for (int k = 0; k < static_cast<int>(Ground::Mud); ++k) { // the front line is below
        CAPTURE(ground_name(static_cast<Ground>(k)));
        CHECK(counts[static_cast<std::size_t>(k)] > 0);
    }
}

TEST_CASE("Fields stay between the minimum and maximum size") {
    const GroundParams p;
    for (int y = -3000; y < 6000; y += 37) {
        for (int x = -2000; x < 5000; x += 41) {
            const FieldRect f = field_at(p, kSeed, x, y);
            CAPTURE(x);
            CAPTURE(y);
            REQUIRE(f.w >= p.field_min_u);
            REQUIRE(f.w <= p.field_max_u);
            REQUIRE(f.h >= p.field_min_u);
            REQUIRE(f.h <= p.field_max_u);
            REQUIRE(x >= f.x);
            REQUIRE(x < f.x + f.w);
            REQUIRE(y >= f.y);
            REQUIRE(y < f.y + f.h);
        }
    }
}

TEST_CASE("Every unit of a field reports the same field") {
    const GroundParams p;
    const FieldRect f = field_at(p, kSeed, 1500, 1500);
    for (int y = f.y; y < f.y + f.h; y += 5) {
        for (int x = f.x; x < f.x + f.w; x += 5) {
            const FieldRect g = field_at(p, kSeed, x, y);
            REQUIRE(g.id == f.id);
            REQUIRE(g.x == f.x);
            REQUIRE(g.y == f.y);
        }
    }
}

TEST_CASE("A river winds through the middle of every strip of farmland") {
    const GroundParams p;
    for (const int n : {-1, 0, 2, 4}) {
        CAPTURE(n);
        const int west = n * kRiverEveryU;
        int lo = west + kRiverEveryU;
        int hi = west;
        for (int y = -1000; y < kMapDepthU + 1000; y += 16) {
            const int x = river_centre_x(p, kSeed, y, n);
            lo = x < lo ? x : lo;
            hi = x > hi ? x : hi;
            const Ground g = sample_ground(p, kSeed, x, y).kind;
            REQUIRE((g == Ground::River || g == Ground::Road)); // a road there is a bridge
        }
        CHECK(lo >= west + kRiverEveryU / 4 - p.river_swing_u);
        CHECK(hi <= west + kRiverEveryU * 3 / 4 + p.river_swing_u);
        CHECK(hi - lo > 40); // it bends
    }
}

TEST_CASE("The ground carries on past the battle area's edges") {
    const GroundParams p;
    std::array<int, kGroundKinds> counts{};
    for (int i = 0; i < 4000; ++i) {
        const int x = -20000 + i * 13;
        const int y = 30000 - i * 17;
        const GroundSample s = sample_ground(p, kSeed, x, y);
        REQUIRE(static_cast<int>(s.kind) < kGroundKinds);
        ++counts[static_cast<std::size_t>(s.kind)];
    }
    int field_kinds = 0;
    for (int k = 0; k < kFieldKinds; ++k) {
        field_kinds += counts[static_cast<std::size_t>(k)] > 0 ? 1 : 0;
    }
    CHECK(field_kinds >= 4);
}

TEST_CASE("Roads only run along the block grid") {
    GroundParams p;
    p.river_u = 0; // so a road sample can't be a bridge hiding a river
    int roads = 0;
    for (int y = 0; y < 2048; y += 3) {
        for (int x = 0; x < 2048; x += 3) {
            if (sample_ground(p, kSeed, x, y).kind != Ground::Road) {
                continue;
            }
            ++roads;
            // Grid lines are block edges, which are always field edges: a road unit sits within
            // half a road's width of the edge of the field under it.
            const FieldRect f = field_at(p, kSeed, x, y);
            const int after = p.road_u - p.road_u / 2;
            const int before = p.road_u / 2;
            const bool near_x = x - f.x < after || f.x + f.w - x <= before;
            const bool near_y = y - f.y < after || f.y + f.h - y <= before;
            CAPTURE(x);
            CAPTURE(y);
            REQUIRE((near_x || near_y));
        }
    }
    CHECK(roads > 0);
}

TEST_CASE("Field kinds follow the weights") {
    GroundParams p;
    p.weights = {0, 0, 0, 0, 0, 1}; // woods only
    for (int i = 0; i < 200; ++i) {
        const FieldRect f = field_at(p, kSeed, i * 97, i * 61);
        REQUIRE(field_kind(p, f) == Ground::Woods);
    }
}

TEST_CASE("validate keeps the ground's numbers usable") {
    GroundParams p;
    CHECK(validate(p)); // the defaults are fine
    p.field_min_u = 100;
    p.field_max_u = 120;
    p.weights = {0, 0, 0, 0, 0, 0};
    p.block_u = 1;
    CHECK_FALSE(validate(p));
    CHECK(p.field_max_u == 200);
    CHECK(p.weights[static_cast<std::size_t>(Ground::Pasture)] == 1);
    CHECK(p.block_u == 64);
}

TEST_CASE("The front line is a band of mud, trenches and shell holes halfway up the map") {
    const GroundParams p;
    std::array<int, kGroundKinds> on{};
    std::array<int, kGroundKinds> off{};
    for (int wx = 2000; wx < 4000; wx += 3) {
        for (int dw = -150; dw <= 150; dw += 3) {
            // The farmland unit under world (wx, wy): turned 45 degrees.
            const auto at = [](int x, int y) {
                const int fx = static_cast<int>(static_cast<float>(x - y) * 0.7071f);
                const int fy = static_cast<int>(static_cast<float>(x + y) * 0.7071f);
                return std::pair{fx, fy};
            };
            const auto [bx, by] = at(wx, kFrontU + dw);
            ++on[static_cast<std::size_t>(sample_ground(p, kSeed, bx, by).kind)];
            const auto [fx, fy] = at(wx, kFrontU - 1000 + dw);
            ++off[static_cast<std::size_t>(sample_ground(p, kSeed, fx, fy).kind)];
        }
    }
    for (const Ground g : {Ground::Mud, Ground::Trench, Ground::Crater}) {
        CAPTURE(ground_name(g));
        CHECK(on[static_cast<std::size_t>(g)] > 100);
        CHECK(off[static_cast<std::size_t>(g)] == 0);
    }
}
