#include "core/fighter.hpp"

namespace wof {
namespace {

constexpr Rgb8 kOlive{92, 98, 56};
constexpr Rgb8 kOliveDark{70, 76, 44};
constexpr Rgb8 kYellow{224, 182, 46};
constexpr Rgb8 kEngineFace{46, 46, 42};
constexpr Rgb8 kGlass{52, 66, 76};
constexpr Rgb8 kLeather{98, 68, 42};

// A block that runs along y from y0 to y1, with a different cross-section at each end: at y0 it
// spans x -w0..w0 and z b0..t0, at y1 x -w1..w1 and z b1..t1 (a taper).
Block taper_y(float y0, float w0, float b0, float t0, float y1, float w1, float b1, float t1,
              Rgb8 colour, Part part) {
    Block k;
    k.corners = {{{-w0, y0, b0},
                  {w0, y0, b0},
                  {w1, y1, b1},
                  {-w1, y1, b1},
                  {-w0, y0, t0},
                  {w0, y0, t0},
                  {w1, y1, t1},
                  {-w1, y1, t1}}};
    k.colour = colour;
    k.part = part;
    return k;
}

// One wing, from the root at x = root to the tip at x = tip (negative for the left wing): the
// chord runs from y_back to y_front at the root and narrows toward the tip, which is raised by
// `dihedral` u.
Block wing(float root, float tip, float dihedral, Part part) {
    constexpr float kRootBack = -0.6f;
    constexpr float kRootFront = 4.0f;
    constexpr float kTipBack = 0.4f;
    constexpr float kTipFront = 3.2f;
    constexpr float kBottom = -1.0f;
    constexpr float kTop = -0.55f;
    Block k;
    k.corners = {{{root, kRootBack, kBottom},
                  {tip, kTipBack, kBottom + dihedral},
                  {tip, kTipFront, kBottom + dihedral},
                  {root, kRootFront, kBottom},
                  {root, kRootBack, kTop},
                  {tip, kTipBack, kTop + dihedral},
                  {tip, kTipFront, kTop + dihedral},
                  {root, kRootFront, kTop}}};
    k.colour = kYellow;
    k.part = part;
    return k;
}

} // namespace

Block box(float x0, float x1, float y0, float y1, float z0, float z1, Rgb8 colour,
          Part part) noexcept {
    Block k;
    k.corners = {{{x0, y0, z0},
                  {x1, y0, z0},
                  {x1, y1, z0},
                  {x0, y1, z0},
                  {x0, y0, z1},
                  {x1, y0, z1},
                  {x1, y1, z1},
                  {x0, y1, z1}}};
    k.colour = colour;
    k.part = part;
    return k;
}

std::vector<Block> fighter_model() {
    std::vector<Block> m;
    // The radial engine: two crossed boxes read as a round cowling, a dark engine face, and the
    // spinner.
    m.push_back(box(-1.9f, 1.9f, 5.4f, 8.0f, -1.2f, 1.7f, kOliveDark, Part::Engine));
    m.push_back(box(-1.35f, 1.35f, 5.4f, 8.0f, -1.75f, 2.25f, kOliveDark, Part::Engine));
    m.push_back(box(-1.5f, 1.5f, 8.0f, 8.25f, -1.3f, 1.8f, kEngineFace, Part::Engine));
    m.push_back(box(-0.5f, 0.5f, 8.25f, 9.2f, -0.25f, 0.75f, kSpinnerColour, Part::Engine));
    // Fuselage, and the tail cone tapering back to the tail.
    m.push_back(box(-1.2f, 1.2f, -2.0f, 5.4f, -1.1f, 1.3f, kOlive, Part::Fuselage));
    m.push_back(taper_y(-2.0f, 1.2f, -1.1f, 1.3f, -9.0f, 0.4f, -0.1f, 0.6f, kOlive, Part::Tail));
    // The low wings, with a little dihedral.
    m.push_back(wing(-1.2f, -11.0f, 0.9f, Part::LeftWing));
    m.push_back(wing(1.2f, 11.0f, 0.9f, Part::RightWing));
    // Tailplane and fin.
    m.push_back(box(-4.0f, 4.0f, -9.0f, -6.8f, 0.15f, 0.45f, kYellow, Part::Tail));
    Block fin = taper_y(-9.2f, 0.22f, 0.4f, 3.3f, -6.4f, 0.22f, 0.4f, 0.6f, kYellow, Part::Tail);
    m.push_back(fin);
    // The open cockpit: windscreen, the pilot's head, and the headrest fairing behind.
    m.push_back(box(-0.75f, 0.75f, 1.8f, 2.3f, 1.3f, 2.1f, kGlass, Part::Cockpit));
    m.push_back(box(-0.45f, 0.45f, 0.4f, 1.2f, 1.3f, 2.35f, kLeather, Part::Cockpit));
    m.push_back(taper_y(0.2f, 0.4f, 1.3f, 2.4f, -3.6f, 0.2f, 1.3f, 1.45f, kOlive, Part::Fuselage));
    // The wheel spats under the wings.
    m.push_back(box(-3.1f, -2.2f, 0.8f, 3.6f, -3.0f, -1.0f, kOliveDark, Part::Fuselage));
    m.push_back(box(2.2f, 3.1f, 0.8f, 3.6f, -3.0f, -1.0f, kOliveDark, Part::Fuselage));
    return m;
}

} // namespace wof
