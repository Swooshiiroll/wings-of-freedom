#pragma once

#include "core/body.hpp"
#include "core/iso.hpp"
#include "core/world.hpp"

namespace wof {

// How far up the gap from the shadow to the plane the view centres low down (0 the ground, 1 the
// plane), until your plane has ridden kCameraRise above the middle.
inline constexpr float kCameraAim = 0.6f;

// Your plane stays near the middle of the view: it rides at most this far above it as you climb...
inline constexpr float kCameraRise = 30.0f;
// ...and the gap down to its shadow grows to this, then holds.
inline constexpr float kCameraGap = 100.0f;

// The plane's half height on screen: the camera keeps at least this much room above it.
inline constexpr float kCameraPlaneHalf = 24.0f;

// How the view sits around your plane at a height (design/flight.md, World and view). The world
// is drawn scaled by depth about one point, the focus (core/view_xf.hpp): the further below your
// plane that point is, the wider the shadow gap.
struct Framing {
    float below = 0.0f; // the focus is this far below your plane (iso pixels)
    float drop = 0.0f;  // and is drawn this far below the view's middle
};
// `view_h` is the view's height; `ground_k` the ground's size at this height (ground_zoom).
[[nodiscard]] Framing framing_of(float z, float view_h, float ground_k) noexcept;

// The camera's numbers (tuning.txt, `camera.*`).
struct CameraParams {
    float lead_u = 10.0f;   // how far ahead of the plane, along its heading, the view looks
    float lead_ease = 2.0f; // how fast the lead swings round after a turn (per second)

    friend constexpr bool operator==(const CameraParams&, const CameraParams&) = default;
};

// Follows one plane (design/flight.md, World and view): the plane stays near the middle of the
// view, riding a little higher as you climb while the gap to its shadow widens, then holds
// (framing_of). It leads a little in the direction of flight. It steps with the simulation's ticks
// and remembers the last two, so drawing can interpolate between them.
class Camera {
  public:
    // Starts following `b` with the lead already settled, as at the start of a run.
    // `ground_k` is the ground's size at the plane's height (ground_zoom).
    void snap_to(const Body& b, const CameraParams& p, float view_h = 270.0f,
                 float ground_k = 1.0f) noexcept;

    // One tick of following: the lead eases toward `lead_u` along the heading.
    void follow(const Body& b, const CameraParams& p, float view_h = 270.0f,
                float ground_k = 1.0f) noexcept;

    // The iso pixel the world is drawn about, `alpha` (0..1) of the way from the previous tick to
    // this one, and how far below the view's middle it goes (make_view).
    [[nodiscard]] ScreenPt focus(float alpha = 1.0f) const noexcept;
    [[nodiscard]] float drop(float alpha = 1.0f) const noexcept;

    // The lead, in world units.
    [[nodiscard]] GroundPt lead() const noexcept { return lead_; }

  private:
    // Where the view is for the plane now.
    void aim(const Body& b, float view_h, float ground_k) noexcept;

    GroundPt lead_{};
    ScreenPt previous_{};
    ScreenPt current_{};
    float previous_drop_ = 0.0f;
    float current_drop_ = 0.0f;
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
