#include "core/ground.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

#include "core/hash.hpp"
#include "core/iso.hpp"
#include "core/map.hpp"

namespace wof {
namespace {

// Hash salts, fixed forever: changing one changes every map (and the golden hashes).
constexpr std::uint64_t kSaltOrigin = 0x6f726967696eull;  // "origin"
constexpr std::uint64_t kSaltRoadNS = 0x726f61646e73ull;  // "roadns"
constexpr std::uint64_t kSaltRoadEW = 0x726f61646577ull;  // "roadew"
constexpr std::uint64_t kSaltBlock = 0x626c6f636bull;     // "block"
constexpr std::uint64_t kSaltKind = 0x6b696e64ull;        // "kind"
constexpr std::uint64_t kSaltHedge = 0x6865646765ull;     // "hedge"
constexpr std::uint64_t kSaltRiver = 0x7269766572ull;     // "river"
constexpr std::uint64_t kSaltRiverBase = 0x7269766261ull; // "rivba"

constexpr std::int64_t kQ16 = 65536;

[[nodiscard]] int floor_mod(int a, int b) noexcept {
    const int m = a % b;
    return m < 0 ? m + b : m;
}

// A hash's value in [0, n).
[[nodiscard]] int below(std::uint64_t h, int n) noexcept {
    return static_cast<int>(h % static_cast<std::uint64_t>(n));
}

// The whole square root, rounded down (integer maths, the same everywhere).
[[nodiscard]] std::uint64_t isqrt(std::uint64_t v) noexcept {
    std::uint64_t r = 0;
    std::uint64_t bit = 1ull << 62u;
    while (bit > v) {
        bit >>= 2u;
    }
    while (bit != 0) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1u) + bit;
        } else {
            r >>= 1u;
        }
        bit >>= 2u;
    }
    return r;
}

// Where the block grid starts, so it doesn't line up with the battle area's edges.
struct Origin {
    int x = 0;
    int y = 0;
};
[[nodiscard]] Origin grid_origin(const GroundParams& p, std::uint64_t seed) noexcept {
    return {below(hash2(seed, 0, 0, kSaltOrigin), p.block_u),
            below(hash2(seed, 1, 0, kSaltOrigin), p.block_u)};
}

// True if (x, y) is on a road. Roads run along the block grid's lines; each stretch between two
// crossings is there unless its hash makes it one of the missing ones.
[[nodiscard]] bool on_road(const GroundParams& p, std::uint64_t seed, int x, int y) noexcept {
    if (p.road_u <= 0) {
        return false;
    }
    const Origin o = grid_origin(p, seed);
    const int lo = p.road_u / 2;  // road units before the line
    const int hi = p.road_u - lo; // and after it
    const int lx = floor_mod(x - o.x, p.block_u);
    const int ly = floor_mod(y - o.y, p.block_u);
    const int bx = floor_div(x - o.x, p.block_u);
    const int by = floor_div(y - o.y, p.block_u);
    // A north-south road on grid line k, in the stretch of block row `by`.
    if (lx < hi || lx >= p.block_u - lo) {
        const int k = lx < hi ? bx : bx + 1;
        if (below(hash2(seed, k, by, kSaltRoadNS), 100) >= p.road_skip_pct) {
            return true;
        }
    }
    // An east-west road on grid line j, in the stretch of block column `bx`.
    if (ly < hi || ly >= p.block_u - lo) {
        const int j = ly < hi ? by : by + 1;
        if (below(hash2(seed, bx, j, kSaltRoadEW), 100) >= p.road_skip_pct) {
            return true;
        }
    }
    return false;
}

// The river's bend points: one every river_step_u north, each up to river_swing_u east or west of
// the river's line (somewhere in the middle half of the battle area, by seed).
[[nodiscard]] int bend_x(const GroundParams& p, std::uint64_t seed, int k) noexcept {
    const int base = kMapWidthU / 2 + below(hash2(seed, 0, 0, kSaltRiverBase), kMapWidthU / 2 + 1) -
                     kMapWidthU / 4;
    const int swing = p.river_swing_u;
    return base + below(hash2(seed, k, 0, kSaltRiver), 2 * swing + 1) - swing;
}

// The river's centre at y and its slope there (east per unit north, in 1/65536ths). Between bend
// points it follows an S-curve (smoothstep), so it winds instead of zigzagging.
struct RiverAt {
    int x = 0;
    std::int64_t slope_q16 = 0;
};
[[nodiscard]] RiverAt river_at(const GroundParams& p, std::uint64_t seed, int y) noexcept {
    const int step = p.river_step_u;
    const int k = floor_div(y, step);
    const std::int64_t t = y - static_cast<std::int64_t>(k) * step;
    const std::int64_t s = step;
    const std::int64_t x0 = bend_x(p, seed, k);
    const std::int64_t dx = bend_x(p, seed, k + 1) - x0;
    const std::int64_t s3 = s * s * s;
    const std::int64_t eased = t * t * (3 * s - 2 * t); // 0 .. s^3
    const std::int64_t slope = dx * 6 * t * (s - t);    // d(eased * dx) / dt
    return {static_cast<int>(x0 + dx * eased / s3), slope * kQ16 / s3};
}

} // namespace

const char* ground_name(Ground g) noexcept {
    switch (g) {
    case Ground::Wheat:
        return "wheat";
    case Ground::Barley:
        return "barley";
    case Ground::Ploughed:
        return "ploughed";
    case Ground::Pasture:
        return "pasture";
    case Ground::Meadow:
        return "meadow";
    case Ground::Woods:
        return "woods";
    case Ground::Hedge:
        return "hedge";
    case Ground::Road:
        return "road";
    case Ground::River:
        return "river";
    case Ground::Bank:
        return "bank";
    }
    return "?";
}

