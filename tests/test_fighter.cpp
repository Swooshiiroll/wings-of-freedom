#include "core/fighter.hpp"
#include "core/raster.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <vector>

#include <doctest/doctest.h>

using namespace wof;

namespace {
constexpr int kCell = 48;
constexpr float kOrigin = 24.0f;

struct Centre {
    float x = 0.0f;
    float y = 0.0f;
    int count = 0;
};

// The average position of the pixels showing blocks that pass `pick`.
template <typename Pick> Centre centre_of(const Sprite& s, const std::vector<Block>& m, Pick pick) {
    Centre c;
    for (int y = 0; y < s.h; ++y) {
        for (int x = 0; x < s.w; ++x) {
            const int id = s.block_at(x, y);
            if (id >= 0 && pick(m[static_cast<std::size_t>(id)])) {
                c.x += static_cast<float>(x);
                c.y += static_cast<float>(y);
                ++c.count;
            }
        }
    }
    if (c.count > 0) {
        c.x /= static_cast<float>(c.count);
        c.y /= static_cast<float>(c.count);
    }
    return c;
}

Sprite plane(const std::vector<Block>& m, float heading, float pitch, float bank) {
    return render_model(m, Pose{heading, pitch, bank}, kCell, kCell, kOrigin, kOrigin);
}

// Where the nose is drawn: the spinner rendered on its own (in the whole plane it's often behind
// the cowling, seen from the south-east).
Centre nose_at(float heading, float pitch) {
    std::vector<Block> spinner;
    for (const Block& b : fighter_model()) {
        if (b.colour == kSpinnerColour) {
            spinner.push_back(b);
        }
    }
    REQUIRE(spinner.size() == 1u);
    return centre_of(plane(spinner, heading, pitch, 0.0f), spinner,
                     [](const Block&) { return true; });
}
} // namespace

TEST_CASE("Heading north, the nose points up-right on screen") {
    const Centre nose = nose_at(0.0f, 0.0f);
    REQUIRE(nose.count > 0);
    CHECK(nose.x > kOrigin + 4.0f);
    CHECK(nose.y < kOrigin - 1.0f);
}

TEST_CASE("Each heading points the nose its own way") {
    // North-east is screen right, south-west left, north-west up, south-east down.
    CHECK(nose_at(45.0f, 0.0f).x > kOrigin + 8.0f);
    CHECK(nose_at(225.0f, 0.0f).x < kOrigin - 8.0f);
    CHECK(nose_at(315.0f, 0.0f).y < kOrigin - 4.0f);
    CHECK(nose_at(135.0f, 0.0f).y > kOrigin + 4.0f);
}

TEST_CASE("Pitch shows: the nose rises when climbing and drops when diving") {
    for (const float heading : {0.0f, 90.0f, 200.0f, 315.0f}) {
        CAPTURE(heading);
        const Centre level = nose_at(heading, 0.0f);
        CHECK(nose_at(heading, 40.0f).y < level.y - 3.0f);
        CHECK(nose_at(heading, -40.0f).y > level.y + 3.0f);
        // And every 10 degrees between is a step on the way up.
        float last = nose_at(heading, -40.0f).y + 1.0f;
        for (int p = -40; p <= 40; p += 10) {
            const Centre c = nose_at(heading, static_cast<float>(p));
            CHECK(c.y < last);
            last = c.y;
        }
    }
}

TEST_CASE("Banking right puts the right wing down") {
    const auto m = fighter_model();
    const auto right = [](const Block& b) { return b.part == Part::RightWing; };
    const auto left = [](const Block& b) { return b.part == Part::LeftWing; };
    // Heading north-west, the nose points up the screen and the right wing to the right.
    const Sprite banked = plane(m, 315.0f, 0.0f, 30.0f);
    CHECK(centre_of(banked, m, right).y > centre_of(banked, m, left).y + 2.0f);
    const Sprite other = plane(m, 315.0f, 0.0f, -30.0f);
    CHECK(centre_of(other, m, right).y < centre_of(other, m, left).y - 2.0f);
    const Sprite level = plane(m, 315.0f, 0.0f, 0.0f);
    CHECK(centre_of(level, m, right).y ==
          doctest::Approx(centre_of(level, m, left).y).epsilon(0.05));
}

