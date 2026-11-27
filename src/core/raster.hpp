#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/fighter.hpp"

namespace wof {

// Bakes a blocky model into pixel-art sprites (design/flight.md, Look): a small software
// rasterizer, so the planes are drawn at every heading, pitch and bank without drawing each one by
// hand. Pure maths: the front end uploads the pixels.
//
// The projection is the view the ground uses (core/iso.hpp): north up, looked at from 30° above,
// and the model's own height at 1.22 px per u, so the plane looks solid. (Altitude is drawn
// squeezed, at 0.4 px per u, as the gap to the shadow.)

inline constexpr float kModelHeightPxPerU = 1.2247449f; // sqrt(2) x cos(30°)

// A plane's attitude, in degrees: compass heading, nose-up pitch, right-wing-down bank.
struct Pose {
    float heading_deg = 0.0f;
    float pitch_deg = 0.0f;
    float bank_deg = 0.0f;
};

// Turns a point in the plane's frame into the world's (x east, y north, z up): bank about the nose,
// then pitch about the wings, then heading about the vertical.
[[nodiscard]] Vec3 pose_point(Vec3 p, const Pose& pose) noexcept;

// A sprite: w x h pixels, 0xAARRGGBB, and which block of the model each pixel shows (for tests):
// -1 empty, -2 outline.
struct Sprite {
    int w = 0;
    int h = 0;
    std::vector<std::uint32_t> pixels;
    std::vector<std::int16_t> block;

    [[nodiscard]] std::uint32_t at(int x, int y) const noexcept {
        return pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                      static_cast<std::size_t>(x)];
    }
    [[nodiscard]] int block_at(int x, int y) const noexcept {
        return block[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                     static_cast<std::size_t>(x)];
    }
};

inline constexpr std::uint32_t kOutline = 0xff1a1c14u;
inline constexpr std::uint32_t kShadow = 0x60000000u; // black at 38%

// Draws the model's blocks whose part is in `parts`, posed, with its origin at pixel (ox, oy) of a
// w x h sprite: z-buffered, flat-shaded by a fixed sun from the upper left, with a 1 px dark
// outline.
[[nodiscard]] Sprite render_model(const std::vector<Block>& model, const Pose& pose, int w, int h,
                                  float ox, float oy, std::uint32_t parts = kAllParts);

// The model's shadow: the posed model flattened onto the ground, as a see-through dark shape with
// its origin (the point under the plane) at pixel (ox, oy).
[[nodiscard]] Sprite render_shadow(const std::vector<Block>& model, const Pose& pose, int w, int h,
                                   float ox, float oy, std::uint32_t parts = kAllParts);

} // namespace wof
