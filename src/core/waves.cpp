#include "core/waves.hpp"

#include <algorithm>
#include <cstddef>

#include "core/ai.hpp"
#include "core/map.hpp"
#include "core/world.hpp"

namespace wof {
namespace {

// One enemy fighter from the north edge, heading south near the player's height. Returns its
// height.
float arrive(World& w, const Rules& rules, int index) {
    const WaveParams& p = rules.wave;
    const Body& player = w.bodies.front();
    const auto width = static_cast<float>(kMapWidthU);
    // Near the player's side of the map, spread across.
    const float x = std::clamp(player.x + (w.rng.next_float() - 0.5f) * 600.0f +
                                   static_cast<float>(index) * p.spread_u,
                               rules.edge.warning_u, width - rules.edge.warning_u);
    const float y = static_cast<float>(kMapDepthU) - p.north_inset_u;
    Body b = new_plane(rules, w.waves.enemy, x, y, 180.0f);
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
    while (s.waiting > 0 && alive < rules.wave.max_alive) {
        const float z = arrive(w, rules, arrived);
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
