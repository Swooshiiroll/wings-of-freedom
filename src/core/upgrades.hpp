#pragma once

#include <cstdint>
#include <string_view>

#include "core/damage.hpp"
#include "core/flight.hpp"
#include "core/plane_rules.hpp"
#include "core/planes.hpp"

namespace wof {

// The upgrades of the aircraft tree (design/aircraft_tree.md): six per plane, each bought once and
// then always fitted.

enum class Upgrade : std::uint8_t { Guns, Engine, Fuel, Armour, Bombs, Sight };
inline constexpr int kUpgrades = 6;

// A set of upgrades: upgrade_bit()s.
using UpgradeSet = std::uint8_t;
inline constexpr UpgradeSet kAllUpgrades = 0x3f;

[[nodiscard]] constexpr UpgradeSet upgrade_bit(Upgrade u) noexcept {
    return static_cast<UpgradeSet>(1u << static_cast<unsigned>(u));
}
[[nodiscard]] constexpr bool has_upgrade(UpgradeSet set, Upgrade u) noexcept {
    return (set & upgrade_bit(u)) != 0;
}

// The short name used in files and on the command line: guns, engine, fuel, armour, bombs, sight.
[[nodiscard]] const char* upgrade_key(Upgrade u) noexcept;
bool parse_upgrade(std::string_view text, Upgrade& out) noexcept;

// How a plane's upgrade is shown in the hangar, and whether it's a real fit of the type or a close
// variant, or a field mod.
struct UpgradeInfo {
    const char* label = "";
    bool real = false;
};
[[nodiscard]] UpgradeInfo upgrade_info(PlaneId plane, Upgrade u) noexcept;

// What the upgrades are worth (tuning.txt `upgrade.*`), all (start) values.
struct UpgradeParams {
    float engine_speed = 1.06f;   // top speed with the engine, times this
    float engine_climb = 1.10f;   // and climb
    float fuel_endurance = 1.45f; // endurance with the extra tank
    float armour_pilot = 0.5f;    // the share of hits on the pilot that armour lets through
    float sealed_leak = 0.5f;     // the share of a fuel leak a sealed tank lets through (P-36)

    friend constexpr bool operator==(const UpgradeParams&, const UpgradeParams&) = default;
};

// A plane's rules with a set of upgrades fitted.
[[nodiscard]] PlaneRules fit_plane(PlaneId plane, UpgradeSet set, const FlightParams& base,
                                   const DamageParams& damage, const PlaneScale& scale,
                                   const UpgradeParams& up);

} // namespace wof
