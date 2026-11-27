#include "core/waves.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "core/map.hpp"
#include "core/tuning.hpp"
#include "core/world.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {

// Pilots that never fire and planes that never ram, so a player circling survives any number of
// waves.
Rules quiet_rules() {
    Rules r;
    r.ai.fire_range_u = 0.0f;
    r.damage.collision_radius_u = 0.0f;
    r.wave.raid_every = 0; // fighters only: bombers have their own tests
    r.rederive();
    return r;
}

World fresh(const Rules& r, int first = 1) {
    World w;
    Body b = new_plane(r, PlaneId::P26A, 1440.0f, 1500.0f, 0.0f);
    w.bodies.push_back(b);
    // He 51s at every step, unless a test says otherwise.
    w.waves = start_waves(r, {PlaneId::He51B, PlaneId::He51B, PlaneId::He51B}, first);
    return w;
}

void run(World& w, const Rules& r, float seconds) {
    InputCommand circle;
    circle.turn = 127;
    const int ticks = static_cast<int>(std::lround(seconds * kTicksPerSecond));
    for (int i = 0; i < ticks; ++i) {
        step(w, std::vector<InputCommand>{circle}, r);
    }
}

} // namespace

TEST_CASE("Wave n brings 1 + n/2 fighters") {
    CHECK(wave_size(1) == 1);
    CHECK(wave_size(2) == 2);
    CHECK(wave_size(3) == 2);
    CHECK(wave_size(4) == 3);
    CHECK(wave_size(9) == 5);
}

TEST_CASE("Enemy fighters move up their tree every four waves, mixing as they go") {
    const auto span = [](int n) {
        const TierSpan s = enemy_steps(n, 4);
        return std::pair{s.lo, s.hi};
    };
    CHECK(span(1) == std::pair{0, 0});
    CHECK(span(4) == std::pair{0, 0});
    CHECK(span(5) == std::pair{0, 1});
    CHECK(span(8) == std::pair{0, 1});
    CHECK(span(9) == std::pair{1, 2});
    CHECK(span(12) == std::pair{1, 2});
    CHECK(span(13) == std::pair{2, 2});
    CHECK(span(40) == std::pair{2, 2});
}

TEST_CASE("The waves send the enemy's tree: first types early, both in a mix, the last later") {
    const Rules r = quiet_rules();
    const FighterTree german = fighter_tree(Nation::Germany);
    const auto types_at = [&](int first) {
        std::array<int, 3> seen{};
        for (std::uint64_t seed = 1; seed <= 20; ++seed) {
            World w = fresh(r, first);
            w.rng = Rng{seed};
            w.waves = start_waves(r, german, first);
            run(w, r, 10.0f + kTickSeconds);
            for (const Pilot& p : w.pilots) {
                const auto type =
                    static_cast<PlaneId>(w.bodies[static_cast<std::size_t>(p.body)].type);
                for (std::size_t step = 0; step < german.size(); ++step) {
                    seen[step] += type == german[step] ? 1 : 0;
                }
            }
        }
        return seen;
    };
    const auto early = types_at(2);
    CHECK(early[0] > 0);
    CHECK(early[1] + early[2] == 0);
    const auto mixed = types_at(6);
    CHECK(mixed[0] > 0);
    CHECK(mixed[1] > 0);
    CHECK(mixed[2] == 0);
    const auto late = types_at(14);
    CHECK(late[0] + late[1] == 0);
    CHECK(late[2] > 0);
}

TEST_CASE("The first wave comes at 10 s, then one every 40 s, each announced") {
    const Rules r = quiet_rules();
    World w = fresh(r);
    run(w, r, 9.9f);
    CHECK(w.pilots.empty());
    int announced = 0;
    InputCommand circle;
    circle.turn = 127;
    for (int i = 0; i < 82 * kTicksPerSecond; ++i) {
        step(w, std::vector<InputCommand>{circle}, r);
        for (const Event& e : w.events) {
            if (e.kind == EventKind::WaveArrived) {
                ++announced;
                CHECK(e.who == announced);
                CHECK(static_cast<int>(e.damage) == wave_size(announced));
            }
        }
    }
    CHECK(announced == 3);        // 10, 50 and 90 s
    CHECK(w.pilots.size() == 5u); // 1 + 2 + 2
    CHECK(w.waves.current == 3);
}

