#include "core/menu.hpp"

#include <cstddef>

#include <doctest/doctest.h>

using namespace wof;

namespace {
void press(Menu& m, Settings& s, MenuInput in, int times = 1) {
    for (int i = 0; i < times; ++i) {
        (void)menu_input(m, in, s);
    }
}
} // namespace

TEST_CASE("The pause page: resume, restart, options and quit, wrapping round") {
    Menu m;
    Settings s;
    open_menu(m);
    CHECK(m.page == MenuPage::Pause);
    CHECK(menu_input(m, MenuInput::Select, s) == MenuAction::Resume);
    CHECK(m.page == MenuPage::Closed);
    open_menu(m);
    press(m, s, MenuInput::Down);
    CHECK(menu_input(m, MenuInput::Select, s) == MenuAction::Restart);
    open_menu(m);
    press(m, s, MenuInput::Up); // wraps to Quit
    CHECK(m.row == kPauseQuit);
    CHECK(menu_input(m, MenuInput::Select, s) == MenuAction::Quit);
    open_menu(m);
    CHECK(menu_input(m, MenuInput::Back, s) == MenuAction::Resume); // Esc resumes
}

TEST_CASE("Options: values change with left, right and select, and Back returns") {
    Menu m;
    Settings s;
    open_menu(m);
    press(m, s, MenuInput::Down, 2);
    press(m, s, MenuInput::Select);
    REQUIRE(m.page == MenuPage::Options);
    press(m, s, MenuInput::Down); // auto-leveling
    CHECK(menu_input(m, MenuInput::Select, s) == MenuAction::SettingsChanged);
    CHECK(s.auto_level == AutoLevel::Assisted);
    press(m, s, MenuInput::Right);
    CHECK(s.auto_level == AutoLevel::Off);
    press(m, s, MenuInput::Right); // round again
    CHECK(s.auto_level == AutoLevel::On);
    press(m, s, MenuInput::Down);
    press(m, s, MenuInput::Left);
    CHECK(s.your_aim == HeightRule::AutoAim);
    press(m, s, MenuInput::Down);
    press(m, s, MenuInput::Right);
    CHECK(s.enemy_aim == HeightRule::Strict);
    CHECK(s.your_aim == HeightRule::AutoAim); // separate
    press(m, s, MenuInput::Down);             // mid-air collisions
    press(m, s, MenuInput::Right);
    CHECK_FALSE(s.collisions);
    CHECK(menu_input(m, MenuInput::Select, s) == MenuAction::SettingsChanged);
    CHECK(s.collisions);
    press(m, s, MenuInput::Down); // the dashboard's size
    press(m, s, MenuInput::Right);
    CHECK(s.panel == PanelSize::Medium);
    press(m, s, MenuInput::Down); // units
    press(m, s, MenuInput::Left);
    CHECK(s.units == Units::Metric);
    press(m, s, MenuInput::Back);
    CHECK(m.page == MenuPage::Pause);
    CHECK(m.row == kPauseOptions);
}

TEST_CASE("Controls: select captures a key; Esc cancels, Delete clears, reserved are ignored") {
    Menu m;
    Settings s;
    open_menu(m);
    m.page = MenuPage::Controls;
    m.row = static_cast<int>(Action::Guns);
    press(m, s, MenuInput::Right); // the second slot
    CHECK(menu_input(m, MenuInput::Select, s) == MenuAction::None);
    REQUIRE(m.capturing);
    CHECK(menu_key(m, kScanF1, s) == MenuAction::None); // reserved: still waiting
    CHECK(m.capturing);
    CHECK(menu_key(m, 13, s) == MenuAction::SettingsChanged);
    CHECK_FALSE(m.capturing);
    CHECK(s.keys[static_cast<std::size_t>(Action::Guns)] == KeyPair{9, 13});
    press(m, s, MenuInput::Select);
    CHECK(menu_key(m, kScanEscape, s) == MenuAction::None);
    CHECK_FALSE(m.capturing);
    CHECK(s.keys[static_cast<std::size_t>(Action::Guns)].alt == 13);
    press(m, s, MenuInput::Select);
    CHECK(menu_key(m, kScanDelete, s) == MenuAction::SettingsChanged);
    CHECK(s.keys[static_cast<std::size_t>(Action::Guns)].alt == kNoKey);
    m.row = kControlsReset;
    press(m, s, MenuInput::Select);
    CHECK(s.keys == default_keys());
    press(m, s, MenuInput::Back);
    CHECK(m.page == MenuPage::Options);
}

TEST_CASE("The mouse: pointing highlights a row and key column, clicking selects it") {
    Menu m;
    Settings s;
    open_menu(m);
    const MenuLayout l = menu_layout(MenuPage::Pause, 480, 270);
    const MenuHit quit = menu_hit(MenuPage::Pause, 480, 270, l.x + 10.0f, l.y + l.row_h * 3.5f);
    CHECK(quit.row == kPauseQuit);
    menu_hover(m, quit.row, quit.slot);
    CHECK(m.row == kPauseQuit);
    CHECK(menu_click(m, quit.row, quit.slot, s) == MenuAction::Quit);
    CHECK(menu_hit(MenuPage::Pause, 480, 270, 2.0f, l.y + 1.0f).row == -1);
    CHECK(menu_hit(MenuPage::Pause, 480, 270, l.x + 10.0f, l.y - 1.0f).row == -1);

    const MenuLayout c = menu_layout(MenuPage::Controls, 480, 270);
    CHECK(c.y >= 28.0f);                                                // room for the title
    CHECK(c.y + static_cast<float>(kControlsRows) * c.row_h <= 250.0f); // and the hint
    const MenuHit second =
        menu_hit(MenuPage::Controls, 480, 270, c.slot_x[1] + 5.0f, c.y + c.row_h * 6.5f);
    CHECK(second.row == static_cast<int>(Action::Guns));
    CHECK(second.slot == 1);
    open_menu(m);
    m.page = MenuPage::Controls;
    CHECK(menu_click(m, second.row, second.slot, s) == MenuAction::None);
    CHECK(m.capturing);
    CHECK(m.slot == 1);
}
