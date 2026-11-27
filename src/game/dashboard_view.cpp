#include "game/dashboard_view.hpp"

namespace wof::game {

DashboardView::~DashboardView() {
    shutdown();
}

void DashboardView::shutdown() noexcept {
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
}

void DashboardView::draw(const PanelState& s, PanelSize size, int view_w, int view_h) {
    const DashLayout layout = dash_layout(size);
    if (canvas_.height() != layout.height || texture_ == nullptr) {
        canvas_ = Canvas(kPanelW, layout.height);
        shutdown();
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, kPanelW, layout.height);
        if (texture_ == nullptr) {
            SDL_Log("dashboard texture failed: %s", SDL_GetError());
            return;
        }
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE);
    }
    draw_panel(canvas_, s, layout);
    SDL_UpdateTexture(texture_, nullptr, canvas_.pixels().data(), kPanelW * 4);

    const auto top = static_cast<float>(view_h - layout.height);
    // A wider screen: the panel's colour (and its top edge) out to both sides.
    const Colour face = canvas_.at(0, 4);
    const Colour edge = canvas_.at(0, 0);
    const auto fill = [this](Colour c, const SDL_FRect& r) {
        SDL_SetRenderDrawColor(renderer_, static_cast<Uint8>((c >> 16u) & 0xffu),
                               static_cast<Uint8>((c >> 8u) & 0xffu), static_cast<Uint8>(c & 0xffu),
                               255);
        SDL_RenderFillRect(renderer_, &r);
    };
    fill(face, {0.0f, top, static_cast<float>(view_w), static_cast<float>(layout.height)});
    fill(edge, {0.0f, top, static_cast<float>(view_w), 1.0f});
    const SDL_FRect dst{static_cast<float>((view_w - kPanelW) / 2), top,
                        static_cast<float>(kPanelW), static_cast<float>(layout.height)};
    SDL_RenderTexture(renderer_, texture_, nullptr, &dst);
}

} // namespace wof::game