TEST_CASE("A dark outline surrounds the plane") {
    const auto m = fighter_model();
    const Sprite s = plane(m, 30.0f, 10.0f, 0.0f);
    int outline = 0;
    for (int y = 0; y < s.h; ++y) {
        for (int x = 0; x < s.w; ++x) {
            const auto filled = [&](int nx, int ny) {
                return nx >= 0 && ny >= 0 && nx < s.w && ny < s.h && s.block_at(nx, ny) >= 0;
            };
            if (s.block_at(x, y) == -2) {
                ++outline;
                CHECK(s.at(x, y) == kOutline);
                CHECK(
                    (filled(x - 1, y) || filled(x + 1, y) || filled(x, y - 1) || filled(x, y + 1)));
            }
            if (s.block_at(x, y) >= 0) {
                // Nothing drawn touches an empty pixel: the outline is always between.
                for (const auto& [dx, dy] : {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < s.w && ny < s.h) {
                        CHECK(s.block_at(nx, ny) != -1);
                    }
                }
            }
        }
    }
    CHECK(outline > 20);
}

TEST_CASE("The whole plane fits its 48 px cell at every pose") {
    const auto m = fighter_model();
    for (int h = 0; h < 32; ++h) {
        for (int p = -40; p <= 40; p += 20) {
            for (const float b : {-30.0f, 0.0f, 30.0f}) {
                const Sprite s = plane(m, static_cast<float>(h) * 11.25f, static_cast<float>(p), b);
                for (int i = 0; i < kCell; ++i) {
                    // Nothing on the border rows and columns, so nothing was cut off.
                    REQUIRE(s.block_at(i, 0) == -1);
                    REQUIRE(s.block_at(i, kCell - 1) == -1);
                    REQUIRE(s.block_at(0, i) == -1);
                    REQUIRE(s.block_at(kCell - 1, i) == -1);
                }
            }
        }
    }
}

TEST_CASE("The shadow is the plane flattened: see-through, and shorter when pitched") {
    const auto m = fighter_model();
    const auto count = [](const Sprite& s) {
        int n = 0;
        for (const std::uint32_t px : s.pixels) {
            if (px != 0u) {
                CHECK(px == kShadow);
                ++n;
            }
        }
        return n;
    };
    const int level =
        count(render_shadow(m, Pose{0.0f, 0.0f, 0.0f}, kCell, kCell, kOrigin, kOrigin));
    const int steep =
        count(render_shadow(m, Pose{0.0f, 40.0f, 0.0f}, kCell, kCell, kOrigin, kOrigin));
    CHECK(level > 60);
    CHECK(steep < level);
}

TEST_CASE("A plane can be drawn with a part missing") {
    const auto m = fighter_model();
    const auto left = [](const Block& b) { return b.part == Part::LeftWing; };
    const Sprite whole = render_model(m, Pose{}, kCell, kCell, kOrigin, kOrigin);
    const Sprite broken = render_model(m, Pose{}, kCell, kCell, kOrigin, kOrigin,
                                       kAllParts & ~part_bit(Part::LeftWing));
    CHECK(centre_of(whole, m, left).count > 0);
    CHECK(centre_of(broken, m, left).count == 0);
}

namespace {
// Where the He 51's nose is drawn: its spinner on its own.
Centre he51_nose_at(float heading, float pitch) {
    std::vector<Block> spinner;
    for (const Block& b : he51_model()) {
        if (b.colour == kSpinnerColour) {
            spinner.push_back(b);
        }
    }
    REQUIRE(spinner.size() == 1u);
    return centre_of(plane(spinner, heading, pitch, 0.0f), spinner,
                     [](const Block&) { return true; });
}
} // namespace

