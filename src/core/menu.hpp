#pragma once

#include <array>
#include <cstdint>

#include "core/settings.hpp"

namespace wof {

// The pause and options menus (design/flight.md, Menus): which page, which row is highlighted,
// and what keys and the mouse do. No SDL; the front end draws it (game/menu_view.hpp).

enum class MenuPage : std::uint8_t { Closed, Pause, Options, Controls };

enum class MenuInput : std::uint8_t {
    Up,
    Down,
    Left,
    Right,
    Select,
    Back,
    Clear, // Delete: take a key off an action
};

// What the front end has to do after an input.
enum class MenuAction : std::uint8_t { None, Resume, Restart, Quit, SettingsChanged };

// The rows of each page.
enum PauseRow : int { kPauseResume, kPauseRestart, kPauseOptions, kPauseQuit, kPauseRows };
enum OptionsRow : int {
    kOptionsControls,
    kOptionsAutoLevel,
    kOptionsYourAim,
    kOptionsEnemyAim,
    kOptionsPanel,
    kOptionsUnits,
    kOptionsBack,
    kOptionsRows
};
// Controls: one row per action, then reset and back.
inline constexpr int kControlsReset = static_cast<int>(kActions);
inline constexpr int kControlsBack = static_cast<int>(kActions) + 1;
inline constexpr int kControlsRows = static_cast<int>(kActions) + 2;

[[nodiscard]] int menu_rows(MenuPage page) noexcept;

struct Menu {
    MenuPage page = MenuPage::Closed;
    int row = 0;
    int slot = 0;           // Controls: the main key (0) or the second (1)
    bool capturing = false; // Controls: waiting for the key to bind
};

// The pause page.
void open_menu(Menu& m) noexcept;

MenuAction menu_input(Menu& m, MenuInput in, Settings& s) noexcept;
// A key pressed while capturing: it's bound (Esc cancels, Delete clears, reserved keys are
// ignored).
MenuAction menu_key(Menu& m, int scancode, Settings& s) noexcept;
// The mouse: pointing highlights, clicking selects.
void menu_hover(Menu& m, int row, int slot) noexcept;
MenuAction menu_click(Menu& m, int row, int slot, Settings& s) noexcept;

// Where a page's rows are in the view, in art pixels.
struct MenuLayout {
    float x = 0.0f; // the panel's left
    float y = 0.0f; // the first row's top
    float w = 0.0f;
    float row_h = 14.0f;
    std::array<float, kKeySlots> slot_x{}; // Controls: the key columns
    float slot_w = 0.0f;
};
[[nodiscard]] MenuLayout menu_layout(MenuPage page, int view_w, int view_h) noexcept;

// The row (and key column) under a view position; row -1 if none.
struct MenuHit {
    int row = -1;
    int slot = 0;
};
[[nodiscard]] MenuHit menu_hit(MenuPage page, int view_w, int view_h, float px, float py) noexcept;

} // namespace wof
