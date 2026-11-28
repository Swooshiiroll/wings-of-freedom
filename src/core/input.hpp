#pragma once

#include <algorithm>
#include <cstdint>

namespace wof {

// When the nose eases back to level by itself, once the pitch keys are let go (design/flight.md,
// Flight). Each player's own choice, so it travels with their commands.
enum class AutoLevel : std::uint8_t {
    On,       // always
    Assisted, // only when let go within flight.assist_zone_deg of level
    Off,      // never: the nose stays where it was left
};
inline constexpr int kAutoLevels = 3;

// What one player (or one AI pilot) asks of their plane for one tick. Every plane is driven only
// through these, never by the front end changing state directly, so a host or headless server
// can run the same simulation for co-op later, with clients sending their commands.
//
// The axes are whole numbers from -127 to 127, so a keyboard gives full deflection and a gamepad
// can later give anything in between, and a command is a few bytes on the wire.
struct InputCommand {
    std::int8_t turn = 0;      // -127 full left .. 127 full right
    std::int8_t pitch = 0;     // -127 full dive (W) .. 127 full climb (S)
    std::int8_t throttle = 0;  // -127 throttle down .. 127 throttle up (a rate, not a setting)
    bool guns = false;         // held: fire the guns
    bool heavy = false;        // held: release the selected heavy weapon
    bool switch_heavy = false; // pressed this tick: select the next heavy weapon
    bool next_wave = false;    // pressed this tick: call the next wave (design/flight.md, Waves)
    AutoLevel level = AutoLevel::On;

    friend constexpr bool operator==(const InputCommand&, const InputCommand&) = default;
};

inline constexpr int kAxisFull = 127;

// An axis as -1..1.
[[nodiscard]] constexpr float axis(std::int8_t v) noexcept {
    return std::clamp(static_cast<float>(v) / static_cast<float>(kAxisFull), -1.0f, 1.0f);
}

} // namespace wof
