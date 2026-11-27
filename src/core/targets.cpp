#include "core/targets.hpp"

#include <array>
#include <cmath>
#include <numbers>

#include "core/iso.hpp"

namespace wof {
namespace {

constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

struct DroneSpot {
    float cx;
    float cy;
    float orbit_u;
    float z;
    bool clockwise;
};

// North of the start (1440, 480), across the middle of the battle area.
constexpr std::array<DroneSpot, 4> kDrones = {{
    {1440.0f, 1300.0f, 180.0f, 40.0f, true},
    {1000.0f, 1900.0f, 220.0f, 90.0f, false},
    {1900.0f, 2400.0f, 250.0f, 140.0f, true},
    {1440.0f, 3100.0f, 150.0f, 200.0f, false},
}};

constexpr std::array<std::array<float, 2>, 8> kGround = {{
    {1440.0f, 1000.0f},
    {1300.0f, 1500.0f},
    {1600.0f, 1700.0f},
    {1200.0f, 2200.0f},
    {1700.0f, 2600.0f},
    {1440.0f, 3000.0f},
    {1100.0f, 3500.0f},
    {1800.0f, 3800.0f},
}};

void place(Target& t) noexcept {
    const float a = t.angle_deg * kDegToRad;
    t.x = t.cx + std::sin(a) * t.orbit_u;
    t.y = t.cy + std::cos(a) * t.orbit_u;
    t.heading_deg = wrap_heading(t.angle_deg + (t.clockwise ? 90.0f : -90.0f));
}

} // namespace

std::vector<Target> practice_targets(const TargetParams& p) {
    (void)p;
    std::vector<Target> out;
    float start = 0.0f;
    for (const DroneSpot& d : kDrones) {
        Target t;
        t.kind = TargetKind::Drone;
        t.cx = d.cx;
        t.cy = d.cy;
        t.orbit_u = d.orbit_u;
        t.z = d.z;
        t.clockwise = d.clockwise;
        t.angle_deg = start;
        start += 97.0f; // spread out round their orbits
        place(t);
        out.push_back(t);
    }
    for (const auto& g : kGround) {
        Target t;
        t.kind = TargetKind::Ground;
        t.cx = g[0];
        t.cy = g[1];
        t.x = g[0];
        t.y = g[1];
        out.push_back(t);
    }
    return out;
}

void move_targets(std::vector<Target>& targets, const TargetParams& p, float dt) noexcept {
    for (Target& t : targets) {
        t.since_hit_s += dt;
        if (t.kind != TargetKind::Drone || t.orbit_u <= 0.0f) {
            continue;
        }
        const float turn = p.drone_speed_u_s / t.orbit_u / kDegToRad * dt;
        t.angle_deg = wrap_heading(t.angle_deg + (t.clockwise ? turn : -turn));
        place(t);
    }
}

void hit_target(Target& t) noexcept {
    ++t.hits;
    t.since_hit_s = 0.0f;
}

} // namespace wof
