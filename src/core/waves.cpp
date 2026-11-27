#include "core/waves.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "core/ai.hpp"
#include "core/map.hpp"
#include "core/world.hpp"

namespace wof {
namespace {

// The edges enemies fly in from: never the south, your way home.
enum class Edge : std::uint8_t { North, East, West };

// One enemy fighter in from an edge, heading in, near the player's height: abreast of the player or
// north of them (design/flight.md, Waves). Returns its height.
float arrive(World& w, const Rules& rules, Edge edge, int index) {
    const WaveParams& p = rules.wave;
    const Body& player = w.bodies.front();
    const auto width = static_cast<float>(kMapWidthU);
    const auto depth = static_cast<float>(kMapDepthU);
    const float margin = rules.edge.warning_u;
    const float along = static_cast<float>(index) * p.spread_u;
    float x = 0.0f;
    float y = 0.0f;
    float heading = 180.0f;
    if (edge == Edge::North) {
        x = std::clamp(player.x + (w.rng.next_float() - 0.5f) * p.along_u + along, margin,
                       width - margin);
        y = depth - p.edge_inset_u;
    } else {
        y = std::clamp(player.y + w.rng.next_float() * p.along_u + along, margin, depth - margin);
        x = edge == Edge::East ? width - p.edge_inset_u : p.edge_inset_u;
        heading = edge == Edge::East ? 270.0f : 90.0f;
    }
    Body b = new_plane(rules, w.waves.enemy, x, y, heading);
    const PlaneRules& plane = plane_of(rules, b);
    const float offset = (w.rng.next_float() * 2.0f - 1.0f) * p.height_spread_u;
    b.z = std::clamp(player.z + offset, rules.ai.floor_u + 20.0f, plane.flight.ceiling_u - 20.0f);
    b.speed_u_s = plane.flight.max_level_u_s * 0.8f;
    b.throttle = 1.0f;
    const auto style = static_cast<Style>(w.rng.next_below(kStyles));
    w.pilots.push_back(new_pilot(static_cast<int>(w.bodies.size()), style,
                                 skill_of(rules.ai, w.waves.current), w.rng));
    w.bodies.push_back(b);
    return b.z;
}

} // namespace

WaveState start_waves(const Rules& rules, PlaneId enemy, int first) noexcept {
    WaveState s;
    s.enabled = true;
    s.enemy = enemy;
    s.next = std::max(1, first);
    s.timer_s = rules.wave.first_s;
    return s;
}

int enemies_alive(const World& w) noexcept {
    int n = 0;
    for (const Pilot& p : w.pilots) {
        const Body& b = w.bodies[static_cast<std::size_t>(p.body)];
        n += b.outcome == Outcome::Flying && !b.damage.going_down ? 1 : 0;
    }
    return n;
}

void update_waves(World& w, const Rules& rules) {
    WaveState& s = w.waves;
    if (!s.enabled || w.bodies.empty() || w.bodies.front().outcome != Outcome::Flying) {
        return;
    }
    bool new_wave = false;
    s.timer_s -= kTickSeconds;
    if (s.timer_s <= 0.0f) {
        s.current = s.next++;
        s.waiting += wave_size(s.current);
        s.timer_s += std::max(1.0f, rules.wave.every_s);
        new_wave = true;
    }
    int arrived = 0;
    float height = w.bodies.front().z;
    int alive = enemies_alive(w);
    // Those arriving together come in from one edge.
    const Edge edge = s.waiting > 0 && alive < rules.wave.max_alive
                          ? static_cast<Edge>(w.rng.next_below(3))
                          : Edge::North;
    while (s.waiting > 0 && alive < rules.wave.max_alive) {
        const float z = arrive(w, rules, edge, arrived);
        if (arrived == 0) {
            height = z;
        }
        ++arrived;
        ++alive;
        --s.waiting;
    }
    if (new_wave) {
        // who: the wave's number; damage: how many it brings; z: about their height.
        const Body& player = w.bodies.front();
        w.events.push_back(Event{EventKind::WaveArrived, s.current, -1, player.x, player.y, height,
                                 static_cast<float>(wave_size(s.current))});
    }
}

} // namespace wof
