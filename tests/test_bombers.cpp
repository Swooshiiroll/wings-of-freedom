#include "core/gunners.hpp"
#include "core/waves.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "core/map.hpp"
#include "core/tuning.hpp"
#include "core/world.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {

// Fighters that never fire and nothing that rams, so a circling player survives.
Rules quiet_rules() {
    Rules r;
    r.ai.fire_range_u = 0.0f;
    r.damage.collision_radius_u = 0.0f;
    r.wave.first_s = 0.0f;
    r.rederive();
    return r;
}

// A US player far to the west, circling, and a wave `n` of Germany's.
World raid_world(const Rules& r, int n) {
    World w;
    w.bodies.push_back(new_plane(r, PlaneId::P12E, 3000.0f, 8000.0f, 0.0f));
    w.waves = start_waves(r, fighter_tree(Nation::Germany), n);
    return w;
}

void run(World& w, const Rules& r, float seconds, int turn = 127) {
    InputCommand c;
    c.turn = static_cast<std::int8_t>(turn);
    const int ticks = static_cast<int>(std::lround(seconds * kTicksPerSecond));
    for (int i = 0; i < ticks; ++i) {
        step(w, std::vector<InputCommand>{c}, r);
    }
}

std::vector<const Pilot*> with_duty(const World& w, Duty duty) {
    std::vector<const Pilot*> out;
    for (const Pilot& p : w.pilots) {
        if (p.duty == duty) {
            out.push_back(&p);
        }
    }
    return out;
}

const Body& body_of(const World& w, const Pilot& p) {
    return w.bodies[static_cast<std::size_t>(p.body)];
}

// A bomber heading north with a player beside it at (dx, dy, dz), both at cruise.
World gunnery_world(const Rules& r, float dx, float dy, float dz) {
    World w;
    Body player = new_plane(r, PlaneId::P26A, 5000.0f + dx, 5000.0f + dy, 0.0f);
    player.z = 200.0f + dz;
    player.speed_u_s = 84.0f;
    Body bomber = new_plane(r, PlaneId::He111B2, 5000.0f, 5000.0f, 0.0f);
    bomber.z = 200.0f;
    bomber.speed_u_s = 84.0f;
    w.bodies = {player, bomber};
    Pilot pilot = new_pilot(1, Style::Turn, Skill{0.0f, 0.15f}, w.rng);
    pilot.duty = Duty::Bomb;
    pilot.course_deg = 0.0f;
    pilot.height_u = 200.0f;
    pilot.cruise_u_s = 84.0f;
    w.pilots = {pilot};
    return w;
}

std::array<int, 3> fired(const World& w) {
    const auto& g = w.bodies[1].loadout.guns;
    return {g[0].fired, g[1].fired, g[2].fired};
}

} // namespace

TEST_CASE("Raids come every third wave: a pair, then three, then five, escorted from the second") {
    CHECK(raid_size(1, 3) == 0);
    CHECK(raid_size(3, 3) == 2);
    CHECK(raid_size(6, 3) == 3);
    CHECK(raid_size(9, 3) == 5);
    CHECK(raid_size(15, 3) == 5);
    CHECK(raid_escorts(3, 3) == 0);
    CHECK(raid_escorts(6, 3) == 1);
    CHECK(raid_escorts(9, 3) == 2);
    CHECK(raid_size(6, 0) == 0);
}

TEST_CASE("The first raid is the early bombers, in from the north and heading for your base") {
    const Rules r = quiet_rules();
    World w = raid_world(r, 3);
    run(w, r, kTickSeconds);
    const auto bombers = with_duty(w, Duty::Bomb);
    REQUIRE(bombers.size() == 2u);
    CHECK(with_duty(w, Duty::Escort).empty());
    for (const Pilot* p : bombers) {
        const Body& b = body_of(w, *p);
        CHECK(b.type == static_cast<std::uint8_t>(PlaneId::Do23G));
        CHECK(b.y > static_cast<float>(kMapDepthU) - r.wave.edge_inset_u - 100.0f);
        CHECK(b.z >= 160.0f);
        CHECK(b.z <= 240.0f); // the Do 23's 12,000 ft
        // On course for the middle of the south edge.
        const float to_base =
            std::atan2(static_cast<float>(kMapWidthU) * 0.5f - b.x, -b.y) * 57.2958f;
        CHECK(std::abs(std::remainder(p->course_deg - to_base, 360.0f)) < 3.0f);
    }
    CHECK(w.waves.raids == 1);
}

