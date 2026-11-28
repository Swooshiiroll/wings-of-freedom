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

// Everything the waves have sent goes down at once.
void down_all(World& w) {
    for (std::size_t i = 1; i < w.bodies.size(); ++i) {
        w.bodies[i].damage.going_down = true;
    }
}

// Steps with the player circling, calling the next wave or not; counts events of a kind.
int count_events(World& w, const Rules& r, float seconds, EventKind kind, bool call = false) {
    InputCommand c;
    c.turn = 127;
    c.next_wave = call;
    int n = 0;
    const int ticks = static_cast<int>(std::lround(seconds * kTicksPerSecond));
    for (int i = 0; i < ticks; ++i) {
        step(w, std::vector<InputCommand>{c}, r);
        for (const Event& e : w.events) {
            n += e.kind == kind ? 1 : 0;
        }
    }
    return n;
}

TEST_CASE("The first wave comes at 10 s, and no other while any of it is up") {
    const Rules r = quiet_rules();
    World w = fresh(r);
    run(w, r, 9.9f);
    CHECK(w.pilots.empty());
    CHECK(count_events(w, r, 100.0f, EventKind::WaveArrived) == 1);
    CHECK(w.waves.current == 1);
    CHECK(w.pilots.size() == 1u);
}

TEST_CASE("Once everything is down the wave is cleared, and the next sets out 10 s later") {
    const Rules r = quiet_rules();
    World w = fresh(r);
    run(w, r, 11.0f);
    REQUIRE(w.waves.current == 1);
    down_all(w);
    CHECK_FALSE(wave_up(w));
    InputCommand circle;
    circle.turn = 127;
    step(w, std::vector<InputCommand>{circle}, r);
    REQUIRE(std::count_if(w.events.begin(), w.events.end(), [](const Event& e) {
                return e.kind == EventKind::WaveCleared && e.who == 1;
            }) == 1);
    CHECK(count_events(w, r, 9.8f, EventKind::WaveCleared) == 0); // announced once
    CHECK(w.waves.current == 1);
    CHECK(count_events(w, r, 0.3f, EventKind::WaveArrived) == 1);
    CHECK(w.waves.current == 2);
    CHECK(w.pilots.size() == 3u); // 1, then 2
}

TEST_CASE("A wave up for 3 minutes can be followed by the next, only when a player calls it") {
    const Rules r = quiet_rules();
    World w = fresh(r);
    run(w, r, 11.0f);
    CHECK(count_events(w, r, 170.0f, EventKind::WaveArrived, true) == 0); // too soon to call
    CHECK_FALSE(can_call_wave(w, r));
    run(w, r, 10.0f);
    CHECK(can_call_wave(w, r));
    CHECK(count_events(w, r, 60.0f, EventKind::WaveArrived) == 0); // never forced
    CHECK(count_events(w, r, 1.0f, EventKind::WaveArrived, true) == 1);
    CHECK(w.waves.current == 2);
    CHECK(w.pilots.size() == 3u); // the first wave's fighter is still about
    CHECK_FALSE(can_call_wave(w, r));
}

TEST_CASE("There's nothing to call before the first wave or once all is down") {
    Rules r = quiet_rules();
    r.wave.call_after_s = 1.0f;
    World w = fresh(r);
    run(w, r, 5.0f);
    CHECK_FALSE(can_call_wave(w, r));
    run(w, r, 8.0f);
    CHECK(can_call_wave(w, r));
    down_all(w);
    CHECK_FALSE(can_call_wave(w, r));
}

TEST_CASE("Bombers and escorts hold the next wave until they're down or gone") {
    Rules r = quiet_rules();
    r.wave.raid_every = 3;
    World w = fresh(r, 6); // 4 fighters, 3 bombers, 1 escort
    run(w, r, 10.0f + kTickSeconds);
    const auto set = [&w](Duty duty, Outcome outcome) {
        for (const Pilot& p : w.pilots) {
            if (p.duty == duty) {
                w.bodies[static_cast<std::size_t>(p.body)].outcome = outcome;
            }
        }
    };
    set(Duty::Hunt, Outcome::ShotDown);
    CHECK(wave_up(w));
    set(Duty::Bomb, Outcome::ReturnedHome); // reached your base
    CHECK(wave_up(w));                      // the escort is still about
    set(Duty::Escort, Outcome::ShotDown);
    CHECK_FALSE(wave_up(w));
}

