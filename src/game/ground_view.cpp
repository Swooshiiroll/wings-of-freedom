#include "game/ground_view.hpp"

#include <algorithm>
#include <array>
#include <limits>

#include "core/hash.hpp"
#include "core/iso.hpp"

namespace wof::game {
namespace {

struct Rgb {
    int r = 0;
    int g = 0;
    int b = 0;
};

// Placeholder palette (real art comes in v0.3): muted WWII European farmland seen from above.
constexpr std::array<Rgb, kGroundKinds> kPalette = {{
    {196, 168, 92},  // wheat
    {176, 174, 110}, // barley
    {122, 90, 62},   // ploughed
    {98, 140, 70},   // pasture
    {126, 156, 84},  // meadow
    {54, 88, 52},    // woods
    {56, 90, 50},    // hedge
    {164, 144, 110}, // road
    {62, 102, 138},  // river
    {116, 118, 90},  // bank
    {112, 92, 64},   // the front line's mud
    {62, 48, 34},    // trench
    {74, 58, 42},    // shell hole
}};

constexpr std::uint64_t kSaltSpeckle = 0x737065636bull; // "speck"
constexpr std::uint64_t kSaltCrown = 0x63726f776eull;   // "crown"

[[nodiscard]] Rgb shade(Rgb c, int d) noexcept {
    return {c.r + d, c.g + d, c.b + d};
}

[[nodiscard]] std::uint32_t pack(Rgb c) noexcept {
    const auto ch = [](int v) { return static_cast<std::uint32_t>(std::clamp(v, 0, 255)); };
    return 0xff000000u | (ch(c.r) << 16u) | (ch(c.g) << 8u) | ch(c.b);
}

// The colour of one ground unit: the kind's colour, varied per field, with rows, tree crowns and
// a little speckle so the ground has texture to show speed and direction.
[[nodiscard]] std::uint32_t colour(const GroundSample& s, int x, int y) noexcept {
    Rgb c = kPalette[static_cast<std::size_t>(s.kind)];
    const auto speck = static_cast<int>(hash2(0, x, y, kSaltSpeckle) & 0xffu);
    switch (s.kind) {
    case Ground::Wheat:
    case Ground::Barley:
    case Ground::Ploughed:
        c = shade(c, static_cast<int>(s.tint % 13u) - 6);
        if (s.row) {
            c = shade(c, s.kind == Ground::Ploughed ? -16 : -9);
        }
        c = shade(c, speck % 5 - 2);
        break;
    case Ground::Pasture:
    case Ground::Meadow:
        c = shade(c, static_cast<int>(s.tint % 13u) - 6);
        c = shade(c, (speck % 7 - 3) * 2);
        break;
    case Ground::Woods: {
        // Tree crowns in 3-unit clumps: lit, plain or in shadow.
        const auto crown = hash2(0, floor_div(x, 3), floor_div(y, 3), kSaltCrown) % 3u;
        c = shade(c, crown == 0 ? -12 : crown == 1 ? 9 : 0);
        c = shade(c, speck % 5 - 2);
        break;
    }
    case Ground::River:
        if (speck % 29 == 0) {
            c = shade(c, 22); // glints
        }
        break;
    case Ground::Hedge:
    case Ground::Road:
    case Ground::Bank:
        c = shade(c, speck % 7 - 3);
        break;
    case Ground::Mud:
        c = shade(c, (speck % 9 - 4) * 2);
        break;
    case Ground::Trench:
        c = shade(c, speck % 5 - 2);
        break;
    case Ground::Crater:
        c = shade(c, s.row ? 26 : speck % 5 - 2); // a lighter rim
        break;
    }
    return pack(c);
}

[[nodiscard]] std::uint64_t key_of(int tx, int ty) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(tx)) << 32u) |
           static_cast<std::uint32_t>(ty);
}

} // namespace

GroundView::~GroundView() {
    for (auto& entry : tiles_) {
        SDL_DestroyTexture(entry.second.texture);
    }
}

void GroundView::reset(const GroundParams& params, std::uint64_t seed) {
    for (auto& entry : tiles_) {
        SDL_DestroyTexture(entry.second.texture);
    }
    tiles_.clear();
    params_ = params;
    seed_ = seed;
}

void GroundView::bake(SDL_Texture* texture, int tx, int ty) {
    pixels_.resize(static_cast<std::size_t>(kTile) * kTile);
    const int sx0 = tx * kTile;
    const int sy0 = ty * kTile;
    std::size_t i = 0;
    for (int py = 0; py < kTile; ++py) {
        GroundCell last{std::numeric_limits<int>::min(), 0};
        std::uint32_t last_colour = 0;
        for (int px = 0; px < kTile; ++px) {
            // Neighbouring pixels are often over the same ground unit: reuse its colour.
            const GroundCell c = ground_of_pixel(sx0 + px, sy0 + py);
            if (!(c == last)) {
                last = c;
                last_colour = colour(sample_ground(params_, seed_, c.x, c.y), c.x, c.y);
            }
            pixels_[i++] = last_colour;
        }
    }
    SDL_UpdateTexture(texture, nullptr, pixels_.data(), kTile * 4);
}

SDL_Texture* GroundView::tile(int tx, int ty) {
    const std::uint64_t key = key_of(tx, ty);
    if (const auto found = tiles_.find(key); found != tiles_.end()) {
        found->second.used = frame_;
        return found->second.texture;
    }
    SDL_Texture* texture = nullptr;
    if (tiles_.size() >= capacity_) {
        auto oldest = tiles_.begin();
        for (auto it = tiles_.begin(); it != tiles_.end(); ++it) {
            if (it->second.used < oldest->second.used) {
                oldest = it;
            }
        }
        texture = oldest->second.texture;
        tiles_.erase(oldest);
    } else {
        texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
                                    kTile, kTile);
        if (texture == nullptr) {
            SDL_Log("ground tile texture failed: %s", SDL_GetError());
            return nullptr;
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    }
    bake(texture, tx, ty);
    tiles_.emplace(key, Tile{texture, frame_});
    return texture;
}

void GroundView::draw(PixelPt origin, int view_w, int view_h) {
    ++frame_;
    const int tx0 = floor_div(origin.x, kTile);
    const int ty0 = floor_div(origin.y, kTile);
    const int tx1 = floor_div(origin.x + view_w - 1, kTile);
    const int ty1 = floor_div(origin.y + view_h - 1, kTile);
    // Room for every visible tile and the ring around them twice over, so nothing on screen is
    // ever the one thrown out.
    const auto ring = static_cast<std::size_t>((tx1 - tx0 + 3) * (ty1 - ty0 + 3));
    capacity_ = std::max<std::size_t>(96, ring * 2);
    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            SDL_Texture* t = tile(tx, ty);
            if (t == nullptr) {
                continue;
            }
            const SDL_FRect dst{static_cast<float>(tx * kTile - origin.x),
                                static_cast<float>(ty * kTile - origin.y), kTile, kTile};
            SDL_RenderTexture(renderer_, t, nullptr, &dst);
        }
    }
    // Make up to two of the tiles just outside the view, so scrolling never waits on a whole row.
    int budget = 2;
    for (int ty = ty0 - 1; ty <= ty1 + 1 && budget > 0; ++ty) {
        for (int tx = tx0 - 1; tx <= tx1 + 1 && budget > 0; ++tx) {
            const bool inside = tx >= tx0 && tx <= tx1 && ty >= ty0 && ty <= ty1;
            if (!inside && !tiles_.contains(key_of(tx, ty))) {
                (void)tile(tx, ty);
                --budget;
            }
        }
    }
}

} // namespace wof::game