TEST_CASE("From wave 9 the later bombers come, with escorts of the wave's fighter types") {
    const Rules r = quiet_rules();
    World w = raid_world(r, 9);
    run(w, r, kTickSeconds);
    const auto bombers = with_duty(w, Duty::Bomb);
    const auto escorts = with_duty(w, Duty::Escort);
    REQUIRE(bombers.size() == 5u);
    REQUIRE(escorts.size() == 2u);
    for (const Pilot* p : bombers) {
        CHECK(body_of(w, *p).type == static_cast<std::uint8_t>(PlaneId::He111B2));
    }
    for (const Pilot* p : escorts) {
        const auto type = static_cast<PlaneId>(body_of(w, *p).type);
        CHECK((type == PlaneId::He51B || type == PlaneId::Bf109B));
        // The formation goes no faster than its escorts can.
        CHECK(p->cruise_u_s <= plane_of(r, body_of(w, *p)).flight.max_level_u_s * 0.9f + 0.1f);
    }
    // Neither counts toward the six wave fighters.
    CHECK(enemies_alive(w) == static_cast<int>(with_duty(w, Duty::Hunt).size()));
}

TEST_CASE("Bombers hold their course to your base, and each one through is counted") {
    const Rules r = quiet_rules();
    World w = raid_world(r, 3);
    int reached = 0;
    InputCommand circle;
    circle.turn = 127;
    for (int i = 0; i < 480 * kTicksPerSecond && w.waves.through < 2; ++i) {
        step(w, std::vector<InputCommand>{circle}, r);
        for (const Event& e : w.events) {
            reached += e.kind == EventKind::BomberThrough ? 1 : 0;
        }
    }
    CHECK(w.waves.through == 2);
    CHECK(reached == 2);
    for (const Pilot* p : with_duty(w, Duty::Bomb)) {
        const Body& b = body_of(w, *p);
        CHECK(b.outcome == Outcome::ReturnedHome);
        CHECK(std::abs(b.x - static_cast<float>(kMapWidthU) * 0.5f) < 400.0f);
    }
}

TEST_CASE("Escorts stay by their bombers until you come near, and go back when you leave") {
    const Rules r = quiet_rules();
    World w = raid_world(r, 6);
    run(w, r, 20.0f);
    const auto escorts = with_duty(w, Duty::Escort);
    REQUIRE(escorts.size() == 1u);
    const std::size_t escort = static_cast<std::size_t>(escorts[0]->body);
    const auto formation = [&w] {
        float x = 0.0f;
        float y = 0.0f;
        int n = 0;
        for (const Pilot* p : with_duty(w, Duty::Bomb)) {
            x += body_of(w, *p).x;
            y += body_of(w, *p).y;
            ++n;
        }
        return std::pair{x / static_cast<float>(n), y / static_cast<float>(n)};
    };
    auto [fx, fy] = formation();
    CHECK(std::hypot(w.bodies[escort].x - fx, w.bodies[escort].y - fy) < 400.0f);
    CHECK_FALSE(escorts[0]->engaged);
    w.bodies[0].x = fx + 300.0f; // close by the bombers
    w.bodies[0].y = fy;
    run(w, r, 0.5f);
    CHECK(with_duty(w, Duty::Escort)[0]->engaged);
    std::tie(fx, fy) = formation();
    w.bodies[0].x = fx + 2000.0f; // away again
    run(w, r, 0.5f);
    CHECK_FALSE(with_duty(w, Duty::Escort)[0]->engaged);
}

TEST_CASE("Each gunner has its arc: nose ahead, dorsal behind and above, ventral behind below") {
    const GunnerParams p;
    // Heading north; targets at 100 u.
    CHECK(in_gunner_arc(0, 0.0f, 0.0f, 100.0f, 0.0f, p));
    CHECK_FALSE(in_gunner_arc(1, 0.0f, 0.0f, 100.0f, 0.0f, p));
    CHECK(in_gunner_arc(1, 0.0f, 0.0f, -100.0f, 20.0f, p));
    CHECK_FALSE(in_gunner_arc(2, 0.0f, 0.0f, -100.0f, 20.0f, p));
    CHECK(in_gunner_arc(2, 0.0f, 0.0f, -100.0f, -20.0f, p));
    CHECK_FALSE(in_gunner_arc(1, 0.0f, 0.0f, -100.0f, -20.0f, p));
    CHECK(in_gunner_arc(1, 0.0f, 100.0f, 0.0f, 20.0f, p));        // abeam: within the dorsal's 120
    CHECK_FALSE(in_gunner_arc(2, 0.0f, 100.0f, 0.0f, -20.0f, p)); // not the ventral's 60
    CHECK_FALSE(in_gunner_arc(0, 0.0f, 0.0f, 200.0f, 0.0f, p));   // out of range
}

