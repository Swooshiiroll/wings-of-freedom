#pragma once

#include <SDL3/SDL_scancode.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/input.hpp"

namespace wof::game {

// What each key does (design/flight.md, Controls). Keys are scancodes, so they're physical key
// positions whatever the keyboard layout. PR 6 adds the controls menu that rebinds them; until
// then the defaults are fixed.
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
};
inline constexpr std::size_t kActions = 9;

struct Binding {
    SDL_Scancode main = SDL_SCANCODE_UNKNOWN;
    SDL_Scancode alt = SDL_SCANCODE_UNKNOWN;
};
using Bindings = std::array<Binding, kActions>;

// flight.md's default keys: A/D turn, W dive, S climb, Shift/Ctrl throttle, F guns, Space heavy
// weapon, Tab switch heavy weapon.
[[nodiscard]] Bindings default_bindings() noexcept;

// True if `key` is bound to `action`.
[[nodiscard]] bool is_key(const Bindings& b, Action action, SDL_Scancode key) noexcept;

// One tick's command from the keys held now (SDL_GetKeyboardState). `switch_pressed`: the switch
// key went down since the last tick (it's a press, not a hold).
[[nodiscard]] InputCommand read_command(const Bindings& b, const bool* keys,
                                        bool switch_pressed) noexcept;

} // namespace wof::game
