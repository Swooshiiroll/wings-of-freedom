#pragma once

#include <SDL3/SDL.h>

#include "core/canvas.hpp"
#include "core/dashboard.hpp"

namespace wof::game {

// Draws the cockpit dashboard (core/dashboard.hpp) across the bottom of the view: painted on a
// canvas each frame and uploaded as one texture, centred, with the panel's colour filling any
// wider screen.
class DashboardView {
  public:
    explicit DashboardView(SDL_Renderer* renderer) noexcept : renderer_(renderer) {}
    ~DashboardView();
    DashboardView(const DashboardView&) = delete;
    DashboardView& operator=(const DashboardView&) = delete;

    // At the bottom of the current render target, view_w x view_h art pixels.
    void draw(const PanelState& s, PanelSize size, int view_w, int view_h);

    // Frees the texture. Before the renderer goes away.
    void shutdown() noexcept;

  private:
    SDL_Renderer* renderer_;
    SDL_Texture* texture_ = nullptr;
    Canvas canvas_{0, 0};
};

} // namespace wof::game
