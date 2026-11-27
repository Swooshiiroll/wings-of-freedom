#include "core/flak.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "core/map.hpp"
#include "core/score.hpp"
#include "core/tuning.hpp"
#include "core/world.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {

int count(const FlakField& f, FlakKind kind) {
    return static_cast<int>(std::count_if(f.guns.begin(), f.guns.end(), [kind](const FlakGun& g) {
        return g.alive && g.kind == kind;
    }));
}

// A player at height z over (5000, 5000), standing still, and one gun of a kind `away` to its
// west.
World under_a_gun(const Rules& r, Nation nation, FlakKind kind, float z, float away) {
    World w;
    Body b = new_plane(r, PlaneId::P12E, 5000.0f, 5000.0f, 0.0f);
    b.z = z;
    b.speed_u_s = 0.0f;
    w.bodies.push_back(b);
    w.flak.nation = nation;
    w.flak.places.push_back(FlakPlace{});
    FlakGun g;
    g.kind = kind;
    g.x = 5000.0f - away;
    g.y = 5000.0f;
    g.hp = flak_stats(nation, kind).hp;
    w.flak.guns.push_back(g);
    return w;
}

bool fires(World w, const Rules& r) {
    for (int i = 0; i < 4 * kTicksPerSecond; ++i) {
        update_flak(w, r);
        if (!w.rounds.empty() || !w.flak.shells.empty()) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("The front line is halfway up with patchy groups, sites beyond, all machine guns") {
    const FlakParams p;
    Rng rng(7);
    const FlakField f = place_flak(Nation::Germany, p, rng);
    REQUIRE(f.places.size() == static_cast<std::size_t>(p.front_groups + p.sites));
    int strong = 0;
    for (int i = 0; i < p.front_groups; ++i) {
        const FlakPlace& at = f.places[static_cast<std::size_t>(i)];
        CHECK_FALSE(at.site);
        CHECK(std::abs(at.y - static_cast<float>(kFrontU)) <= 100.0f);
        const float stretch = static_cast<float>(kMapWidthU) / static_cast<float>(p.front_groups);
        CHECK(at.x >= static_cast<float>(i) * stretch);
        CHECK(at.x < static_cast<float>(i + 1) * stretch);
        strong += at.strong ? 1 : 0;
    }
    CHECK(strong == p.strongpoints);
    for (int i = p.front_groups; i < p.front_groups + p.sites; ++i) {
        const FlakPlace& at = f.places[static_cast<std::size_t>(i)];
        CHECK(at.site);
        CHECK(at.y >= static_cast<float>(kFrontU) + p.site_near_u);
        CHECK(at.y < static_cast<float>(kMapDepthU));
    }
    CHECK(count(f, FlakKind::MachineGun) ==
          p.front_groups * p.guns_per_group + p.sites * p.guns_per_site);
    CHECK(count(f, FlakKind::Cannon) + count(f, FlakKind::Heavy) == 0);
    Rng again(7);
    CHECK(place_flak(Nation::Germany, p, again).guns.front().x == f.guns.front().x);
}

TEST_CASE("Cannon replace machine guns at wave 5, heavy guns arrive at 9, wrecks stay wrecks") {
    const Rules r;
    World w;
    w.bodies.push_back(new_plane(r, PlaneId::P12E, 1000.0f, 1000.0f, 0.0f));
    Rng rng(3);
    w.flak = place_flak(Nation::US, r.flak, rng);
    w.flak.guns[0].alive = false;
    const int start = count(w.flak, FlakKind::MachineGun);
    w.waves.current = 4; // waves 2 and 4 each add a gun
    update_flak(w, r);
    CHECK(count(w.flak, FlakKind::MachineGun) == start + 2);
    w.waves.current = 5;
    update_flak(w, r);
    CHECK(count(w.flak, FlakKind::MachineGun) == 0);
    CHECK(count(w.flak, FlakKind::Cannon) == start + 2);
    CHECK_FALSE(w.flak.guns[0].alive);
    w.waves.current = 9;
    update_flak(w, r);
    CHECK(count(w.flak, FlakKind::Heavy) == r.flak.sites + r.flak.strongpoints);
    w.waves.current = 40;
    update_flak(w, r);
    CHECK(w.flak.reinforced == r.flak.reinforce_max);
}

TEST_CASE("A gun fires at you only within its range and reach") {
    const Rules r;
    CHECK(fires(under_a_gun(r, Nation::Germany, FlakKind::MachineGun, 40.0f, 100.0f), r));
    CHECK_FALSE(fires(under_a_gun(r, Nation::Germany, FlakKind::MachineGun, 40.0f, 200.0f), r));
    CHECK_FALSE(fires(under_a_gun(r, Nation::Germany, FlakKind::MachineGun, 80.0f, 100.0f), r));
    CHECK_FALSE(fires(under_a_gun(r, Nation::Germany, FlakKind::Cannon, 100.0f, 100.0f), r));
    CHECK(fires(under_a_gun(r, Nation::US, FlakKind::Cannon, 100.0f, 100.0f), r));
    CHECK(fires(under_a_gun(r, Nation::Germany, FlakKind::Heavy, 450.0f, 300.0f), r));
    CHECK_FALSE(fires(under_a_gun(r, Nation::Germany, FlakKind::Heavy, 550.0f, 100.0f), r));
}

TEST_CASE("Guns ignore their own planes and hold fire rather than shoot through them") {
    const Rules r;
    World w = under_a_gun(r, Nation::Germany, FlakKind::MachineGun, 40.0f, 160.0f);
    Body friendly = new_plane(r, PlaneId::Ar65E, 4920.0f, 5000.0f, 0.0f);
    friendly.z = 20.0f; // between gun and player
    friendly.speed_u_s = 0.0f;
    w.bodies.push_back(friendly);
    w.pilots.push_back(new_pilot(1, Style::Turn, Skill{}, w.rng));
    CHECK_FALSE(fires(w, r));
    w.bodies[0].outcome = Outcome::ShotDown; // no player left
    CHECK_FALSE(fires(w, r));
}

TEST_CASE("A machine gun's rounds hit you, and count as no one's kill") {
    const Rules r;
    World w = under_a_gun(r, Nation::US, FlakKind::MachineGun, 30.0f, 60.0f);
    int hits = 0;
    for (int i = 0; i < 3 * kTicksPerSecond; ++i) {
        w.events.clear();
        update_flak(w, r);
        move_projectiles(w, r);
        for (const Event& e : w.events) {
            if (e.kind == EventKind::RoundHitPlane) {
                CHECK(e.by < 0);
                ++hits;
            }
        }
    }
    CHECK(hits > 3);
}

TEST_CASE("Heavy bursts scatter more the higher you are") {
    const Rules r;
    const auto spread = [&r](float z) {
        World w = under_a_gun(r, Nation::Germany, FlakKind::Heavy, z, 100.0f);
        float sum = 0.0f;
        int n = 0;
        for (int i = 0; i < 200; ++i) {
            w.flak.guns[0].wait_s = 0.0f;
            update_flak(w, r);
            for (const FlakShell& s : w.flak.shells) {
                sum += std::sqrt((s.x - 5000.0f) * (s.x - 5000.0f) +
                                 (s.y - 5000.0f) * (s.y - 5000.0f) + (s.z - z) * (s.z - z));
                ++n;
            }
            w.flak.shells.clear();
        }
        REQUIRE(n > 100);
        return sum / static_cast<float>(n);
    };
    CHECK(spread(400.0f) > 2.0f * spread(100.0f));
}

TEST_CASE("Strafing and bombs destroy guns; yours score, with chains") {
    const Rules r;
    World w = under_a_gun(r, Nation::Germany, FlakKind::MachineGun, 100.0f, 0.0f);
    Round round;
    round.x = 5000.0f;
    round.y = 5000.0f;
    round.z = 1.0f;
    round.vz = -300.0f;
    round.life_s = 1.0f;
    round.damage = 25.0f;
    round.owner = -2; // flak's own never harms guns
    w.rounds.push_back(round);
    move_projectiles(w, r);
    CHECK(w.flak.guns[0].alive);
    round.owner = 0;
    w.rounds.push_back(round);
    move_projectiles(w, r);
    CHECK_FALSE(w.flak.guns[0].alive); // 25 damage on its 20 HP
    CHECK(w.flak.destroyed == 1);
}

TEST_CASE("A destroyed gun is counted once, and worth 50 light or 150 heavy") {
    const Rules r;
    World w = under_a_gun(r, Nation::Germany, FlakKind::Heavy, 100.0f, 100.0f);
    w.events.clear();
    hit_flak(w, 0, 100.0f, 0);
    hit_flak(w, 0, 100.0f, 0);
    CHECK_FALSE(w.flak.guns[0].alive);
    CHECK(w.flak.destroyed == 1);
    RunScore s;
    score_tick(s, w, r.score, kTickSeconds);
    CHECK(s.points == 150);
    CHECK(s.guns == 1);
    w.events = {Event{EventKind::FlakDown, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f}};
    score_tick(s, w, r.score, kTickSeconds);
    CHECK(s.points == 150 + 75); // 50 x1.5 in the chain
}

TEST_CASE("Bombs destroy guns too") {
    const Rules r;
    World w = under_a_gun(r, Nation::Germany, FlakKind::Cannon, 100.0f, 0.0f);
    Bomb bomb;
    bomb.x = 5010.0f;
    bomb.y = 5000.0f;
    bomb.z = 1.0f;
    bomb.vz = -100.0f;
    bomb.owner = 0;
    bomb.radius_u = 20.0f;
    bomb.damage = 40.0f;
    w.bombs.push_back(bomb);
    move_projectiles(w, r);
    CHECK_FALSE(w.flak.guns[0].alive);
}

TEST_CASE("Flak is in the tuning file") {
    Tuning t;
    std::string errors;
    CHECK(from_tuning("flak.front_groups = 10\nflak.heavy_wave = 12\nscore.flak_heavy = 200\n"
                      "ground.front_depth_u = 300\n",
                      t, errors));
    CHECK(t.rules.flak.front_groups == 10);
    CHECK(t.rules.flak.heavy_wave == 12);
    CHECK(t.rules.score.flak_heavy == 200);
    CHECK(t.ground.front_depth_u == 300);
}
