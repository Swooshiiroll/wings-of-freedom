#pragma once

#include <SDL3/SDL_scancode.h>

#include "core/input.hpp"
#include "core/settings.hpp"

namespace wof::game {

// Turns the keys held into a player's commands, by the settings' bindings (core/settings.hpp:
// scancodes, physical key positions whatever the keyboard layout).

// True if `key` is bound to `action`.
[[nodiscard]] bool is_key(const KeyBindings& b, Action action, SDL_Scancode key) noexcept;

// One tick's command from the keys held now (SDL_GetKeyboardState), with the player's
// auto-leveling. `switch_pressed`: the switch key went down since the last tick (it's a press, not
// a hold).
[[nodiscard]] InputCommand read_command(const Settings& s, const bool* keys,
                                        bool switch_pressed) noexcept;

} // namespace wof::game
