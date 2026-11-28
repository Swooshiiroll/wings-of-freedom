#pragma once

#include <string>
#include <string_view>

#include "core/camera.hpp"
#include "core/ground.hpp"
#include "core/world.hpp"

namespace wof {

// Numbers for how things look rather than how they play (tuning.txt, `look.*`).
struct LookParams {
    float scorch_fade_s = 30.0f;   // a bomb's scorch mark fades out over this long
    float zoom_at_ceiling = 0.25f; // the ground's size seen from the ceiling (design/planes.md)

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

// The tuning file's format. 2 (alpha.5, real planes): each plane's flight comes from its real
// stats, so the old per-plane flight keys are gone and the ceiling is 30,000 ft. 3 (alpha.6): the
// tanks are doubled and the ground is a quarter size from the ceiling. 4 (alpha.10): the bigger
// map, with waves from its edges (wave.north_inset_u replaced). 5 (v0.1.0-pre.2): waves wait until
// cleared (wave.every_s and along_u replaced), a higher start and a steadier camera. A file of an
// older format is set aside rather than read, so its old numbers don't quietly win.
inline constexpr int kTuningFormat = 5;

// The format a tuning text says it is (its `format = N` line), or 1 if it doesn't say: files from
// before the line existed.
[[nodiscard]] int tuning_format(std::string_view text);

// The tuning file's text: the format line, then every value as `key = value`, one per line, in a
// fixed order, with a comment line per section.
//
//   format = 3
//   camera.lead_u = 40
//   flight.ceiling_u = 600
//   ground.field_max_u = 160
[[nodiscard]] std::string to_tuning(const Tuning& t);

// Reads a tuning text over `t`: keys it doesn't mention keep their current values. Each unknown
// key, bad value or value that had to be clamped into range adds a line to `errors`. Returns
// true if there were none.
bool from_tuning(std::string_view text, Tuning& t, std::string& errors);

} // namespace wof
