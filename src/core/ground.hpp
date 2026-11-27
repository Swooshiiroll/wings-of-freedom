#pragma once

#include <array>
#include <cstdint>

namespace wof {

// The ground under the battle (design/flight.md, World and view): flat in v0.1, drawn as World
// War II farmland. Rectangular fields of crops, plough and grass with hedgerows, woods, dirt
// roads and a river.
//
// There is no stored map. The ground is a pure function of the seed and a ground unit, defined
// everywhere, so it carries on past the battle area's edges unmarked. It uses integer maths and
// hashes only, so a seed gives exactly the same ground on every compiler: co-op clients will
// generate it themselves. The golden-hash test pins it on MSVC and GCC.
//
// Layout:
// - Roads cut the world into square blocks, with some stretches missing.
// - Each block is split again and again, at hashed places, into fields between the minimum and
//   maximum size.
// - Each field gets a kind by weight. Crop and plough fields have rows along their long side.
// - A hedgerow runs along each field's west and south sides (so one between any two fields),
//   unless the field is woods or the hedge is one of the missing ones.
// - One river winds north to south through the middle of the map, under bridges where roads
//   cross it.

enum class Ground : std::uint8_t {
    Wheat,
    Barley,
    Ploughed,
    Pasture,
    Meadow,
    Woods,
    Hedge,
    Road,
    River,
    Bank,
};
inline constexpr int kGroundKinds = 10;
inline constexpr int kFieldKinds = 6; // Wheat to Woods: what a field can be

[[nodiscard]] const char* ground_name(Ground g) noexcept;

// The ground's numbers (tuning.txt, `ground.*`), all (start) values, in world units.
struct GroundParams {
    int block_u = 512;          // road grid spacing
    int road_u = 4;             // road width
    int road_skip_pct = 25;     // share of road stretches that are missing
    int field_min_u = 64;       // a field's sides are at least this
    int field_max_u = 160;      // and at most this (kept at least twice the minimum)
    int hedge_u = 2;            // hedgerow width
    int hedge_missing_pct = 15; // share of field sides with no hedge
    int row_u = 4;              // crop and plough row width
    int river_u = 24;           // river width
    int river_bank_u = 3;       // bank width on each side
    int river_step_u = 256;     // the river bends once per this much distance north
    int river_swing_u = 128;    // and swings up to this far east or west at each bend
    // How often each field kind comes up, relative to the others: wheat, barley, ploughed,
    // pasture, meadow, woods.
    std::array<int, kFieldKinds> weights{20, 15, 15, 20, 15, 15};

    friend constexpr bool operator==(const GroundParams&, const GroundParams&) = default;
};

// Keeps every value in a usable range (and the maximum field size at least twice the minimum).
// Returns false if anything had to change.
bool validate(GroundParams& p) noexcept;

// A field: its rectangle in ground units and its id (a hash, also used to vary its shade).
struct FieldRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    std::uint64_t id = 0;
};

// The field the ground unit (x, y) belongs to (roads and the river drawn over the top of it).
[[nodiscard]] FieldRect field_at(const GroundParams& p, std::uint64_t seed, int x, int y) noexcept;

// The kind a field is.
[[nodiscard]] Ground field_kind(const GroundParams& p, const FieldRect& f) noexcept;

// What's on the ground at one unit.
struct GroundSample {
    Ground kind = Ground::Pasture;
    bool row = false;       // on the darker of a crop or plough field's alternating rows
    std::uint32_t tint = 0; // the field's hash, for varying shades between fields (0 elsewhere)
};
[[nodiscard]] GroundSample sample_ground(const GroundParams& p, std::uint64_t seed, int x,
                                         int y) noexcept;

// The river's centre line at y, in ground units east.
[[nodiscard]] int river_centre_x(const GroundParams& p, std::uint64_t seed, int y) noexcept;

} // namespace wof
