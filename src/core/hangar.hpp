#pragma once

#include <cstdint>

#include "core/progress.hpp"

namespace wof {

// The hangar's cursor and what pressing does (design/aircraft_tree.md, Screens). No SDL; the front
// end draws it (game/hangar_view.hpp).

enum class HangarKind : std::uint8_t { Plane, Upgrade, Fly, Reset, Back };
struct HangarRow {
    HangarKind kind = HangarKind::Plane;
    int index = 0; // the tree step, or the upgrade
};
// The rows in order: the three planes, the six upgrades, Fly, Reset progress, Back.
inline constexpr int kHangarFly = kTreeSteps + kUpgrades;
inline constexpr int kHangarReset = kHangarFly + 1;
inline constexpr int kHangarBack = kHangarFly + 2;
inline constexpr int kHangarRows = kHangarFly + 3;
[[nodiscard]] HangarRow hangar_row(int row) noexcept;

struct Hangar {
    int row = 0;
    int step = 0;         // the plane whose upgrades show
    bool confirm = false; // pressed once on a purchase or reset: press again to do it
};
// The hangar as it opens: on the plane chosen to fly.
[[nodiscard]] Hangar open_hangar(const NationProgress& n) noexcept;

// Moves the cursor (arrows or the mouse). Moving cancels a confirmation, and onto a plane shows
// that plane's upgrades.
void hangar_point(Hangar& h, int row) noexcept;
void hangar_move(Hangar& h, int by) noexcept; // wraps round

enum class HangarAction : std::uint8_t {
    None,
    Fly,
    Back,
    Changed, // the progress changed: save it
};
// Enter or a click on the cursor's row. Buying and resetting take two presses.
HangarAction hangar_select(Hangar& h, NationProgress& n, const Prices& p) noexcept;

} // namespace wof
