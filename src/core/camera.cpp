#include "core/camera.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace wof {

GroundPt settled_lead(float heading_deg, const CameraParams& p) noexcept {
    const float h = heading_deg * (std::numbers::pi_v<float> / 180.0f);
    return {std::sin(h) * p.lead_u, std::cos(h) * p.lead_u};
}

ScreenPt Camera::aim(const Body& b, float view_h) const noexcept {
    // Up the gap between the plane (at z) and its shadow (at 0)...
    ScreenPt focus = to_screen(b.x + lead_.x, b.y + lead_.y, b.z * kCameraAim);
    // ...but never so far below the plane that it leaves the top of the view.
    const float lowest = to_screen(b.x, b.y, b.z).y + view_h * 0.5f - kCameraPlaneHalf;
    focus.y = std::min(focus.y, lowest);
    return focus;
}

void Camera::snap_to(const Body& b, const CameraParams& p, float view_h) noexcept {
    lead_ = settled_lead(b.heading_deg, p);
    current_ = aim(b, view_h);
    previous_ = current_;
}

void Camera::follow(const Body& b, const CameraParams& p, float view_h) noexcept {
    const GroundPt target = settled_lead(b.heading_deg, p);
    const float k = 1.0f - std::exp(-p.lead_ease * kTickSeconds);
    lead_.x += (target.x - lead_.x) * k;
    lead_.y += (target.y - lead_.y) * k;
    previous_ = current_;
    current_ = aim(b, view_h);
}

ScreenPt Camera::focus(float alpha) const noexcept {
    return {previous_.x + (current_.x - previous_.x) * alpha,
            previous_.y + (current_.y - previous_.y) * alpha};
}

PixelPt view_origin(ScreenPt focus, int view_w, int view_h) noexcept {
    return {static_cast<int>(std::lround(focus.x - static_cast<float>(view_w) * 0.5f)),
            static_cast<int>(std::lround(focus.y - static_cast<float>(view_h) * 0.5f))};
}

} // namespace wof
