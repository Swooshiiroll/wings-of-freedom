#include "game/plane_sprites.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "core/damage.hpp"
#include "core/fighter.hpp"
#include "core/iso.hpp"
#include "core/raster.hpp"

namespace wof::game {
namespace {

constexpr int kAtlasW = PlaneSprites::kCell * PlaneSprites::kColumns;
constexpr int kAtlasH = PlaneSprites::kCell * PlaneSprites::kRows;
constexpr float kBakedBankDeg = 30.0f;

// Copies a cell's pixels into an atlas.
void blit(const Sprite& s, std::vector<std::uint32_t>& atlas, int column, int row) {
    for (int y = 0; y < s.h; ++y) {
        for (int x = 0; x < s.w; ++x) {
            const auto ax = static_cast<std::size_t>(column * PlaneSprites::kCell + x);
            const auto ay = static_cast<std::size_t>(row * PlaneSprites::kCell + y);
            atlas[ay * static_cast<std::size_t>(kAtlasW) + ax] = s.at(x, y);
        }
    }
}

SDL_Texture* upload(SDL_Renderer* r, const std::vector<std::uint32_t>& pixels) {
    SDL_Texture* t =
        SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, kAtlasW, kAtlasH);
    if (t == nullptr) {
        SDL_Log("plane atlas %dx%d failed: %s", kAtlasW, kAtlasH, SDL_GetError());
        return nullptr;
    }
    SDL_UpdateTexture(t, nullptr, pixels.data(), kAtlasW * 4);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

} // namespace

PlaneSprites::~PlaneSprites() {
    for (auto& entry : atlases_) {
        SDL_DestroyTexture(entry.second.plane);
        SDL_DestroyTexture(entry.second.shadow);
    }
}

PlaneSprites::Atlas PlaneSprites::bake_atlas(std::uint32_t parts,
                                             std::vector<std::uint32_t>* keep) const {
    const Uint64 start = SDL_GetTicksNS();
    const std::vector<Block> model = fighter_model();
    const auto n = static_cast<std::size_t>(kAtlasW) * static_cast<std::size_t>(kAtlasH);
    std::vector<std::uint32_t> plane_pixels(n, 0u);
    std::vector<std::uint32_t> shadow_pixels(n, 0u);
    constexpr float kMid = static_cast<float>(kCell) * 0.5f;
    for (int h = 0; h < kColumns; ++h) {
        for (int p = 0; p < kPitches; ++p) {
            for (int b = 0; b < kBanks; ++b) {
                const Pose pose{static_cast<float>(h) * (360.0f / kHeadings),
                                static_cast<float>(p - kPitches / 2) * 10.0f,
                                static_cast<float>(b - 1) * kBakedBankDeg};
                const int row = p * kBanks + b;
                blit(render_model(model, pose, kCell, kCell, kMid, kMid, parts), plane_pixels, h,
                     row);
                blit(render_shadow(model, pose, kCell, kCell, kMid, kMid, parts), shadow_pixels, h,
                     row);
            }
        }
    }
    Atlas a{upload(renderer_, plane_pixels), upload(renderer_, shadow_pixels)};
    const double ms = static_cast<double>(SDL_GetTicksNS() - start) / 1e6;
    SDL_Log("plane sprites (parts 0x%02x): %d poses baked in %.1f ms", parts, kColumns * kRows, ms);
    if (keep != nullptr) {
        *keep = std::move(plane_pixels);
        bake_ms_ = ms;
    }
    return a;
}

bool PlaneSprites::bake() {
    const Atlas a = bake_atlas(kAllParts, &plane_pixels_);
    atlases_[kAllParts] = a;
    return a.plane != nullptr && a.shadow != nullptr;
}

const PlaneSprites::Atlas& PlaneSprites::atlas_for(std::uint32_t parts) const {
    const auto found = atlases_.find(parts);
    if (found != atlases_.end()) {
        return found->second;
    }
    // A plane missing a wing or its tail: baked the first time one is seen.
    return atlases_[parts] = bake_atlas(parts, nullptr);
}

PlaneSprites::Cell PlaneSprites::cell_of(const Body& b) noexcept {
    const auto pitch = static_cast<int>(std::lround(b.pitch_deg / 10.0f)) + kPitches / 2;
    const int bank = b.bank_deg < -15.0f ? 0 : b.bank_deg > 15.0f ? 2 : 1;
    return {heading_index(b.heading_deg), std::clamp(pitch, 0, kPitches - 1) * kBanks + bank};
}

void PlaneSprites::draw(const Body& b, PixelPt origin, bool shadow, SDL_Color tint,
                        bool flash) const {
    const Atlas& set = atlas_for(kAllParts & ~missing_parts(b));
    SDL_Texture* atlas = shadow ? set.shadow : set.plane;
    if (atlas == nullptr) {
        return;
    }
    const Cell c = cell_of(b);
    const ScreenPt at = to_screen(b.x, b.y, shadow ? 0.0f : b.z);
    const float half = static_cast<float>(kCell) * 0.5f;
    const SDL_FRect src{static_cast<float>(c.column * kCell), static_cast<float>(c.row * kCell),
                        static_cast<float>(kCell), static_cast<float>(kCell)};
    const SDL_FRect dst{std::round(at.x) - static_cast<float>(origin.x) - half,
                        std::round(at.y) - static_cast<float>(origin.y) - half,
                        static_cast<float>(kCell), static_cast<float>(kCell)};
    SDL_SetTextureColorMod(atlas, tint.r, tint.g, tint.b);
    SDL_RenderTexture(renderer_, atlas, &src, &dst);
    if (flash && !shadow) {
        // A hit: the same sprite again, added on top, so it glows.
        SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_ADD);
        SDL_SetTextureColorMod(atlas, 255, 255, 255);
        SDL_RenderTexture(renderer_, atlas, &src, &dst);
        SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND);
    }
    SDL_SetTextureColorMod(atlas, 255, 255, 255);
}

bool PlaneSprites::save_sheet(const char* path) const {
    std::vector<std::uint32_t> sheet = plane_pixels_;
    for (std::uint32_t& px : sheet) {
        if ((px >> 24u) == 0u) {
            px = 0xff628c46u; // pasture green, to judge how the plane reads on the ground
        }
    }
    SDL_Surface* s = SDL_CreateSurfaceFrom(kAtlasW, kAtlasH, SDL_PIXELFORMAT_ARGB8888, sheet.data(),
                                           kAtlasW * 4);
    if (s == nullptr) {
        SDL_Log("sprite sheet surface failed: %s", SDL_GetError());
        return false;
    }
    const bool ok = SDL_SavePNG(s, path);
    if (ok) {
        SDL_Log("saved %s (%dx%d)", path, kAtlasW, kAtlasH);
    } else {
        SDL_Log("saving %s failed: %s", path, SDL_GetError());
    }
    SDL_DestroySurface(s);
    return ok;
}

} // namespace wof::game
