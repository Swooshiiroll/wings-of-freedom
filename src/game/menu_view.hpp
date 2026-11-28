#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "core/menu.hpp"
#include "core/settings.hpp"
#include "core/weapons.hpp"

namespace wof::game {

// Draws the pause and options menus (core/menu.hpp) over the view, in art pixels, with the debug
// font. `weapons` gives the aim bands for their descriptions.
void draw_menu(SDL_Renderer* r, const Menu& m, const Settings& s, const WeaponParams& weapons,
               int view_w, int view_h);

// A boxed line of text across the top of the view: the offer of the next wave.
void draw_prompt(SDL_Renderer* r, const std::string& text, int view_w);

// A scancode's name for the player's keyboard layout, or "-" for none.
[[nodiscard]] std::string key_label(int scancode);

} // namespace wof::game
