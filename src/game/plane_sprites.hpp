#pragma once

#include <SDL3/SDL.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "core/body.hpp"
#include "core/planes.hpp"
#include "core/view_xf.hpp"

namespace wof::game {

// The planes' sprites (design/flight.md, Look): each plane's model (core/fighter.hpp) baked by
// core/raster.hpp into two atlases, the plane and its shadow, at 32 headings x 9 pitches (every
// 10° from -40 to 40) x 3 banks (left, level, right): 864 poses, 32 cells across (one per heading)
// and 27 down (pitch, then bank).
//
// Planes are drawn in steps of 1/8 of full size, from a quarter to 1.5x, by how far below or above
// you they are (design/planes.md, Height), each step baked as crisp pixel art in cells of
// 6 px x its eighths (12 to 72 px; the full size is 48). Atlases are baked by model, parts drawn
// and step. A model's full size bakes when asked for; every other one bakes on a background thread,
// and until it's ready the nearest ready step is stretched to the size.
class PlaneSprites {
  public:
    static constexpr int kCellPerStep = 6;                     // a step's cell: 6 px x its eighths
    static constexpr int kCell = kCellPerStep * kFullSizeStep; // the full size's, 48 px
    static constexpr int kPitches = 9;
    static constexpr int kBanks = 3;
    static constexpr int kColumns = 32;
    static constexpr int kRows = kPitches * kBanks;

    // `in_line`: bake everything when it's asked for, on this thread, so screenshots are exact.
    PlaneSprites(SDL_Renderer* renderer, bool in_line) noexcept
        : renderer_(renderer), in_line_(in_line) {}
    ~PlaneSprites();
    PlaneSprites(const PlaneSprites&) = delete;
    PlaneSprites& operator=(const PlaneSprites&) = delete;

    // Bakes a model's full size now, and queues its other sizes for the background. False if a
    // texture couldn't be made.
    bool bake(ModelId id);

    // Uploads one finished background bake, if there is one. Once a frame.
    void pump();

    // Draws a body's plane (its type's model) at its height, sized by its depth, or its shadow on
    // the ground below it, sized with the ground. Anchored to whole art pixels. `tint` multiplies
    // the colours (the practice drones are orange-red); `flash` makes it glow (a hit).
    void draw(const Body& b, const ViewXf& xf, bool shadow, SDL_Color tint = {255, 255, 255, 255},
              bool flash = false);

    // The atlas cell for a body: heading, pitch to the nearest 10°, and bank past ±15°.
    struct Cell {
        int column = 0;
        int row = 0;
    };
    [[nodiscard]] static Cell cell_of(const Body& b) noexcept;

    // Developer tool: saves a model's atlas at a size step over a grass-green background as a PNG.
    static bool save_sheet(const char* path, ModelId id, int step);

    [[nodiscard]] double bake_ms() const noexcept { return bake_ms_; } // the last full-size bake
    [[nodiscard]] int ready() const noexcept { return static_cast<int>(atlases_.size()); }
    [[nodiscard]] int queued() const noexcept { return static_cast<int>(queued_.size()); }

    // Stops the baking thread and frees the textures. Before the renderer goes away.
    void shutdown();

  private:
    // The pixels of one bake: the plane's atlas, and its shadow's (empty above full size, where
    // no shadow is ever drawn).
    struct Pixels {
        std::vector<std::uint32_t> plane;
        std::vector<std::uint32_t> shadow;
        double ms = 0.0;
    };
    struct Job {
        std::uint32_t key = 0;
        ModelId id{};
        std::uint32_t parts = 0;
        int step = kFullSizeStep;
    };
    struct Done {
        Job job;
        Pixels pixels;
    };
    struct Atlas {
        SDL_Texture* plane = nullptr;
        SDL_Texture* shadow = nullptr;
    };
    // The texture to draw from, and the step it was baked at.
    struct Found {
        SDL_Texture* texture = nullptr;
        int step = kFullSizeStep;
    };

    [[nodiscard]] static std::uint32_t key_of(ModelId id, std::uint32_t parts, int step) noexcept {
        return (static_cast<std::uint32_t>(id) << 16u) | (parts << 8u) |
               static_cast<std::uint32_t>(step);
    }
    static Pixels bake_pixels(ModelId id, std::uint32_t parts, int step);

    Atlas upload_atlas(const Pixels& pixels, int step) const;
    // Bakes and uploads on this thread, now.
    Atlas bake_now(const Job& job);
    // Queues a bake for the background, unless it's ready or already queued. `urgent` puts it at
    // the front: it's needed on screen now.
    void request(const Job& job, bool urgent);
    // The background thread: bakes the queued jobs in order.
    void work();
    // The wanted step if ready, else the nearest ready one, of these parts or else of the whole
    // plane. Empty if the model has nothing ready.
    [[nodiscard]] Found nearest(ModelId id, std::uint32_t parts, int step, bool shadow) const;
    Found find(ModelId id, std::uint32_t parts, int step, bool shadow);

    SDL_Renderer* renderer_;
    bool in_line_ = false;
    double bake_ms_ = 0.0;
    std::map<std::uint32_t, Atlas> atlases_; // ready, by model, parts and step
    std::set<std::uint32_t> queued_;         // asked of the background thread, not yet uploaded

    // Shared with the background thread.
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Job> jobs_;
    std::deque<Done> done_;
    bool quit_ = false;
    std::thread worker_;
};

} // namespace wof::game
