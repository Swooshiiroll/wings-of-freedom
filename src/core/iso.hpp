#pragma once

#include <cmath>
#include <numbers>

namespace wof {

// The view (design/flight.md, World and view): north is straight up the screen, east to the right,
// looked at from 30° above (2:1). World x runs east, y north and z up, in units (u). Screen pixels:
// x right, y down, at the art's 1x scale.
//
//   sx = sqrt(2) x
//   sy = -sqrt(2)/2 y - 0.4 z
//
// One unit of ground is about one art pixel. Height is drawn as the gap between a plane and its
// shadow. The farmland's own field axes run diagonally across this (ground_of_pixel), so it looks
// isometric.

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
    constexpr float kRoot2 = std::numbers::sqrt2_v<float>;
    return {kRoot2 * x, -0.5f * kRoot2 * y - kHeightPxPerU * z};
}

// The ground point under a screen pixel position (the inverse at height 0).
[[nodiscard]] constexpr GroundPt to_ground(float sx, float sy) noexcept {
    constexpr float kRoot2 = std::numbers::sqrt2_v<float>;
    return {sx / kRoot2, -sy * kRoot2};
}

// Rounds towards minus infinity, for whole numbers of either sign.
[[nodiscard]] constexpr int floor_div(int a, int b) noexcept {
    const int q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

// The farmland unit (core/ground.hpp) under the centre of screen pixel (sx, sy). The farmland has
// its own axes, running diagonally across the screen, so its fields look isometric. Integer maths
// only, so ground drawing is exact on every compiler: at the centre (sx + 0.5, sy + 0.5),
// x = (2 sx + 4 sy + 3) / 4 and y = (2 sx - 4 sy - 1) / 4.
[[nodiscard]] constexpr GroundCell ground_of_pixel(int sx, int sy) noexcept {
    return {floor_div(2 * sx + 4 * sy + 3, 4), floor_div(2 * sx - 4 * sy - 1, 4)};
}

// The farmland unit drawn at the ground point under world (x, y).
[[nodiscard]] inline GroundCell farmland_of(float x, float y) noexcept {
    const ScreenPt s = to_screen(x, y);
    return ground_of_pixel(static_cast<int>(std::floor(s.x)), static_cast<int>(std::floor(s.y)));
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
