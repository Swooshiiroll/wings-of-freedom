#pragma once

#include <cmath>

namespace wof {

// The 2:1 true isometric projection (design/flight.md, World and view). World x runs east, y
// north and z up, in units (u). Iso pixels: x right, y down, at the art's 1x scale.
//
//   sx = x + y
//   sy = (x - y) / 2 - 0.4 z
//
// North comes out up-right and south (home) down-left, every ground line runs at 2:1, and one unit
// of ground is about one art pixel. Height is drawn as the gap between a plane and its shadow.

inline constexpr float kHeightPxPerU = 0.4f;
inline constexpr int kHeadings = 32; // plane sprites are drawn at one heading every 11.25 degrees

struct ScreenPt {
    float x = 0.0f;
    float y = 0.0f;
};

struct GroundPt {
    float x = 0.0f;
    float y = 0.0f;
};

struct GroundCell {
    int x = 0;
    int y = 0;

    friend constexpr bool operator==(GroundCell, GroundCell) = default;
};

// A world point to iso pixels.
[[nodiscard]] constexpr ScreenPt to_screen(float x, float y, float z = 0.0f) noexcept {
    return {x + y, (x - y) * 0.5f - kHeightPxPerU * z};
}

// The ground point under an iso pixel position (the inverse at height 0).
[[nodiscard]] constexpr GroundPt to_ground(float sx, float sy) noexcept {
    return {sx * 0.5f + sy, sx * 0.5f - sy};
}

// Rounds towards minus infinity, for whole numbers of either sign.
[[nodiscard]] constexpr int floor_div(int a, int b) noexcept {
    const int q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

// The whole ground unit under the centre of iso pixel (sx, sy). Integer maths only, so ground
// drawing is exact: at the centre (sx + 0.5, sy + 0.5), x = (2 sx + 4 sy + 3) / 4 and
// y = (2 sx - 4 sy - 1) / 4.
[[nodiscard]] constexpr GroundCell ground_of_pixel(int sx, int sy) noexcept {
    return {floor_div(2 * sx + 4 * sy + 3, 4), floor_div(2 * sx - 4 * sy - 1, 4)};
}

// A compass heading in degrees (0 north, 90 east) folded into [0, 360).
[[nodiscard]] inline float wrap_heading(float deg) noexcept {
    const float w = std::fmod(deg, 360.0f);
    return w < 0.0f ? w + 360.0f : w;
}

// Which of the 32 sprite headings a heading is drawn with (0 = north, counting clockwise).
[[nodiscard]] inline int heading_index(float deg) noexcept {
    const auto i = static_cast<int>(std::lround(wrap_heading(deg) / (360.0f / kHeadings)));
    return i % kHeadings;
}

} // namespace wof
