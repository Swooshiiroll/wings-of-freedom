#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "core/dashboard.hpp"
#include "core/input.hpp"
#include "core/units.hpp"
#include "core/weapons.hpp"

namespace wof {

// The player's choices, saved in settings.txt next to the game (design/flight.md, Menus). Keys are
// SDL scancode numbers (physical key positions, whatever the layout); the front end names them.

enum class Action : std::uint8_t {
    TurnLeft,
    TurnRight,
    Dive,
    Climb,
    ThrottleUp,
    ThrottleDown,
    Guns,
    Heavy,
    SwitchHeavy,
    NextWave, // call the next wave, once it can be
};
inline constexpr std::size_t kActions = 10;
[[nodiscard]] const char* action_name(Action a) noexcept; // "Turn left"

inline constexpr int kNoKey = 0;
// The scancodes the menus use or keep for themselves (SDL's values).
inline constexpr int kScanEscape = 41;
inline constexpr int kScanDelete = 76;
inline constexpr int kScanF1 = 58;
inline constexpr int kScanF12 = 69;

// An action's keys: a main one and a second one, either may be none.
struct KeyPair {
    int main = kNoKey;
    int alt = kNoKey;

    friend constexpr bool operator==(const KeyPair&, const KeyPair&) = default;
};
using KeyBindings = std::array<KeyPair, kActions>;
inline constexpr int kKeySlots = 2;

// A/D turn, W dive, S climb, Shift/Ctrl throttle (left and right), F guns, Space heavy, Tab switch,
// N next wave.
[[nodiscard]] KeyBindings default_keys() noexcept;

// Esc and F1-F12 belong to the menus and developer tools.
[[nodiscard]] bool reserved_key(int key) noexcept;

// Puts `key` in the action's slot (0 main, 1 second), taking it from wherever else it was. False
// (and nothing changes) for a reserved key or none.
bool bind_key(KeyBindings& keys, Action a, int slot, int key) noexcept;
void clear_key(KeyBindings& keys, Action a, int slot) noexcept;

[[nodiscard]] const char* auto_level_name(AutoLevel level) noexcept; // "on", "assisted", "off"
bool parse_auto_level(std::string_view text, AutoLevel& out) noexcept;
[[nodiscard]] const char* units_name(Units u) noexcept; // "nation", "imperial", "metric"

struct Settings {
    AutoLevel auto_level = AutoLevel::On;
    HeightRule your_aim = HeightRule::Forgiving;
    HeightRule enemy_aim = HeightRule::Forgiving;
    bool collisions = true;             // whether planes can collide with yours in mid-air
    PanelSize panel = PanelSize::Large; // the dashboard's height
    Units units = Units::Nation;        // for arrows and banners
    KeyBindings keys = default_keys();

    friend constexpr bool operator==(const Settings&, const Settings&) = default;
};

// settings.txt: `key = value` lines.
[[nodiscard]] std::string to_settings(const Settings& s);
// Reads a settings text over `s`: what it doesn't mention keeps its value, except a key the text
// gives to another action (a new action whose default key was already in use starts unbound).
// Each unknown line or bad value adds a line to `errors`. True if there were none.
bool from_settings(std::string_view text, Settings& s, std::string& errors);

} // namespace wof
