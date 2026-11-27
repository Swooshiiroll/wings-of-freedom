#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

#include "core/body.hpp"
#include "core/camera.hpp"

namespace wof::game {

// The placeholder fighter's sprites (design/flight.md, Look): core/fighter.hpp's model baked at
// start by core/raster.hpp into two atlases, the plane and its shadow, at 32 headings x 9 pitches
// (every 10° from -40 to 40) x 3 banks (left, level, right). 864 poses in 48x48 cells: each atlas
// is 32 cells across (one per heading) and 27 down (pitch, then bank).
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

    // Bakes both atlases and uploads them. False if a texture couldn't be made.
    bool bake();

    // Draws a body's plane at its height, or its shadow on the ground below it, for a view whose
    // top-left is the iso pixel `origin`. Anchored to whole art pixels.
    void draw(const Body& b, PixelPt origin, bool shadow) const;

    // The atlas cell for a body: heading, pitch to the nearest 10°, and bank past ±15°.
    struct Cell {
        int column = 0;
        int row = 0;
    };
    [[nodiscard]] static Cell cell_of(const Body& b) noexcept;

    // Developer tool: saves the plane atlas over a grass-green background as a PNG.
    bool save_sheet(const char* path) const;

    [[nodiscard]] double bake_ms() const noexcept { return bake_ms_; }

  private:
    SDL_Renderer* renderer_;
    SDL_Texture* plane_ = nullptr;
    SDL_Texture* shadow_ = nullptr;
    std::vector<std::uint32_t> plane_pixels_; // kept for save_sheet
    double bake_ms_ = 0.0;
};

} // namespace wof::game
