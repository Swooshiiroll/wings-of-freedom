#pragma once

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "core/camera.hpp"
#include "core/ground.hpp"

namespace wof::game {

// Draws the farmland (core/ground.hpp) in the isometric view. The ground is cut into 128x128
// pixel tiles in iso pixel space; each tile's pixels are worked out once (the inverse projection,
// the ground under each pixel, and the palette) and kept as a texture while it's in use, so a
// frame is a few dozen textured quads. That runs on any renderer, software ones included.
class GroundView {
  public:
    static constexpr int kTile = 128;

    explicit GroundView(SDL_Renderer* renderer) noexcept : renderer_(renderer) {}
    ~GroundView();
    GroundView(const GroundView&) = delete;
    GroundView& operator=(const GroundView&) = delete;

    // Sets the ground to draw, forgetting every tile made for the old one.
    void reset(const GroundParams& params, std::uint64_t seed);

    // Draws the ground into the current render target, for a view whose top-left is the iso pixel
    // `origin`, and makes a couple of the tiles just outside it ready for when they scroll in.
    void draw(PixelPt origin, int view_w, int view_h);

    [[nodiscard]] int cached() const noexcept { return static_cast<int>(tiles_.size()); }

  private:
    struct Tile {
        SDL_Texture* texture = nullptr;
        std::uint64_t used = 0; // the frame it was last drawn or made in
    };

    // The tile, made now if it isn't cached (reusing the least recently used one's texture once
    // the cache is full). nullptr if a texture couldn't be made.
    SDL_Texture* tile(int tx, int ty);
    void bake(SDL_Texture* texture, int tx, int ty);

    SDL_Renderer* renderer_;
    GroundParams params_;
    std::uint64_t seed_ = 0;
    std::unordered_map<std::uint64_t, Tile> tiles_;
    std::vector<std::uint32_t> pixels_;
    std::uint64_t frame_ = 0;
    std::size_t capacity_ = 96;
};

} // namespace wof::game