TEST_CASE("Gunners fire at you in their arcs, in short bursts, until knocked out") {
    const Rules r;
    {
        World w = gunnery_world(r, 0.0f, -80.0f, 30.0f); // behind and above
        run(w, r, 6.0f, 0);
        const auto f = fired(w);
        CHECK(f[0] == 0);
        CHECK(f[2] == 0);
        CHECK(f[1] > 30);
        CHECK(f[1] < 70); // about half of 100: bursts
    }
    {
        World w = gunnery_world(r, 0.0f, 80.0f, 0.0f); // ahead
        run(w, r, 1.0f, 0);
        CHECK(fired(w)[0] > 0);
        CHECK(fired(w)[1] == 0);
    }
    {
        World w = gunnery_world(r, 0.0f, -80.0f, 30.0f);
        w.bodies[1].damage.gunner[1] = 15.0f; // three wounds
        run(w, r, 2.0f, 0);
        CHECK(fired(w)[1] == 0);
    }
}

TEST_CASE("A gunner takes three wounds, and its share of hits goes back once it's out") {
    const Rules r;
    Body b = new_plane(r, PlaneId::He111B2, 0.0f, 0.0f, 0.0f);
    const DamageParams& d = plane_of(r, b).damage;
    REQUIRE(d.gunners == 3);
    Rng rng(3);
    std::array<int, kParts> behind{};
    for (int i = 0; i < 20000; ++i) {
        ++behind[static_cast<std::size_t>(pick_part(b, HitFrom::Behind, d, rng))];
    }
    const auto share = [&behind](Part p) {
        return static_cast<float>(behind[static_cast<std::size_t>(p)]) / 200.0f;
    };
    CHECK(share(Part::DorsalGunner) == doctest::Approx(8.0f).epsilon(0.1));
    CHECK(share(Part::VentralGunner) == doctest::Approx(8.0f).epsilon(0.1));
    CHECK(share(Part::Fuselage) == doctest::Approx(34.0f).epsilon(0.05));
    for (int i = 0; i < 3; ++i) {
        CHECK_FALSE(gunner_out(b, 1, d));
        apply_hit(b, Part::DorsalGunner, 5.0f, d);
    }
    CHECK(gunner_out(b, 1, d));
    CHECK(part_stage(b, Part::DorsalGunner, d) == Stage::Destroyed);
    for (int i = 0; i < 2000; ++i) {
        CHECK(pick_part(b, HitFrom::Behind, d, rng) != Part::DorsalGunner);
    }
}

TEST_CASE("A bomber is a bigger mark: rounds hit it where they'd miss a fighter") {
    const Rules r;
    for (const PlaneId id : {PlaneId::P26A, PlaneId::He111B2}) {
        World w;
        w.bodies.push_back(new_plane(r, PlaneId::P12E, 100.0f, 100.0f, 0.0f));
        Body target = new_plane(r, id, 100.0f + 12.0f, 200.0f, 0.0f);
        target.z = w.bodies[0].z;
        target.speed_u_s = 0.0f;
        w.bodies.push_back(target);
        Round round;
        round.x = 100.0f;
        round.y = 195.0f;
        round.z = target.z;
        round.vy = 600.0f;
        round.life_s = 1.0f;
        round.owner = 0;
        w.rounds.push_back(round);
        move_projectiles(w, r);
        const bool hit = std::any_of(w.events.begin(), w.events.end(), [](const Event& e) {
            return e.kind == EventKind::RoundHitPlane;
        });
        CHECK(hit == (id == PlaneId::He111B2));
    }
}

TEST_CASE("Raids, gunners and escorts are in the tuning file") {
    Tuning t;
    std::string errors;
    CHECK(from_tuning("wave.raid_every = 4\ngunner.range_u = 200\nai.escort_break_u = 700\n"
                      "damage.nose_gunner_pct = 12\n",
                      t, errors));
    CHECK(t.rules.wave.raid_every == 4);
    CHECK(t.rules.gunner.range_u == doctest::Approx(200.0f));
    CHECK(t.rules.ai.escort_break_u == doctest::Approx(700.0f));
    CHECK(t.rules.damage.nose_gunner_pct == 12);
}
