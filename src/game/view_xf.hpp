#pragma once

#include <SDL3/SDL.h>

#include <cmath>

#include "core/iso.hpp"

namespace wof::game {

// Where the world lands in the view (design/planes.md, Height): iso pixels around the camera's
// focus, scaled by the ground zoom, about the view's centre. Everything in the world is drawn
// through this except the planes' sprites, which stay full size at the point it gives.
struct ViewXf {
    ScreenPt focus;     // the iso pixel at the view's centre
    float zoom = 1.0f;  // 1 on the deck, smaller up high
    float cx = 0.0f;    // the view's centre, in view pixels
    float cy = 0.0f;

    // A world point (x east, y north, z up, in u) in view pixels.
    [[nodiscard]] SDL_FPoint at(float x, float y, float z) const noexcept {
        const ScreenPt s = to_screen(x, y, z);
        return {(s.x - focus.x) * zoom + cx, (s.y - focus.y) * zoom + cy};
    }
};

// The view for a focus, a zoom and a view size. The focus is snapped so that at full size the
// world lands on whole pixels (crisp pixel art).
[[nodiscard]] inline ViewXf make_view(ScreenPt focus, float zoom, int view_w, int view_h) noexcept {
    ViewXf v;
    v.cx = static_cast<float>(view_w) * 0.5f;
    v.cy = static_cast<float>(view_h) * 0.5f;
    v.focus = {std::round(focus.x - v.cx) + v.cx, std::round(focus.y - v.cy) + v.cy};
    v.zoom = zoom;
    return v;
}

} // namespace wof::game
