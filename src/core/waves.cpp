#include "core/waves.hpp"

#include <algorithm>
#include <array>
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

// The sides enemies come from: never the south, your way home.
enum class Side : std::uint8_t { North, East, West };
constexpr std::uint32_t kSides = 3;

constexpr float kToRad = std::numbers::pi_v<float> / 180.0f;

// The type a fighter of wave n flies: from the tree steps for it, either when there are two.
PlaneId fighter_for(World& w, const Rules& rules, int n) {
    const TierSpan span = enemy_steps(n, rules.wave.tier_every);
    const int step = span.lo == span.hi || w.rng.next_below(2) == 0 ? span.lo : span.hi;
    return w.waves.enemy[static_cast<std::size_t>(step)];
}

// A side with room on the map for fighters to appear out_u from the player, at random.
Side pick_side(World& w, const Rules& rules) {
    const Body& player = w.bodies.front();
    const float out = rules.wave.out_u;
    const float margin = rules.edge.warning_u;
    std::array<Side, kSides> room{};
    std::uint32_t n = 0;
    if (player.y + out <= static_cast<float>(kMapDepthU) - margin) {
        room[n++] = Side::North;
    }
    if (player.x + out <= static_cast<float>(kMapWidthU) - margin) {
        room[n++] = Side::East;
    }
    if (player.x - out >= margin) {
        room[n++] = Side::West;
    }
    // Nowhere (only with a huge out_u): any side, as far as the map goes.
    return n > 0 ? room[w.rng.next_below(n)] : static_cast<Side>(w.rng.next_below(kSides));
}

// One enemy fighter in from a side, out_u from the player and heading for them, near their height:
// abreast of the player or north of them (design/flight.md, Waves). Returns its height.
float arrive(World& w, const Rules& rules, Side side, int index) {
    const WaveParams& p = rules.wave;
    const Body& player = w.bodies.front();
    const float margin = rules.edge.warning_u;
    const float along = static_cast<float>(index) * p.spread_u;
    // North: within 40 degrees either way. East and west: up to 30 degrees north of abreast.
    const float r = w.rng.next_float();
    const float from = side == Side::North  ? (r - 0.5f) * 80.0f
                       : side == Side::East ? 90.0f - r * 30.0f
                                            : 270.0f + r * 30.0f;
    float x = player.x + std::sin(from * kToRad) * p.out_u;
    float y = player.y + std::cos(from * kToRad) * p.out_u;
    (side == Side::North ? x : y) += along;
    x = std::clamp(x, margin, static_cast<float>(kMapWidthU) - margin);
    y = std::clamp(y, margin, static_cast<float>(kMapDepthU) - margin);
    const float toward = std::atan2(player.x - x, player.y - y) / kToRad;
    const float heading = toward < 0.0f ? toward + 360.0f : toward;
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

bool wave_up(const World& w) noexcept {
    if (w.waves.waiting > 0) {
        return true;
    }
    return std::any_of(w.pilots.begin(), w.pilots.end(), [&w](const Pilot& p) {
        const Body& b = w.bodies[static_cast<std::size_t>(p.body)];
        return b.outcome == Outcome::Flying && !b.damage.going_down;
    });
}

bool can_call_wave(const World& w, const Rules& rules) noexcept {
    const WaveState& s = w.waves;
    return s.enabled && s.current > 0 && !w.bodies.empty() &&
           w.bodies.front().outcome == Outcome::Flying && s.since_s >= rules.wave.call_after_s &&
           wave_up(w);
}

void update_waves(World& w, const Rules& rules, bool called) {
    WaveState& s = w.waves;
    if (!s.enabled || w.bodies.empty() || w.bodies.front().outcome != Outcome::Flying) {
        return;
    }
    bool new_wave = false;
    if (s.current == 0 || !wave_up(w)) {
        // The first wave's delay, or the break after everything's down.
        if (s.current > 0 && !s.cleared) {
            s.cleared = true;
            s.timer_s = rules.wave.break_s;
            const Body& player = w.bodies.front();
            w.events.push_back(
                Event{EventKind::WaveCleared, s.current, -1, player.x, player.y, player.z, 0.0f});
        }
        s.timer_s -= kTickSeconds;
        new_wave = s.timer_s <= 0.0f;
    } else {
        new_wave = called && can_call_wave(w, rules);
        s.since_s += kTickSeconds;
    }
    if (new_wave) {
        s.current = s.next++;
        s.waiting += wave_size(s.current);
        s.since_s = 0.0f;
        s.cleared = false;
    }
    int arrived = 0;
    float height = w.bodies.front().z;
    int alive = enemies_alive(w);
    // Those arriving together come in from one side.
    const Side side =
        s.waiting > 0 && alive < rules.wave.max_alive ? pick_side(w, rules) : Side::North;
    while (s.waiting > 0 && alive < rules.wave.max_alive) {
        const float z = arrive(w, rules, side, arrived);
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