TEST_CASE("The He 51 points its nose along its heading, and its pitch shows") {
    CHECK(he51_nose_at(45.0f, 0.0f).x > kOrigin + 8.0f);
    CHECK(he51_nose_at(225.0f, 0.0f).x < kOrigin - 8.0f);
    CHECK(he51_nose_at(315.0f, 0.0f).y < kOrigin - 4.0f);
    for (const float heading : {0.0f, 120.0f, 250.0f}) {
        CAPTURE(heading);
        CHECK(he51_nose_at(heading, 40.0f).y < he51_nose_at(heading, 0.0f).y - 3.0f);
        CHECK(he51_nose_at(heading, -40.0f).y > he51_nose_at(heading, 0.0f).y + 3.0f);
    }
}

TEST_CASE("The He 51 is a biplane: each side's upper and lower wings are one part") {
    const auto m = he51_model();
    int left = 0;
    int right = 0;
    float top = -100.0f;
    float bottom = 100.0f;
    for (const Block& b : m) {
        left += b.part == Part::LeftWing ? 1 : 0;
        right += b.part == Part::RightWing ? 1 : 0;
        if (b.part == Part::LeftWing) {
            for (const Vec3& c : b.corners) {
                top = std::max(top, c.z);
                bottom = std::min(bottom, c.z);
            }
        }
    }
    CHECK(left == 3); // upper, lower and the strut between
    CHECK(right == 3);
    CHECK(top - bottom > 4.0f);
    // Losing the left wing takes both its wings off.
    const auto lw = [](const Block& b) { return b.part == Part::LeftWing; };
    const Sprite broken = render_model(m, Pose{}, kCell, kCell, kOrigin, kOrigin,
                                       kAllParts & ~part_bit(Part::LeftWing));
    CHECK(centre_of(broken, m, lw).count == 0);
    CHECK(plane_model(ModelId::He51).size() == m.size());
    CHECK(plane_model(ModelId::P26).size() == fighter_model().size());
}

TEST_CASE("The He 51 fits its 48 px cell at every pose") {
    const auto m = he51_model();
    for (int h = 0; h < 32; ++h) {
        for (int p = -40; p <= 40; p += 20) {
            for (const float b : {-30.0f, 0.0f, 30.0f}) {
                const Sprite s = plane(m, static_cast<float>(h) * 11.25f, static_cast<float>(p), b);
                for (int i = 0; i < kCell; ++i) {
                    REQUIRE(s.block_at(i, 0) == -1);
                    REQUIRE(s.block_at(i, kCell - 1) == -1);
                    REQUIRE(s.block_at(0, i) == -1);
                    REQUIRE(s.block_at(kCell - 1, i) == -1);
                }
            }
        }
    }
}

TEST_CASE("A scaled model fits its cell at the smallest and largest sprite sizes") {
    for (const ModelId id : {ModelId::P26, ModelId::He51}) {
        for (const int step : {2, 4, 8, 12}) {
            const int cell = 6 * step;
            const std::vector<Block> m =
                scaled_model(plane_model(id), static_cast<float>(step) / 8.0f);
            const int canvas = cell * 3; // drawn on a bigger canvas, nothing may fall outside
            const int lo = cell;
            const int hi = cell * 2;
            for (int h = 0; h < 32; h += 3) {
                for (int p = -40; p <= 40; p += 40) {
                    for (const float bank : {-30.0f, 0.0f, 30.0f}) {
                        const Sprite s = render_model(
                            m, Pose{static_cast<float>(h) * 11.25f, static_cast<float>(p), bank},
                            canvas, canvas, static_cast<float>(canvas) * 0.5f,
                            static_cast<float>(canvas) * 0.5f);
                        int outside = 0;
                        for (int y = 0; y < canvas; ++y) {
                            for (int x = 0; x < canvas; ++x) {
                                const bool in = x >= lo && x < hi && y >= lo && y < hi;
                                outside += !in && s.block_at(x, y) != -1 ? 1 : 0;
                            }
                        }
                        CAPTURE(static_cast<int>(id));
                        CAPTURE(step);
                        CAPTURE(h);
                        REQUIRE(outside == 0);
                    }
                }
            }
        }
    }
}
