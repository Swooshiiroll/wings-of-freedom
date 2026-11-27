#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <map>
#include <vector>

#include "core/body.hpp"
#include "core/camera.hpp"
#include "core/planes.hpp"
#include "game/view_xf.hpp"

namespace wof::game {

// The planes' sprites (design/flight.md, Look): each plane's model (core/fighter.hpp) baked by
// core/raster.hpp, the first time it's needed, into two atlases, the plane and its shadow, at 32
// headings x 9 pitches (every 10° from -40 to 40) x 3 banks (left, level, right). 864 poses in
// 48x48 cells: each atlas is 32 cells across (one per heading) and 27 down (pitch, then bank).
class PlaneSprites {
  public:
    static constexpr int kCell = 48;
    static constexpr int kPitches = 9;
    static constexpr int kBanks = 3;
    static constexpr int kColumns = 32;
    static constexpr int kRows = kPitches * kBanks;

    explicit PlaneSprites(SDL_Renderer* renderer) noexcept : renderer_(renderer) {}
    ~PlaneSprites();
    PlaneSprites(const PlaneSprites&) = delete;
    PlaneSprites& operator=(const PlaneSprites&) = delete;

    // Bakes a model's atlases now (the player's plane, at start) and keeps its pixels for
    // save_sheet. False if a texture couldn't be made.
    bool bake(ModelId id);

    // Draws a body's plane (its type's model) at its height, full size, or its shadow on the
    // ground below it, shrunk with the ground zoom. Anchored to whole art pixels. `tint` multiplies
    // the colours (the practice drones are orange-red); `flash` makes it glow (a hit).
    void draw(const Body& b, const ViewXf& xf, bool shadow, SDL_Color tint = {255, 255, 255, 255},
              bool flash = false) const;

    // The atlas cell for a body: heading, pitch to the nearest 10°, and bank past ±15°.
    struct Cell {
        int column = 0;
        int row = 0;
    };
    [[nodiscard]] static Cell cell_of(const Body& b) noexcept;

    // Developer tool: saves the atlas last baked by bake() over a grass-green background as a
    // PNG.
    bool save_sheet(const char* path) const;

    [[nodiscard]] double bake_ms() const noexcept { return bake_ms_; }

  private:
    SDL_Renderer* renderer_;
    // A plane atlas and its shadow atlas, for one set of parts.
    struct Atlas {
        SDL_Texture* plane = nullptr;
        SDL_Texture* shadow = nullptr;
    };

    // Bakes the atlases for the model's `parts` (keeping the plane's pixels in `keep` if given).
    Atlas bake_atlas(ModelId id, std::uint32_t parts, std::vector<std::uint32_t>* keep) const;

    // The atlases for a set of parts, baked the first time they're needed (a plane that's lost a
    // wing or its tail: about 0.1 s, once).
    const Atlas& atlas_for(ModelId id, std::uint32_t parts) const;

    [[nodiscard]] static std::uint32_t key_of(ModelId id, std::uint32_t parts) noexcept {
        return (static_cast<std::uint32_t>(id) << 8u) | parts;
    }

    mutable std::map<std::uint32_t, Atlas> atlases_; // by model and the parts drawn
    std::vector<std::uint32_t> plane_pixels_;        // the whole plane's, kept for save_sheet
    mutable double bake_ms_ = 0.0;
};

} // namespace wof::game
