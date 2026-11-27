#include "core/menu.hpp"

#include <cstddef>

namespace wof {
namespace {

// The next value of a small enum, going `dir` (+1 or -1) round and round.
template <typename E> [[nodiscard]] E cycled(E value, int count, int dir) noexcept {
    return static_cast<E>((static_cast<int>(value) + dir + count) % count);
}

void show(Menu& m, MenuPage page, int row) noexcept {
    m.page = page;
    m.row = row;
    m.slot = 0;
    m.capturing = false;
}

// Changes the value on an Options row, if it has one.
[[nodiscard]] bool change(int row, int dir, Settings& s) noexcept {
    switch (row) {
    case kOptionsAutoLevel:
        s.auto_level = cycled(s.auto_level, kAutoLevels, dir);
        return true;
    case kOptionsYourAim:
        s.your_aim = cycled(s.your_aim, kHeightRules, dir);
        return true;
    case kOptionsEnemyAim:
        s.enemy_aim = cycled(s.enemy_aim, kHeightRules, dir);
        return true;
    case kOptionsPanel:
        s.panel = cycled(s.panel, kPanelSizes, dir);
        return true;
    case kOptionsUnits:
        s.units = cycled(s.units, kUnits, dir);
        return true;
    default:
        return false;
    }
}

[[nodiscard]] MenuAction close(Menu& m, MenuAction action) noexcept {
    show(m, MenuPage::Closed, 0);
    return action;
}

[[nodiscard]] MenuAction select(Menu& m, Settings& s) noexcept {
    switch (m.page) {
    case MenuPage::Pause:
        if (m.row == kPauseResume) {
            return close(m, MenuAction::Resume);
        }
        if (m.row == kPauseRestart) {
            return close(m, MenuAction::Restart);
        }
        if (m.row == kPauseOptions) {
            show(m, MenuPage::Options, 0);
            return MenuAction::None;
        }
        return MenuAction::Quit;
    case MenuPage::Options:
        if (m.row == kOptionsControls) {
            show(m, MenuPage::Controls, 0);
        } else if (m.row == kOptionsBack) {
            show(m, MenuPage::Pause, kPauseOptions);
        } else if (change(m.row, 1, s)) {
            return MenuAction::SettingsChanged;
        }
        return MenuAction::None;
    case MenuPage::Controls:
        if (m.row == kControlsReset) {
            s.keys = default_keys();
            return MenuAction::SettingsChanged;
        }
        if (m.row == kControlsBack) {
            show(m, MenuPage::Options, kOptionsControls);
        } else {
            m.capturing = true;
        }
        return MenuAction::None;
    case MenuPage::Closed:
        break;
    }
    return MenuAction::None;
}

[[nodiscard]] MenuAction back(Menu& m) noexcept {
    switch (m.page) {
    case MenuPage::Pause:
        return close(m, MenuAction::Resume);
    case MenuPage::Options:
        show(m, MenuPage::Pause, kPauseOptions);
        break;
    case MenuPage::Controls:
        show(m, MenuPage::Options, kOptionsControls);
        break;
    case MenuPage::Closed:
        break;
    }
    return MenuAction::None;
}

} // namespace

int menu_rows(MenuPage page) noexcept {
    switch (page) {
    case MenuPage::Pause:
        return kPauseRows;
    case MenuPage::Options:
        return kOptionsRows;
    case MenuPage::Controls:
        return kControlsRows;
    case MenuPage::Closed:
        break;
    }
    return 0;
}

void open_menu(Menu& m) noexcept {
    show(m, MenuPage::Pause, kPauseResume);
}

MenuAction menu_input(Menu& m, MenuInput in, Settings& s) noexcept {
    const int rows = menu_rows(m.page);
    if (rows == 0) {
        return MenuAction::None;
    }
    const bool on_action = m.page == MenuPage::Controls && m.row < static_cast<int>(kActions);
    const auto action = static_cast<Action>(m.row);
    if (m.capturing) {
        if (in == MenuInput::Back) {
            m.capturing = false;
        } else if (in == MenuInput::Clear) {
            clear_key(s.keys, action, m.slot);
            m.capturing = false;
            return MenuAction::SettingsChanged;
        }
        return MenuAction::None;
    }
    switch (in) {
    case MenuInput::Up:
        m.row = (m.row + rows - 1) % rows;
        break;
    case MenuInput::Down:
        m.row = (m.row + 1) % rows;
        break;
    case MenuInput::Left:
    case MenuInput::Right: {
        if (m.page == MenuPage::Controls) {
            m.slot = in == MenuInput::Left ? 0 : 1;
        } else if (m.page == MenuPage::Options &&
                   change(m.row, in == MenuInput::Left ? -1 : 1, s)) {
            return MenuAction::SettingsChanged;
        }
        break;
    }
    case MenuInput::Select:
        return select(m, s);
    case MenuInput::Back:
        return back(m);
    case MenuInput::Clear:
        if (on_action) {
            clear_key(s.keys, action, m.slot);
            return MenuAction::SettingsChanged;
        }
        break;
    }
    return MenuAction::None;
}

MenuAction menu_key(Menu& m, int scancode, Settings& s) noexcept {
    if (!m.capturing) {
        return MenuAction::None;
    }
    if (scancode == kScanEscape) {
        return menu_input(m, MenuInput::Back, s);
    }
    if (scancode == kScanDelete) {
        return menu_input(m, MenuInput::Clear, s);
    }
    if (!bind_key(s.keys, static_cast<Action>(m.row), m.slot, scancode)) {
        return MenuAction::None; // reserved: keep waiting
    }
    m.capturing = false;
    return MenuAction::SettingsChanged;
}

void menu_hover(Menu& m, int row, int slot) noexcept {
    if (m.capturing || row < 0 || row >= menu_rows(m.page)) {
        return;
    }
    m.row = row;
    m.slot = slot;
}

MenuAction menu_click(Menu& m, int row, int slot, Settings& s) noexcept {
    if (row < 0 || row >= menu_rows(m.page)) {
        return MenuAction::None;
    }
    m.capturing = false;
    menu_hover(m, row, slot);
    return select(m, s);
}

MenuLayout menu_layout(MenuPage page, int view_w, int view_h) noexcept {
    MenuLayout l;
    l.w = page == MenuPage::Controls ? 340.0f : 280.0f;
    l.x = static_cast<float>((view_w - static_cast<int>(l.w)) / 2);
    const float rows_h = static_cast<float>(menu_rows(page)) * l.row_h;
    // Centred, a little low to leave room for the title above.
    l.y = static_cast<float>((view_h - static_cast<int>(rows_h)) / 2 + 8);
    l.slot_w = 90.0f;
    l.slot_x = {l.x + l.w - 2.0f * l.slot_w - 8.0f, l.x + l.w - l.slot_w - 4.0f};
    return l;
}

MenuHit menu_hit(MenuPage page, int view_w, int view_h, float px, float py) noexcept {
    const MenuLayout l = menu_layout(page, view_w, view_h);
    MenuHit hit;
    if (px < l.x || px >= l.x + l.w || py < l.y) {
        return hit;
    }
    const auto row = static_cast<int>((py - l.y) / l.row_h);
    if (row >= menu_rows(page)) {
        return hit;
    }
    hit.row = row;
    hit.slot = page == MenuPage::Controls && px >= l.slot_x[1] ? 1 : 0;
    return hit;
}

} // namespace wof
