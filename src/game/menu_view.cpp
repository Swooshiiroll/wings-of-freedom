#include "game/menu_view.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>

#include "core/units.hpp"

namespace wof::game {
namespace {

constexpr float kGlyph = 8.0f;        // the debug font's character size
constexpr std::size_t kKeyChars = 10; // a key name's room in its column

void colour(SDL_Renderer* r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

void text(SDL_Renderer* r, float x, float y, const std::string& s, SDL_Color c) {
    colour(r, c);
    SDL_RenderDebugText(r, std::floor(x), std::floor(y), s.c_str());
}

void centred(SDL_Renderer* r, int view_w, float y, const std::string& s, SDL_Color c) {
    text(r, (static_cast<float>(view_w) - static_cast<float>(s.size()) * kGlyph) * 0.5f, y, s, c);
}

[[nodiscard]] const char* aim_label(HeightRule h) noexcept {
    switch (h) {
    case HeightRule::Forgiving:
        return "Forgiving";
    case HeightRule::Strict:
        return "Strict";
    case HeightRule::AutoAim:
        return "Auto-aim";
    }
    return "?";
}

[[nodiscard]] const char* level_label(AutoLevel level) noexcept {
    switch (level) {
    case AutoLevel::On:
        return "On";
    case AutoLevel::Assisted:
        return "Assisted";
    case AutoLevel::Off:
        return "Off";
    }
    return "?";
}

[[nodiscard]] std::string aim_hint(HeightRule h, const WeaponParams& w) {
    const float band = h == HeightRule::Strict ? w.band_strict_u : w.band_forgiving_u;
    if (h == HeightRule::AutoAim) {
        return "Rounds angle toward targets near the nose";
    }
    return "Hits within " + std::to_string(static_cast<int>(std::lround(band * kFeetPerU))) +
           " ft of the height";
}

// The line under the menu: what the highlighted row means, or how to use the page.
[[nodiscard]] std::string hint(const Menu& m, const Settings& s, const WeaponParams& w) {
    if (m.capturing) {
        return std::string("Press a key for ") + action_name(static_cast<Action>(m.row)) +
               " (Esc: cancel)";
    }
    if (m.page == MenuPage::Options) {
        switch (m.row) {
        case kOptionsAutoLevel:
            return s.auto_level == AutoLevel::On         ? "The nose levels when you let go"
                   : s.auto_level == AutoLevel::Assisted ? "Levels when let go near level"
                                                         : "The nose stays where you leave it";
        case kOptionsYourAim:
            return aim_hint(s.your_aim, w);
        case kOptionsEnemyAim:
            return aim_hint(s.enemy_aim, w);
        default:
            break;
        }
        return "Left/Right: change   Esc: back";
    }
    if (m.page == MenuPage::Controls) {
        return "Enter: rebind   Delete: clear   Esc: back";
    }
    return "Esc: resume";
}

} // namespace

std::string key_label(int scancode) {
    if (scancode == kNoKey) {
        return "-";
    }
    const auto code = static_cast<SDL_Scancode>(scancode);
    std::string name = SDL_GetKeyName(SDL_GetKeyFromScancode(code, SDL_KMOD_NONE, false));
    if (name.empty()) {
        name = SDL_GetScancodeName(code);
    }
    if (name.empty()) {
        name = "key " + std::to_string(scancode);
    }
    // "Left Shift" and "Right Ctrl" as "L Shift" and "R Ctrl", so they fit their column.
    for (const char* side : {"Left ", "Right "}) {
        if (name.rfind(side, 0) == 0) {
            name = std::string(1, side[0]) + " " + name.substr(std::string(side).size());
        }
    }
    return name.substr(0, kKeyChars);
}

void draw_menu(SDL_Renderer* r, const Menu& m, const Settings& s, const WeaponParams& weapons,
               int view_w, int view_h) {
    if (m.page == MenuPage::Closed) {
        return;
    }
    const SDL_Color title{255, 236, 170, 255};
    const SDL_Color plain{236, 232, 200, 255};
    const SDL_Color dim{150, 150, 135, 255};
    const MenuLayout l = menu_layout(m.page, view_w, view_h);
    const int rows = menu_rows(m.page);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    const SDL_FRect shade{0.0f, 0.0f, static_cast<float>(view_w), static_cast<float>(view_h)};
    colour(r, SDL_Color{0, 0, 0, 130});
    SDL_RenderFillRect(r, &shade);
    const SDL_FRect panel{l.x - 8.0f, l.y - 28.0f, l.w + 16.0f,
                          static_cast<float>(rows) * l.row_h + 52.0f};
    colour(r, SDL_Color{18, 24, 32, 232});
    SDL_RenderFillRect(r, &panel);

    centred(r, view_w, l.y - 20.0f,
            m.page == MenuPage::Pause     ? "PAUSED"
            : m.page == MenuPage::Options ? "OPTIONS"
                                          : "CONTROLS",
            title);

    for (int i = 0; i < rows; ++i) {
        const float y = l.y + static_cast<float>(i) * l.row_h;
        const bool lit = i == m.row;
        const bool action = m.page == MenuPage::Controls && i < static_cast<int>(kActions);
        if (lit) {
            const SDL_FRect bar{l.x, y, l.w, l.row_h - 1.0f};
            colour(r, SDL_Color{70, 82, 104, 255});
            SDL_RenderFillRect(r, &bar);
        }
        const SDL_Color c = lit ? title : plain;
        const float ty = y + 3.0f;
        std::string label;
        std::string value;
        if (m.page == MenuPage::Pause) {
            label = i == kPauseResume    ? "Resume"
                    : i == kPauseRestart ? "Restart run"
                    : i == kPauseOptions ? "Options"
                                         : "Quit game";
        } else if (m.page == MenuPage::Options) {
            label = i == kOptionsControls    ? "Controls"
                    : i == kOptionsAutoLevel ? "Auto-leveling"
                    : i == kOptionsYourAim   ? "Your gun aim"
                    : i == kOptionsEnemyAim  ? "Enemy gun aim"
                                             : "Back";
            value = i == kOptionsAutoLevel  ? level_label(s.auto_level)
                    : i == kOptionsYourAim  ? aim_label(s.your_aim)
                    : i == kOptionsEnemyAim ? aim_label(s.enemy_aim)
                                            : "";
        } else {
            label = action                ? action_name(static_cast<Action>(i))
                    : i == kControlsReset ? "Reset to defaults"
                                          : "Back";
        }
        text(r, l.x + 6.0f, ty, label, c);
        if (!value.empty()) {
            const std::string shown = "< " + value + " >";
            text(r, l.x + l.w - static_cast<float>(shown.size()) * kGlyph - 6.0f, ty, shown, c);
        }
        if (action) {
            const KeyPair& keys = s.keys[static_cast<std::size_t>(i)];
            for (int slot = 0; slot < kKeySlots; ++slot) {
                const float x = l.slot_x[static_cast<std::size_t>(slot)];
                const int code = slot == 0 ? keys.main : keys.alt;
                const bool here = lit && slot == m.slot;
                if (here) {
                    const SDL_FRect box{x, y + 1.0f, l.slot_w - 4.0f, l.row_h - 3.0f};
                    colour(r, m.capturing ? SDL_Color{150, 90, 40, 255}
                                          : SDL_Color{104, 118, 146, 255});
                    SDL_RenderFillRect(r, &box);
                }
                const std::string name = here && m.capturing ? "..." : key_label(code);
                text(r, x + 3.0f, ty, name, code == kNoKey && !here ? dim : c);
            }
        }
    }
    centred(r, view_w, l.y + static_cast<float>(rows) * l.row_h + 10.0f, hint(m, s, weapons), dim);
}

} // namespace wof::game
