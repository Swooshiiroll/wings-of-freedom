#include "core/upgrades.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace wof {
namespace {

// design/aircraft_tree.md's table: R a real fit, F a field mod. The sight is the same on every
// plane.
struct PlaneUpgrades {
    PlaneId plane;
    std::array<UpgradeInfo, kUpgrades - 1> list; // Guns to Bombs
};
constexpr std::array<PlaneUpgrades, 6> kTable = {{
    {PlaneId::P12E,
     {{{".50 FOR A .30", true},
       {"600 HP ENGINE (P-12F)", true},
       {"BELLY TANK", false},
       {"SEAT ARMOUR PLATE", false},
       {"4 x 116 LB BOMBS", false}}}},
    {PlaneId::P26A,
     {{{".50 FOR A .30", true},
       {"INJECTED ENGINE (P-26B)", true},
       {"BELLY TANK", false},
       {"SEAT ARMOUR PLATE", false},
       {"4 x 100 LB BOMBS", false}}}},
    {PlaneId::P36A,
     {{{"2 WING .30S (P-36C)", true},
       {"1200 HP ENGINE (P-36C)", true},
       {"AUXILIARY TANK", false},
       {"ARMOUR, SEALED TANK", true},
       {"2 x 100 LB BOMB RACKS", true}}}},
    {PlaneId::Ar65E,
     {{{"THIRD MG 17", false},
       {"BOOSTED BMW VI", false},
       {"BELLY TANK", false},
       {"SEAT ARMOUR PLATE", false},
       {"6 x 10 KG BOMBS", true}}}},
    {PlaneId::He51B,
     {{{"THIRD MG 17", false},
       {"BOOSTED BMW VI", false},
       {"170 L TANK (B-1)", true},
       {"SEAT ARMOUR PLATE", false},
       {"4 x 50 KG BOMBS (C-1)", true}}}},
    {PlaneId::Bf109B,
     {{{"2 WING MG 17S (C-1)", true},
       {"INJECTED JUMO (C-1)", true},
       {"DROP TANK", false},
       {"SEAT ARMOUR PLATE", false},
       {"4 x 50 KG BELLY RACK", false}}}},
}};

constexpr float kWingGunU = 4.0f; // wing guns sit this far out from the nose

GunSpec rifle(int rounds, float side) noexcept {
    return {Calibre::Rifle, 1000.0f, rounds, side, Mount::Fixed};
}

void fit_guns(PlaneType& t, PlaneId plane) noexcept {
    switch (plane) {
    case PlaneId::P12E:
    case PlaneId::P26A:
        t.guns[1] = {Calibre::Heavy, 600.0f, 200, 1.0f, Mount::Fixed};
        break;
    case PlaneId::P36A:
        t.guns[2] = rifle(500, -kWingGunU);
        t.guns[3] = rifle(500, kWingGunU);
        t.gun_count = 4;
        break;
    case PlaneId::Ar65E:
    case PlaneId::He51B:
        t.guns[2] = rifle(500, 0.0f);
        t.gun_count = 3;
        break;
    case PlaneId::Bf109B:
        t.guns[2] = rifle(420, -kWingGunU);
        t.guns[3] = rifle(420, kWingGunU);
        t.gun_count = 4;
        break;
    case PlaneId::B10B:
    case PlaneId::He111B2:
        break;
    }
}

void fit_bombs(PlaneType& t, PlaneId plane) noexcept {
    switch (plane) {
    case PlaneId::P12E:
        t.bomb_lb = 116.0f;
        t.bombs = 4;
        break;
    case PlaneId::P26A:
        t.bomb_lb = 100.0f;
        t.bombs = 4;
        break;
    case PlaneId::P36A:
        t.bomb_lb = 100.0f;
        t.bombs = 2;
        break;
    case PlaneId::Ar65E:
        t.bomb_lb = 22.05f; // 10 kg
        t.bombs = 6;
        break;
    case PlaneId::He51B:
    case PlaneId::Bf109B:
        t.bomb_lb = 110.2f; // 50 kg
        t.bombs = 4;
        break;
    case PlaneId::B10B:
    case PlaneId::He111B2:
        break;
    }
}

} // namespace

const char* upgrade_key(Upgrade u) noexcept {
    switch (u) {
    case Upgrade::Guns:
        return "guns";
    case Upgrade::Engine:
        return "engine";
    case Upgrade::Fuel:
        return "fuel";
    case Upgrade::Armour:
        return "armour";
    case Upgrade::Bombs:
        return "bombs";
    case Upgrade::Sight:
        return "sight";
    }
    return "?";
}

bool parse_upgrade(std::string_view text, Upgrade& out) noexcept {
    for (int i = 0; i < kUpgrades; ++i) {
        const auto u = static_cast<Upgrade>(i);
        if (text == upgrade_key(u)) {
            out = u;
            return true;
        }
    }
    return false;
}

UpgradeInfo upgrade_info(PlaneId plane, Upgrade u) noexcept {
    if (u == Upgrade::Sight) {
        return {"BOMB SIGHT", false};
    }
    for (const PlaneUpgrades& p : kTable) {
        if (p.plane == plane) {
            return p.list[static_cast<std::size_t>(u)];
        }
    }
    return {};
}

PlaneRules fit_plane(PlaneId plane, UpgradeSet set, const FlightParams& base,
                     const DamageParams& damage, const PlaneScale& scale, const UpgradeParams& up) {
    PlaneType t = plane_type(plane);
    DamageParams d = damage;
    if (has_upgrade(set, Upgrade::Guns)) {
        fit_guns(t, plane);
    }
    if (has_upgrade(set, Upgrade::Engine)) {
        t.top_mph *= up.engine_speed;
        t.climb_fpm *= up.engine_climb;
    }
    if (has_upgrade(set, Upgrade::Fuel)) {
        t.endurance_h *= up.fuel_endurance;
    }
    if (has_upgrade(set, Upgrade::Armour)) {
        // A wound takes more hitting.
        d.wound_damage /= std::max(0.1f, up.armour_pilot);
        if (plane == PlaneId::P36A) {
            d.leak_tank_s /= std::max(0.1f, up.sealed_leak);
        }
    }
    if (has_upgrade(set, Upgrade::Bombs)) {
        fit_bombs(t, plane);
    }
    PlaneRules r = derive_plane(t, base, d, scale);
    r.id = plane;
    return r;
}

} // namespace wof
