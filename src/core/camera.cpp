#include "core/camera.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace wof {

GroundPt settled_lead(float heading_deg, const CameraParams& p) noexcept {
    const float h = heading_deg * (std::numbers::pi_v<float> / 180.0f);
    return {std::sin(h) * p.lead_u, std::cos(h) * p.lead_u};
}

Framing framing_of(float z, float view_h, float ground_k) noexcept {
    const float height = std::max(0.0f, -to_screen(0.0f, 0.0f, z).y); // in iso pixels
    // The shadow's gap with the world scaled about your plane itself.
    const float shadow = height * ground_k;
    // Low down, the focus is up the gap from the shadow toward the plane.
    const float natural = height * (1.0f - kCameraAim);
    const float room = std::max(0.0f, view_h * 0.5f - kCameraPlaneHalf);
    const float rise = std::min({natural, kCameraRise, room});
    // The gap is `shadow + below * (1 - ground_k)`: hold it once it's wide enough.
    const float shrink = 1.0f - ground_k;
    float below = natural;
    if (shrink > 0.001f && shadow + below * shrink > kCameraGap) {
        below = std::max(0.0f, (kCameraGap - shadow) / shrink);
    }
    return {below, below - rise};
}

void Camera::aim(const Body& b, float view_h, float ground_k) noexcept {
    const Framing f = framing_of(b.z, view_h, ground_k);
    current_ = to_screen(b.x + lead_.x, b.y + lead_.y, b.z);
    current_.y += f.below;
    current_drop_ = f.drop;
}

void Camera::snap_to(const Body& b, const CameraParams& p, float view_h, float ground_k) noexcept {
    lead_ = settled_lead(b.heading_deg, p);
    aim(b, view_h, ground_k);
    previous_ = current_;
    previous_drop_ = current_drop_;
}

void Camera::follow(const Body& b, const CameraParams& p, float view_h, float ground_k) noexcept {
    const GroundPt target = settled_lead(b.heading_deg, p);
    const float k = 1.0f - std::exp(-p.lead_ease * kTickSeconds);
    lead_.x += (target.x - lead_.x) * k;
    lead_.y += (target.y - lead_.y) * k;
    previous_ = current_;
    previous_drop_ = current_drop_;
    aim(b, view_h, ground_k);
}

float Camera::drop(float alpha) const noexcept {
    return previous_drop_ + (current_drop_ - previous_drop_) * alpha;
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
