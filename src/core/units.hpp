#pragma once

namespace wof {

// What the HUD shows the simulation's units as (design/flight.md, HUD). The battle area is squeezed
// to fit the screen, so these are set for the feel of a WWII fighter, not from ground distances.
inline constexpr float kFeetPerU = 50.0f; // height: the 240 u ceiling reads 12,000 ft
inline constexpr float kMphPerUs = 2.4f;  // speed: 150 u/s reads 360 mph

} // namespace wof
