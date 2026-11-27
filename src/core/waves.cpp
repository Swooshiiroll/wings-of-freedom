#include "core/waves.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

#include "core/ai.hpp"
#include "core/map.hpp"
#include "core/units.hpp"
#include "core/world.hpp"

namespace wof {
namespace {

// The edges enemies fly in from: never the south, your way home.
enum class Edge : std::uint8_t { North, East, West };

constexpr float kToRad = std::numbers::pi_v<float> / 180.0f;

// The type a fighter of wave n flies: from the tree steps for it, either when there are two.
PlaneId fighter_for(World& w, const Rules& rules, int n) {
    const TierSpan span = enemy_steps(n, rules.wave.tier_every);
    const int step = span.lo == span.hi || w.rng.next_below(2) == 0 ? span.lo : span.hi;
    return w.waves.enemy[static_cast<std::size_t>(step)];
}

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
    Body b = new_plane(rules, fighter_for(w, rules, w.waves.current), x, y, heading);
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

// Wave n's raid: bombers in a V from the north edge, straight for your base, with their escorts.
void raid(World& w, const Rules& rules, int n) {
    const WaveParams& p = rules.wave;
    const int count = raid_size(n, p.raid_every);
    const int escorts = raid_escorts(n, p.raid_every);
    const Bombers fleet = bombers_of(plane_type(w.waves.enemy.front()).nation);
    const PlaneId type = n >= p.raid_later ? fleet.later : fleet.early;
    const PlaneType& t = plane_type(type);

    const auto width = static_cast<float>(kMapWidthU);
    const float base_x = width * 0.5f;
    const float wide = p.raid_spacing_u * 2.0f;
    const float x = std::clamp(base_x + (w.rng.next_float() * 2.0f - 1.0f) * p.raid_x_u,
                               wide + rules.edge.warning_u, width - wide - rules.edge.warning_u);
    const float y = static_cast<float>(kMapDepthU) - p.edge_inset_u;
    const float course = std::atan2(base_x - x, -y) / kToRad;
    const float heading = course < 0.0f ? course + 360.0f : course;
    const float top = std::max(p.raid_low_u, t.raid_top_ft / kFeetPerU);
    const float z = p.raid_low_u + w.rng.next_float() * (top - p.raid_low_u);

    std::vector<PlaneId> guard;
    float cruise = t.cruise_mph * rules.scale.u_per_mph;
    for (int e = 0; e < escorts; ++e) {
        guard.push_back(fighter_for(w, rules, n));
        // An escorted formation flies no faster than its escorts can hold.
        const Body probe = new_plane(rules, guard.back(), 0.0f, 0.0f, 0.0f);
        cruise = std::min(cruise, plane_of(rules, probe).flight.max_level_u_s * 0.9f);
    }

    const int id = ++w.waves.raids;
    const float fx = std::sin(heading * kToRad); // forward
    const float fy = std::cos(heading * kToRad);
    const float rx = fy; // right
    const float ry = -fx;
    const auto place = [&](float right, float ahead) {
        return std::pair{x + rx * right + fx * ahead, y + ry * right + fy * ahead};
    };
    for (int i = 0; i < count; ++i) {
        // A pair abreast; larger formations a V behind the leader.
        const int rank = (i + 1) / 2;
        const float side = i % 2 == 1 ? -1.0f : 1.0f;
        const float right = count == 2 ? (i == 0 ? -0.5f : 0.5f) * p.raid_spacing_u
                                       : side * static_cast<float>(rank) * p.raid_spacing_u;
        const float ahead = count == 2 ? 0.0f : -static_cast<float>(rank) * p.raid_back_u;
        const auto [bx, by] = place(right, ahead);
        Body b = new_plane(rules, type, bx, by, heading);
        b.z = z;
        b.speed_u_s = cruise;
        b.throttle = 0.8f;
        Pilot pilot =
            new_pilot(static_cast<int>(w.bodies.size()), Style::Turn, skill_of(rules.ai, n), w.rng);
        pilot.duty = Duty::Bomb;
        pilot.raid = id;
        pilot.course_deg = heading;
        pilot.height_u = z;
        pilot.cruise_u_s = cruise;
        w.pilots.push_back(pilot);
        w.bodies.push_back(b);
    }
    for (int e = 0; e < escorts; ++e) {
        const float side = e % 2 == 0 ? 1.0f : -1.0f;
        const float right = side * (static_cast<float>(count / 2 + 1) * p.raid_spacing_u);
        const auto [ex, ey] = place(right, 60.0f);
        Body b = new_plane(rules, guard[static_cast<std::size_t>(e)], ex, ey, heading);
        b.z = z + 20.0f;
        b.speed_u_s = cruise;
        b.throttle = 0.8f;
        const auto style = static_cast<Style>(w.rng.next_below(kStyles));
        Pilot pilot =
            new_pilot(static_cast<int>(w.bodies.size()), style, skill_of(rules.ai, n), w.rng);
        pilot.duty = Duty::Escort;
        pilot.raid = id;
        pilot.cruise_u_s = cruise;
        pilot.station_right_u = right;
        pilot.station_ahead_u = 60.0f;
        w.pilots.push_back(pilot);
        w.bodies.push_back(b);
    }
    // who: the wave; by: its escorts; damage: its bombers; x, y, z: the formation.
    w.events.push_back(
        Event{EventKind::RaidArrived, n, escorts, x, y, z, static_cast<float>(count)});
}

} // namespace

WaveState start_waves(const Rules& rules, const FighterTree& enemy, int first) noexcept {
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
        n += p.duty == Duty::Hunt && b.outcome == Outcome::Flying && !b.damage.going_down ? 1 : 0;
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
        if (raid_size(s.current, rules.wave.raid_every) > 0) {
            raid(w, rules, s.current);
        }
        // who: the wave's number; damage: how many it brings; z: about their height.
        const Body& player = w.bodies.front();
        w.events.push_back(Event{EventKind::WaveArrived, s.current, -1, player.x, player.y, height,
                                 static_cast<float>(wave_size(s.current))});
    }
}

} // namespace wof