bool validate(GroundParams& p) noexcept {
    const GroundParams before = p;
    p.block_u = std::clamp(p.block_u, 64, 4096);
    p.road_u = std::clamp(p.road_u, 0, 32);
    p.road_skip_pct = std::clamp(p.road_skip_pct, 0, 100);
    p.field_min_u = std::clamp(p.field_min_u, 8, 1024);
    p.field_max_u = std::clamp(p.field_max_u, 2 * p.field_min_u, 4096);
    p.hedge_u = std::clamp(p.hedge_u, 0, 16);
    p.hedge_missing_pct = std::clamp(p.hedge_missing_pct, 0, 100);
    p.row_u = std::clamp(p.row_u, 1, 64);
    p.river_u = std::clamp(p.river_u, 0, 400);
    p.river_bank_u = std::clamp(p.river_bank_u, 0, 64);
    p.river_step_u = std::clamp(p.river_step_u, 32, 1024);
    p.river_swing_u = std::clamp(p.river_swing_u, 0, 2048);
    int total = 0;
    for (int& w : p.weights) {
        w = std::clamp(w, 0, 1000);
        total += w;
    }
    if (total == 0) {
        p.weights[static_cast<std::size_t>(Ground::Pasture)] = 1;
    }
    return p == before;
}

FieldRect field_at(const GroundParams& p, std::uint64_t seed, int x, int y) noexcept {
    const Origin o = grid_origin(p, seed);
    const int bx = floor_div(x - o.x, p.block_u);
    const int by = floor_div(y - o.y, p.block_u);
    FieldRect f{o.x + bx * p.block_u, o.y + by * p.block_u, p.block_u, p.block_u,
                hash2(seed, bx, by, kSaltBlock)};
    // Split the longer side at a hashed place until both sides are at most field_max_u. Every
    // cut leaves at least field_min_u on each side (field_max_u >= 2 x field_min_u).
    while (f.w > p.field_max_u || f.h > p.field_max_u) {
        const bool across_x = f.w > f.h || (f.w == f.h && (f.id & 1u) != 0);
        const int len = across_x ? f.w : f.h;
        const int cut = p.field_min_u + below(f.id >> 1u, len - 2 * p.field_min_u + 1);
        const bool first = across_x ? x < f.x + cut : y < f.y + cut;
        if (across_x) {
            f.x = first ? f.x : f.x + cut;
            f.w = first ? cut : f.w - cut;
        } else {
            f.y = first ? f.y : f.y + cut;
            f.h = first ? cut : f.h - cut;
        }
        f.id = splitmix64(f.id ^ (first ? 0x51u : 0xa3u));
    }
    return f;
}

Ground field_kind(const GroundParams& p, const FieldRect& f) noexcept {
    int total = 0;
    for (const int w : p.weights) {
        total += w;
    }
    int r = below(splitmix64(f.id ^ kSaltKind), std::max(1, total));
    for (int i = 0; i < kFieldKinds; ++i) {
        r -= p.weights[static_cast<std::size_t>(i)];
        if (r < 0) {
            return static_cast<Ground>(i);
        }
    }
    return Ground::Pasture;
}

int river_centre_x(const GroundParams& p, std::uint64_t seed, int y) noexcept {
    return river_at(p, seed, y).x;
}

GroundSample sample_ground(const GroundParams& p, std::uint64_t seed, int x, int y) noexcept {
    if (on_road(p, seed, x, y)) {
        return {Ground::Road, false, 0}; // bridges too: roads are drawn over the river
    }
    if (p.river_u > 0) {
        const RiverAt r = river_at(p, seed, y);
        // Distance across the river, not along x: scaled by 1 / sqrt(1 + slope^2).
        const std::int64_t across = std::llabs(static_cast<std::int64_t>(x) - r.x);
        const std::int64_t d =
            across * kQ16 /
            static_cast<std::int64_t>(isqrt(static_cast<std::uint64_t>(kQ16 * kQ16) +
                                            static_cast<std::uint64_t>(r.slope_q16 * r.slope_q16)));
        const int half = p.river_u / 2;
        if (d < half) {
            return {Ground::River, false, 0};
        }
        if (d < half + p.river_bank_u) {
            return {Ground::Bank, false, 0};
        }
    }
    const FieldRect f = field_at(p, seed, x, y);
    const Ground kind = field_kind(p, f);
    const int fx = x - f.x;
    const int fy = y - f.y;
    if (kind != Ground::Woods) {
        // The west and south sides' hedges, each there unless it's one of the missing ones.
        const bool west =
            fx < p.hedge_u && below(splitmix64(f.id ^ kSaltHedge), 100) >= p.hedge_missing_pct;
        const bool south = fy < p.hedge_u &&
                           below(splitmix64(f.id ^ (kSaltHedge + 1)), 100) >= p.hedge_missing_pct;
        if (west || south) {
            return {Ground::Hedge, false, 0};
        }
    }
    const auto tint = static_cast<std::uint32_t>(f.id >> 32u);
    bool row = false;
    if (kind == Ground::Wheat || kind == Ground::Barley || kind == Ground::Ploughed) {
        // Rows run along the field's long side.
        row = ((f.w >= f.h ? fy : fx) / p.row_u) % 2 == 1;
    }
    return {kind, row, tint};
}

} // namespace wof
