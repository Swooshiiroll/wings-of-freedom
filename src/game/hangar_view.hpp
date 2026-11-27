#pragma once

#include <SDL3/SDL.h>

#include "core/body.hpp"
#include "core/hangar.hpp"
#include "core/progress.hpp"
#include "core/score.hpp"
#include "core/units.hpp"
#include "core/world.hpp"

namespace wof::game {

// Draws the hangar (core/hangar.hpp) and the run summary (design/aircraft_tree.md, Screens), in art
// pixels, with the debug font.

// The hangar's row under a view position, or -1.
[[nodiscard]] int hangar_row_at(int view_w, int view_h, float x, float y) noexcept;

void draw_hangar(SDL_Renderer* r, const Hangar& h, const NationProgress& n, Nation nation,
                 const Rules& rules, UnitSystem units, int view_w, int view_h);

struct Summary {
    Outcome outcome = Outcome::Flying;
    long seconds = 0;
    int wave = 0;
    RunScore score;
    Payout payout;
    Nation nation = Nation::US;
    bool kept = false; // progress is saved: the banked points are the nation's now
    int points_now = 0;
};

// `view_h` is the world's rows, above the dashboard.
void draw_summary(SDL_Renderer* r, const Summary& s, int view_w, int view_h);

} // namespace wof::game
