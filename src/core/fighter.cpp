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

namespace wof {
namespace {

constexpr Rgb8 kRlmGrey{176, 179, 168}; // the He 51's pre-war light grey
constexpr Rgb8 kGreyMetal{140, 143, 136};
constexpr Rgb8 kDark{44, 44, 42};

// A thin upright strut from (x, y, z0) to (x, y, z1).
Block strut(float x, float y, float z0, float z1, Part part) {
    return box(x - 0.15f, x + 0.15f, y - 0.2f, y + 0.2f, z0, z1, kDark, part);
}

// A straight biplane wing from the root at x = root to the tip at x = tip, its chord from y_back
// to y_front, its underside at z at the root, `thick` deep, the tip raised by `dihedral`.
Block slab(float root, float tip, float y_back, float y_front, float z, float thick, float dihedral,
           Part part, Rgb8 colour = kRlmGrey) {
    Block k;
    k.corners = {{{root, y_back, z},
                  {tip, y_back, z + dihedral},
                  {tip, y_front, z + dihedral},
                  {root, y_front, z},
                  {root, y_back, z + thick},
                  {tip, y_back, z + thick + dihedral},
                  {tip, y_front, z + thick + dihedral},
                  {root, y_front, z + thick}}};
    k.colour = colour;
    k.part = part;
    return k;
}

// A monoplane's low wing, tapering from the root (chord back0..front0) to the tip (back1..front1),
// from z `bottom` to `top` at the root, the tip raised by `dihedral`.
Block low_wing(float root, float tip, float back0, float front0, float back1, float front1,
               float bottom, float top, float dihedral, Part part, Rgb8 colour) {
    Block k;
    k.corners = {{{root, back0, bottom},
                  {tip, back1, bottom + dihedral},
                  {tip, front1, bottom + dihedral},
                  {root, front0, bottom},
                  {root, back0, top},
                  {tip, back1, top + dihedral},
                  {tip, front1, top + dihedral},
                  {root, front0, top}}};
    k.colour = colour;
    k.part = part;
    return k;
}

constexpr Rgb8 kArmyOlive{92, 98, 56}; // the US Army's pre-war scheme, as on the P-26
constexpr Rgb8 kArmyOliveDark{70, 76, 44};
constexpr Rgb8 kArmyYellow{224, 182, 46};
constexpr Rgb8 kSilver{182, 186, 192}; // bare metal
constexpr Rgb8 kSilverDark{128, 132, 140};
constexpr Rgb8 kRlm02{118, 128, 104};   // the Ar 65's grey-green
constexpr Rgb8 kRlmGreen{90, 99, 84};   // the Bf 109B's dark grey-green, clear of the fields
constexpr Rgb8 kRlmBlue{146, 172, 196}; // its pale blue underside

// A fin: an upright blade on the tail, its back at y0 and front at y1, `height` tall.
Block fin_blade(float y0, float y1, float height, Rgb8 colour) {
    return taper_y(y0, 0.22f, 0.4f, height, y1, 0.22f, 0.4f, 0.6f, colour, Part::Tail);
}

// An open cockpit: windscreen at `y` and the pilot's head behind it.
void open_cockpit(std::vector<Block>& m, float y) {
    m.push_back(box(-0.6f, 0.6f, y - 0.4f, y, 1.3f, 1.9f, kGlass, Part::Cockpit));
    m.push_back(box(-0.45f, 0.45f, y - 1.5f, y - 0.7f, 1.3f, 2.3f, kLeather, Part::Cockpit));
}

// Fixed landing gear: a leg and a wheel each side, at x = +-`x`.
void fixed_gear(std::vector<Block>& m, float x) {
    for (const float side : {-1.0f, 1.0f}) {
        const float at = side * x;
        m.push_back(box(at - 0.25f, at + 0.25f, 2.2f, 2.7f, -2.8f, -1.0f, kDark, Part::Fuselage));
        m.push_back(box(at - 0.3f, at + 0.3f, 1.6f, 3.3f, -3.8f, -2.4f, kDark, Part::Fuselage));
    }
}

} // namespace

// The Boeing P-12E: a short, stubby biplane with a radial engine, the upper wing a little longer,
// in the Army's olive drab and chrome yellow. About 23 u across and 16 u long.
std::vector<Block> p12_model() {
    std::vector<Block> m;
    m.push_back(box(-1.7f, 1.7f, 4.4f, 6.6f, -1.1f, 1.5f, kArmyOliveDark, Part::Engine));
    m.push_back(box(-1.2f, 1.2f, 4.4f, 6.6f, -1.55f, 1.95f, kArmyOliveDark, Part::Engine));
    m.push_back(box(-1.35f, 1.35f, 6.6f, 6.85f, -1.0f, 1.4f, kDark, Part::Engine));
    m.push_back(box(-0.45f, 0.45f, 6.85f, 7.7f, -0.25f, 0.65f, kSpinnerColour, Part::Engine));
    m.push_back(box(-1.1f, 1.1f, -1.5f, 4.4f, -1.0f, 1.3f, kArmyOlive, Part::Fuselage));
    m.push_back(
        taper_y(-1.5f, 1.1f, -1.0f, 1.3f, -8.0f, 0.35f, -0.1f, 0.6f, kArmyOlive, Part::Tail));
    for (const float side : {-1.0f, 1.0f}) {
        const Part part = side < 0.0f ? Part::LeftWing : Part::RightWing;
        m.push_back(
            slab(side * 1.0f, side * 10.2f, -0.8f, 2.2f, -1.2f, 0.8f, 0.5f, part, kArmyYellow));
        m.push_back(
            slab(side * 0.3f, side * 11.7f, 0.6f, 3.8f, 2.8f, 0.8f, 0.0f, part, kArmyYellow));
        m.push_back(strut(side * 7.0f, 1.4f, -0.3f, 2.8f, part));
        m.push_back(strut(side * 0.8f, 2.0f, 1.3f, 2.8f, Part::Fuselage));
    }
    m.push_back(box(-3.8f, 3.8f, -8.4f, -6.6f, 0.1f, 0.4f, kArmyYellow, Part::Tail));
    m.push_back(fin_blade(-8.8f, -6.6f, 2.8f, kArmyYellow));
    open_cockpit(m, -0.6f);
    fixed_gear(m, 2.1f);
    return m;
}

// The Arado Ar 65E: a big biplane with a long inline engine, a deep chin radiator and two struts a
// side, all in grey-green. About 29 u across and 22 u long.
std::vector<Block> ar65_model() {
    std::vector<Block> m;
    m.push_back(
        taper_y(5.0f, 1.25f, -1.1f, 1.3f, 9.3f, 0.85f, -0.6f, 0.9f, kGreyMetal, Part::Engine));
    m.push_back(box(-0.9f, 0.9f, 4.8f, 7.6f, -2.5f, -1.1f, kDark, Part::Engine));
    m.push_back(box(-0.45f, 0.45f, 9.3f, 10.2f, -0.2f, 0.6f, kSpinnerColour, Part::Engine));
    m.push_back(box(-1.15f, 1.15f, -2.0f, 5.0f, -1.1f, 1.3f, kRlm02, Part::Fuselage));
    m.push_back(taper_y(-2.0f, 1.15f, -1.1f, 1.3f, -11.0f, 0.35f, -0.1f, 0.6f, kRlm02, Part::Tail));
    for (const float side : {-1.0f, 1.0f}) {
        const Part part = side < 0.0f ? Part::LeftWing : Part::RightWing;
        m.push_back(slab(side * 1.1f, side * 12.6f, -1.4f, 2.2f, -1.2f, 0.8f, 0.5f, part, kRlm02));
        m.push_back(slab(side * 0.3f, side * 14.3f, 0.2f, 4.2f, 3.0f, 0.8f, 0.0f, part, kRlm02));
        m.push_back(strut(side * 6.0f, 1.2f, -0.4f, 3.0f, part));
        m.push_back(strut(side * 10.0f, 1.2f, -0.2f, 3.0f, part));
        m.push_back(strut(side * 0.8f, 1.8f, 1.3f, 3.0f, Part::Fuselage));
    }
    m.push_back(box(-4.8f, 4.8f, -11.0f, -8.8f, 0.1f, 0.4f, kRlm02, Part::Tail));
    m.push_back(fin_blade(-11.5f, -8.8f, 3.0f, kRlm02));
    open_cockpit(m, -0.7f);
    fixed_gear(m, 2.2f);
    return m;
}

// The Curtiss P-36A Hawk: a low-wing monoplane with a big radial, an enclosed canopy and its gear
// up; bare metal with yellow wings and tail. About 29 u across and 22 u long.
std::vector<Block> p36_model() {
    std::vector<Block> m;
    m.push_back(box(-1.8f, 1.8f, 5.8f, 8.8f, -1.3f, 1.7f, kSilverDark, Part::Engine));
    m.push_back(box(-1.3f, 1.3f, 5.8f, 8.8f, -1.75f, 2.15f, kSilverDark, Part::Engine));
    m.push_back(box(-1.4f, 1.4f, 8.8f, 9.05f, -1.1f, 1.6f, kDark, Part::Engine));
    m.push_back(box(-0.5f, 0.5f, 9.05f, 10.1f, -0.2f, 0.8f, kSpinnerColour, Part::Engine));
    m.push_back(box(-1.25f, 1.25f, -2.0f, 5.8f, -1.2f, 1.4f, kSilver, Part::Fuselage));
    m.push_back(
        taper_y(-2.0f, 1.25f, -1.2f, 1.4f, -11.0f, 0.35f, -0.1f, 0.7f, kSilver, Part::Tail));
    m.push_back(low_wing(-1.2f, -14.5f, -1.0f, 4.4f, 0.4f, 2.8f, -1.1f, -0.5f, 1.2f, Part::LeftWing,
                         kArmyYellow));
    m.push_back(low_wing(1.2f, 14.5f, -1.0f, 4.4f, 0.4f, 2.8f, -1.1f, -0.5f, 1.2f, Part::RightWing,
                         kArmyYellow));
    m.push_back(box(-5.0f, 5.0f, -11.4f, -9.2f, 0.15f, 0.45f, kArmyYellow, Part::Tail));
    m.push_back(fin_blade(-12.0f, -8.8f, 3.6f, kArmyYellow));
    // The canopy, its fairing running back to the tail, and the pilot inside.
    m.push_back(taper_y(3.2f, 0.65f, 1.4f, 2.4f, -0.2f, 0.65f, 1.4f, 2.6f, kGlass, Part::Cockpit));
    m.push_back(box(-0.4f, 0.4f, -0.2f, 0.6f, 1.4f, 2.7f, kLeather, Part::Cockpit));
    m.push_back(
        taper_y(-0.2f, 0.55f, 1.4f, 2.5f, -4.2f, 0.2f, 1.4f, 1.5f, kSilver, Part::Fuselage));
    return m;
}

// The Messerschmitt Bf 109B: a slim low-wing monoplane with a pointed inline nose, a framed
// canopy and square-cut wingtips; dark green over a pale blue belly. About 25 u across and 22 u
// long.
std::vector<Block> bf109_model() {
    std::vector<Block> m;
    m.push_back(
        taper_y(5.5f, 1.1f, -1.0f, 1.2f, 9.8f, 0.65f, -0.5f, 0.8f, kRlmGreen, Part::Engine));
    m.push_back(box(-0.55f, 0.55f, 5.6f, 8.0f, -1.7f, -0.9f, kDark, Part::Engine));
    m.push_back(box(-0.45f, 0.45f, 9.8f, 10.8f, -0.25f, 0.65f, kSpinnerColour, Part::Engine));
    m.push_back(box(-1.05f, 1.05f, -2.0f, 5.5f, 0.0f, 1.3f, kRlmGreen, Part::Fuselage));
    m.push_back(box(-1.05f, 1.05f, -2.0f, 5.5f, -1.0f, 0.0f, kRlmBlue, Part::Fuselage));
    m.push_back(
        taper_y(-2.0f, 1.05f, -1.0f, 1.3f, -10.5f, 0.3f, -0.1f, 0.6f, kRlmGreen, Part::Tail));
    m.push_back(low_wing(-1.0f, -12.65f, -0.6f, 3.8f, 0.6f, 2.8f, -1.0f, -0.45f, 1.0f,
                         Part::LeftWing, kRlmGreen));
    m.push_back(low_wing(1.0f, 12.65f, -0.6f, 3.8f, 0.6f, 2.8f, -1.0f, -0.45f, 1.0f,
                         Part::RightWing, kRlmGreen));
    m.push_back(box(-4.2f, 4.2f, -10.4f, -8.6f, 0.3f, 0.6f, kRlmGreen, Part::Tail));
    m.push_back(fin_blade(-11.0f, -8.8f, 3.2f, kRlmGreen));
    m.push_back(box(-0.55f, 0.55f, -0.2f, 2.8f, 1.3f, 2.2f, kGlass, Part::Cockpit));
    m.push_back(
        taper_y(-0.2f, 0.5f, 1.3f, 2.1f, -4.0f, 0.2f, 1.3f, 1.45f, kRlmGreen, Part::Fuselage));
    return m;
}

std::vector<Block> he51_model() {
    std::vector<Block> m;
    // The inline engine's long, pointed nose, a radiator underneath, and the spinner.
    m.push_back(
        taper_y(5.5f, 1.2f, -1.0f, 1.3f, 10.0f, 0.6f, -0.35f, 0.7f, kGreyMetal, Part::Engine));
    m.push_back(box(-0.7f, 0.7f, 5.0f, 7.0f, -2.0f, -1.0f, kDark, Part::Engine));
    m.push_back(box(-0.35f, 0.35f, 10.0f, 11.0f, -0.15f, 0.55f, kSpinnerColour, Part::Engine));
    // Fuselage and tail cone.
    m.push_back(box(-1.1f, 1.1f, -2.0f, 5.5f, -1.0f, 1.3f, kRlmGrey, Part::Fuselage));
    m.push_back(
        taper_y(-2.0f, 1.1f, -1.0f, 1.3f, -11.0f, 0.35f, -0.1f, 0.6f, kRlmGrey, Part::Tail));
    // Biplane wings: the lower ones from the fuselage, the upper ones above on struts, staggered
    // forward. Each side's upper and lower wing (and its struts) are one part.
    // The slabs are 0.8 u thick (thinner ones break up edge-on), and the lower wings have a
    // little dihedral, so a wing seen edge-on still reads as a line.
    m.push_back(slab(-1.1f, -12.5f, -1.0f, 3.0f, -1.1f, 0.8f, 0.6f, Part::LeftWing));
    m.push_back(slab(1.1f, 12.5f, -1.0f, 3.0f, -1.1f, 0.8f, 0.6f, Part::RightWing));
    m.push_back(slab(-0.3f, -14.0f, 0.2f, 4.4f, 3.0f, 0.8f, 0.0f, Part::LeftWing));
    m.push_back(slab(0.3f, 14.0f, 0.2f, 4.4f, 3.0f, 0.8f, 0.0f, Part::RightWing));
    m.push_back(strut(-8.5f, 1.6f, -0.1f, 3.0f, Part::LeftWing));
    m.push_back(strut(8.5f, 1.6f, -0.1f, 3.0f, Part::RightWing));
    m.push_back(strut(-0.8f, 1.8f, 1.3f, 3.0f, Part::Fuselage)); // the cabane
    m.push_back(strut(0.8f, 1.8f, 1.3f, 3.0f, Part::Fuselage));
    // Tailplane and fin.
    m.push_back(box(-4.5f, 4.5f, -10.6f, -8.4f, 0.1f, 0.4f, kRlmGrey, Part::Tail));
    m.push_back(taper_y(-11.2f, 0.22f, 0.4f, 3.0f, -8.4f, 0.22f, 0.4f, 0.6f, kRlmGrey, Part::Tail));
    // The open cockpit, behind the upper wing.
    m.push_back(box(-0.6f, 0.6f, -0.6f, -0.2f, 1.3f, 1.9f, kGlass, Part::Cockpit));
    m.push_back(box(-0.45f, 0.45f, -1.7f, -0.9f, 1.3f, 2.3f, kLeather, Part::Cockpit));
    // Fixed landing gear: legs and wheels.
    m.push_back(box(-2.4f, -1.9f, 3.0f, 3.5f, -3.0f, -1.0f, kDark, Part::Fuselage));
    m.push_back(box(1.9f, 2.4f, 3.0f, 3.5f, -3.0f, -1.0f, kDark, Part::Fuselage));
    m.push_back(box(-2.6f, -2.0f, 2.4f, 4.1f, -4.0f, -2.6f, kDark, Part::Fuselage));
    m.push_back(box(2.0f, 2.6f, 2.4f, 4.1f, -4.0f, -2.6f, kDark, Part::Fuselage));
    return m;
}

std::vector<Block> plane_model(ModelId id) {
    switch (id) {
    case ModelId::He51:
        return he51_model();
    case ModelId::P12:
        return p12_model();
    case ModelId::Ar65:
        return ar65_model();
    case ModelId::P36:
        return p36_model();
    case ModelId::Bf109:
        return bf109_model();
    case ModelId::P26:
        break;
    }
    return fighter_model();
}

std::vector<Block> scaled_model(std::vector<Block> model, float s) {
    for (Block& b : model) {
        for (Vec3& c : b.corners) {
            c = {c.x * s, c.y * s, c.z * s};
        }
    }
    return model;
}

} // namespace wof
