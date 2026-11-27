#pragma once

#include <cstdint>
#include <vector>

namespace wof {

// Practice targets (design/flight.md, Weapons): something to shoot at before damage and enemies
// arrive. Drones circle at fixed heights; ground targets sit on the farmland. Hits don't hurt
// them: they count up and flash. They stay as a developer tool afterwards.

enum class TargetKind : std::uint8_t { Drone, Ground };

// The targets' numbers (tuning.txt, `targets.*`).
struct TargetParams {
    float drone_speed_u_s = 70.0f;
    float ground_radius_u = 10.0f; // a ground target's size

    friend constexpr bool operator==(const TargetParams&, const TargetParams&) = default;
};

struct Target {
    TargetKind kind = TargetKind::Ground;
    float cx = 0.0f; // a drone's orbit centre, or a ground target's position
    float cy = 0.0f;
    float orbit_u = 0.0f;   // a drone's orbit radius
    float angle_deg = 0.0f; // where on the orbit it is, as a compass bearing from the centre
    bool clockwise = true;
    float x = 0.0f; // where it is now
    float y = 0.0f;
    float z = 0.0f;
    float heading_deg = 0.0f;
    int hits = 0;
    float since_hit_s = 1e9f; // time since the last hit

    friend constexpr bool operator==(const Target&, const Target&) = default;
};

// The practice layout: 4 drones circling at 40, 90, 140 and 200 u over the middle of the map, and
// 8 ground targets along the way north.
[[nodiscard]] std::vector<Target> practice_targets(const TargetParams& p);

// Moves the drones one step along their orbits and ages the hit timers.
void move_targets(std::vector<Target>& targets, const TargetParams& p, float dt) noexcept;

// Records a hit on a target.
void hit_target(Target& t) noexcept;

} // namespace wof
