#pragma once

#include "core/body.hpp"
#include "core/iso.hpp"
#include "core/world.hpp"

namespace wof {

// How far up the gap from the shadow to the plane the view centres (0 the ground, 1 the plane).
inline constexpr float kCameraAim = 0.6f;

// The plane's half height on screen: the camera keeps at least this much room above it.
inline constexpr float kCameraPlaneHalf = 24.0f;

// The camera's numbers (tuning.txt, `camera.*`).
struct CameraParams {
    float lead_u = 40.0f;   // how far ahead of the plane, along its heading, the view looks
    float lead_ease = 2.0f; // how fast the lead swings round after a turn (per second)

    friend constexpr bool operator==(const CameraParams&, const CameraParams&) = default;
};

// Follows one plane (design/flight.md, World and view): the view aims a little above halfway up
// the gap from the shadow to the plane, so as you climb the plane rides higher on screen and the
// gap keeps growing even though the ground shrinks. In a short view (above the dashboard) it never
// lets the plane off the top. It leads a little in the direction of
// flight. It steps with the simulation's ticks and remembers the last two, so drawing can
// interpolate between them.
class Camera {
  public:
    // Starts following `b` with the lead already settled, as at the start of a run.
    void snap_to(const Body& b, const CameraParams& p, float view_h = 270.0f) noexcept;

    // One tick of following: the lead eases toward `lead_u` along the heading.
    void follow(const Body& b, const CameraParams& p, float view_h = 270.0f) noexcept;

    // The iso pixel the view centres on, `alpha` (0..1) of the way from the previous tick to this
    // one.
    [[nodiscard]] ScreenPt focus(float alpha = 1.0f) const noexcept;

    // The lead, in world units.
    [[nodiscard]] GroundPt lead() const noexcept { return lead_; }

  private:
    [[nodiscard]] ScreenPt aim(const Body& b, float view_h) const noexcept;

    GroundPt lead_{};
    ScreenPt previous_{};
    ScreenPt current_{};
};

// The lead a heading settles to.
[[nodiscard]] GroundPt settled_lead(float heading_deg, const CameraParams& p) noexcept;

struct PixelPt {
    int x = 0;
    int y = 0;

    friend constexpr bool operator==(PixelPt, PixelPt) = default;
};

// The iso pixel at the view's top-left for a focus and a view size, snapped to whole art pixels so
// the pixel art stays crisp.
[[nodiscard]] PixelPt view_origin(ScreenPt focus, int view_w, int view_h) noexcept;

// How big the ground is drawn at a height (design/planes.md): full size on the deck, shrinking
// steadily to `at_ceiling` at the ceiling, so up high you see more and plane and shadow stay in
// view. Everything in the world is drawn at this scale except the planes' sprites.
[[nodiscard]] constexpr float ground_zoom(float z, float ceiling, float at_ceiling) noexcept {
    const float t =
        ceiling > 0.0f ? (z <= 0.0f ? 0.0f : (z >= ceiling ? 1.0f : z / ceiling)) : 0.0f;
    return 1.0f - (1.0f - at_ceiling) * t;
}

} // namespace wof
