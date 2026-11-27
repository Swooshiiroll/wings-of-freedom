#pragma once

#include "core/body.hpp"
#include "core/iso.hpp"
#include "core/world.hpp"

namespace wof {

// The camera's numbers (tuning.txt, `camera.*`).
struct CameraParams {
    float lead_u = 40.0f;   // how far ahead of the plane, along its heading, the view looks
    float lead_ease = 2.0f; // how fast the lead swings round after a turn (per second)

    friend constexpr bool operator==(const CameraParams&, const CameraParams&) = default;
};

// Follows one plane (design/flight.md, World and view): the view centres halfway between the
// plane and its shadow, and leads a little in the direction of flight. It steps with the
// simulation's ticks and remembers the last two, so drawing can interpolate between them.
class Camera {
  public:
    // Starts following `b` with the lead already settled, as at the start of a run.
    void snap_to(const Body& b, const CameraParams& p) noexcept;

    // One tick of following: the lead eases toward `lead_u` along the heading.
    void follow(const Body& b, const CameraParams& p) noexcept;

    // The iso pixel the view centres on, `alpha` (0..1) of the way from the previous tick to this
    // one.
    [[nodiscard]] ScreenPt focus(float alpha = 1.0f) const noexcept;

    // The lead, in world units.
    [[nodiscard]] GroundPt lead() const noexcept { return lead_; }

  private:
    [[nodiscard]] ScreenPt aim(const Body& b) const noexcept;

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

} // namespace wof
