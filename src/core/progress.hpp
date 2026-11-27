#pragma once

#include <array>
#include <string>
#include <string_view>

#include "core/planes.hpp"
#include "core/upgrades.hpp"

namespace wof {

// What the player has earned (design/aircraft_tree.md): points, planes and upgrades, per nation.
// The front end keeps it in progress.txt next to the game.

// Prices (tuning.txt `price.*`), all (start) values.
struct Prices {
    int step2 = 3000; // the second fighter of a tree
    int step3 = 8000; // the third
    int guns = 800;
    int engine = 800;
    int fuel = 500;
    int bombs = 500;
    int armour = 400;
    int sight = 300;

    friend constexpr bool operator==(const Prices&, const Prices&) = default;
};
[[nodiscard]] int plane_price(int step, const Prices& p) noexcept; // step 0 is free
[[nodiscard]] int upgrade_price(Upgrade u, const Prices& p) noexcept;

struct PlaneProgress {
    bool owned = false;
    UpgradeSet upgrades = 0;

    friend constexpr bool operator==(const PlaneProgress&, const PlaneProgress&) = default;
};

// One nation's points and tree. You always own the first fighter.
struct NationProgress {
    int points = 0;
    std::array<PlaneProgress, kTreeSteps> planes{{{true, 0}, {}, {}}};
    int flying = 0; // the tree step chosen to fly

    friend constexpr bool operator==(const NationProgress&, const NationProgress&) = default;
};

struct Progress {
    std::array<NationProgress, 2> nations{}; // by nation_slot()

    friend constexpr bool operator==(const Progress&, const Progress&) = default;
};
// v0.1's two nations: the US is 0, Germany 1.
[[nodiscard]] constexpr int nation_slot(Nation n) noexcept {
    return n == Nation::Germany ? 1 : 0;
}

// Why a purchase can or can't happen.
enum class BuyState : std::uint8_t {
    Owned,  // bought already
    Locked, // the plane before it (or, for an upgrade, the plane) isn't owned
    TooDear,
    Open, // can be bought
};
[[nodiscard]] BuyState plane_state(const NationProgress& n, int step, const Prices& p) noexcept;
[[nodiscard]] BuyState upgrade_state(const NationProgress& n, int step, Upgrade u,
                                     const Prices& p) noexcept;
// Buys if open; false (and nothing changes) otherwise.
bool buy_plane(NationProgress& n, int step, const Prices& p) noexcept;
bool buy_upgrade(NationProgress& n, int step, Upgrade u, const Prices& p) noexcept;

// progress.txt: `key = value` lines.
[[nodiscard]] std::string to_progress(const Progress& g);
// Reads a progress text over `g`: what it doesn't mention keeps its value. Each unknown line or
// bad value adds a line to `errors`. True if there were none.
bool from_progress(std::string_view text, Progress& g, std::string& errors);

} // namespace wof
