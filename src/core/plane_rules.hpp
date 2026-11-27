#pragma once

#include <array>
#include <vector>

#include "core/damage.hpp"
#include "core/flight.hpp"
#include "core/planes.hpp"

namespace wof {

// A plane type's game rules, worked out from its real stats (core/planes.hpp) and the scale:
// how it flies, how much it takes, and what it carries.
struct PlaneRules {
    PlaneId id = PlaneId::P26A;
    FlightParams flight;
    Aero aero;
    DamageParams damage;
    std::array<GunSpec, kMaxGuns> guns{};
    int gun_count = 0;
    float bomb_lb = 0.0f;
    int bombs = 0;

    friend bool operator==(const PlaneRules&, const PlaneRules&) = default;
};

// The best sustained climb a flight model gives at full throttle (u/s): the most of
// speed x (thrust - drag) / g over its speeds.
[[nodiscard]] float best_climb(const FlightParams& f) noexcept;

// A plane's flight model from its real stats:
// - top speed, dive limit, stall, turn rate, best turn speed, pitch rate and tank from the scale;
// - the propeller gives constant power above the stall speed, set so full throttle levels off at
//   the top speed;
// - the best glide speed (which sets the drag curve) is found so the best sustained climb is the
//   scaled real climb.
// Everything else (gravity, ceiling, pitch limit, throttle, stall behaviour, start state) comes
// from `base`.
[[nodiscard]] FlightParams derive_flight(const PlaneType& t, const FlightParams& base,
                                         const PlaneScale& s) noexcept;

// All of a plane's rules: its flight, its HP and engines on top of `damage`'s hit table and
// effects, and its guns and bombs.
[[nodiscard]] PlaneRules derive_plane(const PlaneType& t, const FlightParams& base,
                                      const DamageParams& damage, const PlaneScale& s);

// The whole roster's rules, by PlaneId.
[[nodiscard]] std::vector<PlaneRules>
derive_roster(const FlightParams& base, const DamageParams& damage, const PlaneScale& s);

} // namespace wof