TEST_CASE("Fighters appear about a minute out, from the north, east or west, never the south") {
    const Rules r = quiet_rules();
    const auto side = static_cast<float>(kMapWidthU);
    struct Spot {
        float x;
        float y;
    };
    // The middle, the start by the south edge, the north-east corner and the west edge.
    const std::array<Spot, 4> spots = {Spot{side * 0.5f, side * 0.5f}, Spot{side * 0.5f, 480.0f},
                                       Spot{side - 500.0f, side - 500.0f}, Spot{300.0f, 9000.0f}};
    int north = 0;
    int east = 0;
    int west = 0;
    for (std::size_t s = 0; s < spots.size(); ++s) {
        for (std::uint64_t seed = 1; seed <= 24; ++seed) {
            World w = fresh(r, 5); // three of them, from one side
            w.rng = Rng{seed};
            w.bodies[0].x = spots[s].x;
            w.bodies[0].y = spots[s].y;
            w.bodies[0].z = seed % 2 == 0 ? 150.0f : 400.0f;
            w.bodies[0].speed_u_s = 0.0f;
            w.waves.timer_s = 0.0f; // at once, before the player moves
            run(w, r, kTickSeconds);
            REQUIRE(w.pilots.size() == 3u);
            for (const Pilot& p : w.pilots) {
                const Body& b = w.bodies[static_cast<std::size_t>(p.body)];
                const float dx = b.x - w.bodies[0].x;
                const float dy = b.y - w.bodies[0].y;
                CAPTURE(s);
                CAPTURE(seed);
                CAPTURE(dx);
                CAPTURE(dy);
                CHECK(std::abs(b.z - w.bodies[0].z) <= 65.0f);
                CHECK(b.type == static_cast<std::uint8_t>(PlaneId::He51B));
                CHECK(std::hypot(dx, dy) > 4800.0f);
                CHECK(std::hypot(dx, dy) < 6500.0f);
                CHECK(dy >= -10.0f);
                CHECK(b.x >= 0.0f);
                CHECK(b.x <= side);
                CHECK(b.y <= side);
                if (s == 0) {
                    (std::abs(dx) < dy ? north : dx > 0.0f ? east : west) += 1;
                }
            }
        }
    }
    CHECK(north > 0);
    CHECK(east > 0);
    CHECK(west > 0);
}

TEST_CASE("A wave from a minute out comes and finds you") {
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
    const Rules r = quiet_rules();
    World w = fresh(r, 13); // seven fighters
    run(w, r, 12.0f);
    CHECK(enemies_alive(w) == 6);
    CHECK(w.waves.waiting == 1);
    CHECK(wave_up(w));
    // One goes down: the waiting one takes its place.
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
    CHECK(from_tuning("ai.aim_error_first_deg = 12\nwave.max_alive = 3\nwave.first_s = 2\n"
                      "wave.break_s = 4\nwave.call_after_s = 90\nwave.out_u = 3000\n",
                      t, errors));
    CHECK(t.rules.ai.aim_error_first_deg == doctest::Approx(12.0f));
    CHECK(t.rules.wave.max_alive == 3);
    CHECK(t.rules.wave.first_s == doctest::Approx(2.0f));
    CHECK(t.rules.wave.break_s == doctest::Approx(4.0f));
    CHECK(t.rules.wave.call_after_s == doctest::Approx(90.0f));
    CHECK(t.rules.wave.out_u == doctest::Approx(3000.0f));
    Tuning back;
    CHECK(from_tuning(to_tuning(t), back, errors));
    CHECK(back == t);
}
