#pragma once

#include <algorithm>
#include <cmath>

#include "core/iso.hpp"

namespace wof {

// Something a full ceiling above you is drawn this much bigger (design/planes.md, Height).
inline constexpr float kMaxGrowth = 1.5f;

// How big something at height `z` is drawn when the player's plane (the camera's eye) is at
// `eye_z` (design/planes.md, Height): full size at the player's height, shrinking the further
// below it is, down to `at_ceiling` for the ground seen from the ceiling, and growing the further
// above, up to kMaxGrowth.
[[nodiscard]] constexpr float depth_scale(float z, float eye_z, float ceiling,
                                          float at_ceiling) noexcept {
    if (ceiling <= 0.0f) {
        return 1.0f;
    }
    const float below = (eye_z - z) / ceiling; // in ceilings; negative when above
    const float k =
        below >= 0.0f ? 1.0f - (1.0f - at_ceiling) * below : 1.0f - (kMaxGrowth - 1.0f) * below;
    return std::clamp(k, at_ceiling, kMaxGrowth);
}

// The plane sprites' sizes (design/planes.md): a depth scale rounded to the nearest 1/8, from 1/4
// to 3/2, each baked as crisp pixel art. Returned as eighths (2..12).
inline constexpr int kMinSizeStep = 2;
inline constexpr int kFullSizeStep = 8;
inline constexpr int kMaxSizeStep = 12;

[[nodiscard]] inline int size_step(float k) noexcept {
    return std::clamp(static_cast<int>(std::lround(k * 8.0f)), kMinSizeStep, kMaxSizeStep);
}

// Where the world lands in the view (design/planes.md, Height). The iso projection is the
// parallel projection along the camera's axis, so the world at each height is a copy of itself
// scaled about the focus: a point lands at centre + (iso(x, y, z) − focus) × depth_scale(z).
// Your plane's height is drawn full size, the ground (and everything else) by its depth.
struct ViewXf {
    ScreenPt focus;           // the iso pixel the world is scaled about
    float eye_z = 0.0f;       // the player's height: drawn full size
    float ceiling = 600.0f;   // where the ground is smallest
    float at_ceiling = 0.25f; // the ground's size seen from the ceiling
    float cx = 0.0f;          // where the focus is drawn, in view pixels: the view's middle,
    float cy = 0.0f;          // or a little above or below it (core/camera.hpp, framing_of)

    [[nodiscard]] float scale_at(float z) const noexcept {
        return depth_scale(z, eye_z, ceiling, at_ceiling);
    }

    // A world point (x east, y north, z up, in u) in view pixels.
    [[nodiscard]] ScreenPt at(float x, float y, float z) const noexcept {
        const ScreenPt s = to_screen(x, y, z);
        const float k = scale_at(z);
        return {(s.x - focus.x) * k + cx, (s.y - focus.y) * k + cy};
    }
};

// The view for a focus, the player's height and a view size. The focus is snapped so that the
// player's height lands on whole pixels (crisp pixel art at full size).
[[nodiscard]] inline ViewXf make_view(ScreenPt focus, float eye_z, float ceiling, float at_ceiling,
                                      int view_w, int view_h, float drop = 0.0f) noexcept {
    ViewXf v;
    v.cx = static_cast<float>(view_w) * 0.5f;
    v.cy = static_cast<float>(view_h) * 0.5f + drop;
    v.focus = {std::round(focus.x - v.cx) + v.cx, std::round(focus.y - v.cy) + v.cy};
    v.eye_z = eye_z;
    v.ceiling = ceiling;
    v.at_ceiling = at_ceiling;
    return v;
}

} // namespace wof
