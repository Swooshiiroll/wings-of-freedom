#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace wof {

// A point in a plane's own frame, in u: x toward the right wingtip, y forward (the nose), z up.
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Rgb8 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    friend constexpr bool operator==(Rgb8, Rgb8) = default;
};

// The parts that take damage (design/flight.md, Damage). Each block of a model belongs to one, so
// a damaged plane can be drawn with a part missing.
enum class Part : std::uint8_t { Engine, LeftWing, RightWing, Fuselage, Tail, Cockpit };
inline constexpr std::uint32_t kAllParts = 0x3fu;

[[nodiscard]] constexpr std::uint32_t part_bit(Part p) noexcept {
    return 1u << static_cast<std::uint32_t>(p);
}

// One solid block of a model: a six-sided shape given by its 8 corners. Corners 0-3 are one end
// (x0y0, x1y0, x1y1, x0y1 going round), 4-7 the other end in the same order, so boxes, tapers and
// skews are all the same thing.
struct Block {
    std::array<Vec3, 8> corners{};
    Rgb8 colour{};
    Part part = Part::Fuselage;
};

// An upright box.
[[nodiscard]] Block box(float x0, float x1, float y0, float y1, float z0, float z1, Rgb8 colour,
                        Part part) noexcept;

// The placeholder fighter (design/flight.md, Look), a blocky original model in the shape of the
// Boeing P-26 "Peashooter": low wing with a little dihedral, a big round radial cowling, wheel
// spats under the wings, an open cockpit with a headrest. Olive drab fuselage, yellow wings and
// tail, no markings. About 22 u across and 18 u long. Real art replaces it in v0.3.
[[nodiscard]] std::vector<Block> fighter_model();

// The colour of the propeller spinner, the nose's very front (tests find the nose by it).
inline constexpr Rgb8 kSpinnerColour{196, 60, 44};

} // namespace wof
