#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "core/planes.hpp"

namespace wof {

// What the HUD shows the simulation's units as (design/flight.md, HUD). The battle area is squeezed
// to fit the screen, so these are set for the feel of a WWII fighter, not from ground distances.
inline constexpr float kFeetPerU = 50.0f; // height: the 600 u ceiling reads 30,000 ft
inline constexpr float kMetresPerFoot = 0.3048f;
inline constexpr float kKmhPerMph = 1.609344f;

// The units setting (settings.txt), for arrows, banners and the like. A plane's own gauges always
// read its nation's units (design/dashboard.md).
enum class Units : std::uint8_t { Nation, Imperial, Metric };
inline constexpr int kUnits = 3;

enum class UnitSystem : std::uint8_t { Imperial, Metric };

// A nation's instruments: American and British ones read imperial, the others metric.
[[nodiscard]] constexpr UnitSystem nation_units(Nation n) noexcept {
    return n == Nation::US || n == Nation::UK ? UnitSystem::Imperial : UnitSystem::Metric;
}

// What the setting means for a player of `n`.
[[nodiscard]] constexpr UnitSystem units_of(Units setting, Nation n) noexcept {
    return setting == Units::Imperial ? UnitSystem::Imperial
           : setting == Units::Metric ? UnitSystem::Metric
                                      : nation_units(n);
}

// A height in u as feet or metres.
[[nodiscard]] constexpr float height_in(float u, UnitSystem s) noexcept {
    const float feet = u * kFeetPerU;
    return s == UnitSystem::Imperial ? feet : feet * kMetresPerFoot;
}

// A whole number with a thousands comma, to the nearest `step`: "7,000", "-600".
[[nodiscard]] inline std::string grouped(float value, int step) {
    const int v = static_cast<int>(std::lround(value / static_cast<float>(step))) * step;
    const int a = std::abs(v);
    std::string out = v < 0 ? "-" : "";
    if (a >= 1000) {
        const std::string rest = std::to_string(a % 1000);
        out += std::to_string(a / 1000) + "," + std::string(3 - rest.size(), '0') + rest;
    } else {
        out += std::to_string(a);
    }
    return out;
}

// A height as text: "7,000 FT" or "2,100 M" (to the nearest 100).
[[nodiscard]] inline std::string height_text(float u, UnitSystem s) {
    return grouped(height_in(u, s), 100) + (s == UnitSystem::Imperial ? " FT" : " M");
}

// A height difference as text: "+2,000", "-600", or "LEVEL" when within half a step.
[[nodiscard]] inline std::string height_diff_text(float du, UnitSystem s) {
    const float v = height_in(du, s);
    if (std::abs(v) < 50.0f) {
        return "LEVEL";
    }
    return (v > 0.0f ? "+" : "") + grouped(v, 100);
}

} // namespace wof