TEST_CASE("Enemies fly in from the north, east or west edge, never from behind you to the south") {
    const Rules r = quiet_rules();
    const auto side = static_cast<float>(kMapWidthU);
    int north = 0;
    int east = 0;
    int west = 0;
    for (std::uint64_t seed = 1; seed <= 24; ++seed) {
        World w = fresh(r, 5); // three of them, from one edge
        w.rng = Rng{seed};
        w.bodies[0].x = side * 0.5f;
        w.bodies[0].z = seed % 2 == 0 ? 150.0f : 400.0f;
        run(w, r, 10.0f + kTickSeconds);
        REQUIRE(w.pilots.size() == 3u);
        for (const Pilot& p : w.pilots) {
            const Body& b = w.bodies[static_cast<std::size_t>(p.body)];
            CAPTURE(seed);
            CAPTURE(b.x);
            CAPTURE(b.y);
            CHECK(std::abs(b.z - w.bodies[0].z) <= 65.0f);
            CHECK(b.type == static_cast<std::uint8_t>(PlaneId::He51B));
            CHECK(b.y >= w.bodies[0].y - 10.0f);
            const bool at_north = b.y > side - r.wave.edge_inset_u - 10.0f;
            const bool at_east = b.x > side - r.wave.edge_inset_u - 10.0f;
            const bool at_west = b.x < r.wave.edge_inset_u + 10.0f;
            CHECK((at_north || at_east || at_west));
            north += at_north ? 1 : 0;
            east += at_east ? 1 : 0;
            west += at_west ? 1 : 0;
        }
    }
    CHECK(north > 0);
    CHECK(east > 0);
    CHECK(west > 0);
}

TEST_CASE("A wave from a far edge comes and finds you") {
    const Rules r = quiet_rules();
    World w = fresh(r);
    w.bodies[0].x = static_cast<float>(kMapWidthU) * 0.5f;
    float closest = 1.0e9f;
    InputCommand circle;
    circle.turn = 127;
    for (int i = 0; i < 300 * kTicksPerSecond; ++i) {
        step(w, std::vector<InputCommand>{circle}, r);
        for (const Pilot& p : w.pilots) {
            const Body& b = w.bodies[static_cast<std::size_t>(p.body)];
            closest = std::min(closest, std::hypot(b.x - w.bodies[0].x, b.y - w.bodies[0].y));
        }
    }
    CHECK(closest < 300.0f);
}

TEST_CASE("No more than six are in the air; the rest wait for a place") {
    Rules r = quiet_rules();
    r.wave.every_s = 5.0f;
    World w = fresh(r);
    run(w, r, 40.0f); // seven waves: 18 fighters in all
    CHECK(enemies_alive(w) == 6);
    CHECK(w.waves.waiting > 0);
    // One goes down: a waiting one takes its place.
    w.bodies[1].damage.going_down = true;
    const std::size_t before = w.pilots.size();
    run(w, r, kTickSeconds);
    CHECK(w.pilots.size() == before + 1);
    CHECK(enemies_alive(w) == 6);
}

TEST_CASE("No waves when they're off, and none after the player is down") {
    const Rules r = quiet_rules();
    World off = fresh(r);
    off.waves.enabled = false;
    run(off, r, 20.0f);
    CHECK(off.pilots.empty());
    World down = fresh(r);
    down.bodies[0].outcome = Outcome::Crashed;
    run(down, r, 20.0f);
    CHECK(down.pilots.empty());
}

TEST_CASE("The same seed gives the same fight") {
    const Rules r;
    World a = fresh(r, 4);
    World b = fresh(r, 4);
    run(a, r, 30.0f);
    run(b, r, 30.0f);
    REQUIRE(a.bodies.size() == b.bodies.size());
    CHECK(a.bodies == b.bodies);
    CHECK(a.waves == b.waves);
}

TEST_CASE("The ai and wave numbers are in the tuning file") {
    Tuning t;
    std::string errors;
    CHECK(from_tuning("ai.aim_error_first_deg = 12\nwave.max_alive = 3\nwave.first_s = 2\n", t,
                      errors));
    CHECK(t.rules.ai.aim_error_first_deg == doctest::Approx(12.0f));
    CHECK(t.rules.wave.max_alive == 3);
    CHECK(t.rules.wave.first_s == doctest::Approx(2.0f));
    Tuning back;
    CHECK(from_tuning(to_tuning(t), back, errors));
    CHECK(back == t);
}
