#pragma once

#include <string>
#include <string_view>

#include "core/camera.hpp"
#include "core/ground.hpp"
#include "core/world.hpp"

namespace wof {

// Numbers for how things look rather than how they play (tuning.txt, `look.*`).
struct LookParams {
    float scorch_fade_s = 30.0f; // a bomb's scorch mark fades out over this long

    friend constexpr bool operator==(const LookParams&, const LookParams&) = default;
};

// Every gameplay number that playtesting tunes, as (start) values (design/flight.md). The game
// keeps them in tuning.txt next to the executable and reloads that file whenever it changes.
struct Tuning {
    CameraParams camera;
    Rules rules; // flight, edges, weapons, practice targets (the height rule isn't in the file)
    GroundParams ground;
    LookParams look;

    friend bool operator==(const Tuning&, const Tuning&) = default;
};

// The tuning file's text: every value as `key = value`, one per line, in a fixed order, with a
// comment line per section.
//
//   camera.lead_u = 40
//   flight.max_level_u_s = 150
//   ground.field_max_u = 160
[[nodiscard]] std::string to_tuning(const Tuning& t);

// Reads a tuning text over `t`: keys it doesn't mention keep their current values. Each unknown
// key, bad value or value that had to be clamped into range adds a line to `errors`. Returns
// true if there were none.
bool from_tuning(std::string_view text, Tuning& t, std::string& errors);

} // namespace wof
