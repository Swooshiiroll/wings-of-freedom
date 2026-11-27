#include "core/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "core/iso.hpp"

namespace wof {

void step(World& world, std::span<const InputCommand> commands, const StandInParams& p) {
    constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
    for (std::size_t i = 0; i < world.bodies.size(); ++i) {
        const InputCommand cmd = i < commands.size() ? commands[i] : InputCommand{};
        Body& b = world.bodies[i];
        b.heading_deg = wrap_heading(b.heading_deg + axis(cmd.turn) * p.turn_deg_s * kTickSeconds);
        b.z = std::clamp(b.z + axis(cmd.pitch) * p.climb_u_s * kTickSeconds, 0.0f, p.ceiling_u);
        b.speed_u_s = p.speed_u_s;
        const float h = b.heading_deg * kDegToRad;
        b.x += std::sin(h) * b.speed_u_s * kTickSeconds;
        b.y += std::cos(h) * b.speed_u_s * kTickSeconds;
    }
    ++world.tick;
}

} // namespace wof
