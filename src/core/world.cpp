#include "core/world.hpp"

#include <cstddef>

#include "core/map.hpp"

namespace wof {

void apply_edges(Body& b, const EdgeParams& e, float dt) noexcept {
    if (b.outcome != Outcome::Flying) {
        return;
    }
    if (b.z <= 0.0f) {
        b.z = 0.0f;
        b.outcome = Outcome::Crashed;
        return;
    }
    if (b.y < 0.0f) {
        b.outcome = Outcome::ReturnedHome;
        b.edge_warning = false;
        b.edge_countdown_s = -1.0f;
        return;
    }
    const auto w = static_cast<float>(kMapWidthU);
    const auto d = static_cast<float>(kMapDepthU);
    const bool outside = b.x < 0.0f || b.x > w || b.y > d;
    b.edge_warning =
        !outside && (b.x < e.warning_u || b.x > w - e.warning_u || b.y > d - e.warning_u);
    if (!outside) {
        b.edge_countdown_s = -1.0f;
        return;
    }
    if (b.edge_countdown_s < 0.0f) {
        b.edge_countdown_s = e.countdown_s;
    }
    b.edge_countdown_s -= dt;
    if (b.edge_countdown_s <= 0.0f) {
        b.edge_countdown_s = 0.0f;
        b.outcome = Outcome::LeftArea;
    }
}

void step(World& world, std::span<const InputCommand> commands, const Rules& rules) {
    const Aero aero = aero_of(rules.flight);
    for (std::size_t i = 0; i < world.bodies.size(); ++i) {
        Body& b = world.bodies[i];
        if (b.outcome != Outcome::Flying) {
            continue;
        }
        const InputCommand cmd = i < commands.size() ? commands[i] : InputCommand{};
        fly(b, cmd, rules.flight, aero, kTickSeconds);
        apply_edges(b, rules.edge, kTickSeconds);
    }
    ++world.tick;
}

} // namespace wof
