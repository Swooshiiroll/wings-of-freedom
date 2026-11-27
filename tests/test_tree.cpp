#include "core/hangar.hpp"
#include "core/progress.hpp"
#include "core/score.hpp"
#include "core/upgrades.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include "core/tuning.hpp"
#include "core/world.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {

// A world with the player and one enemy, to feed score_tick events.
World two_planes(const Rules& r) {
    World w;
    w.bodies.push_back(new_plane(r, PlaneId::P12E, 1000.0f, 1000.0f, 0.0f));
    w.bodies.push_back(new_plane(r, PlaneId::Ar65E, 1000.0f, 3000.0f, 180.0f));
    return w;
}

Event down(int who) {
    return Event{EventKind::EnemyDown, who, 0, 0.0f, 0.0f, 0.0f, 0.0f};
}

} // namespace

TEST_CASE("A chain raises the multiplier by a half each kill, up to x4") {
    const ScoreParams p;
    CHECK(chain_multiplier(1, p) == doctest::Approx(1.0f));
    CHECK(chain_multiplier(2, p) == doctest::Approx(1.5f));
    CHECK(chain_multiplier(3, p) == doctest::Approx(2.0f));
    CHECK(chain_multiplier(7, p) == doctest::Approx(4.0f));
    CHECK(chain_multiplier(20, p) == doctest::Approx(4.0f));
}

TEST_CASE("Fighters score 100 and bombers 250; kills within 20 s chain") {
    const Rules r;
    World w = two_planes(r);
    RunScore s;
    w.events = {down(1)};
    score_tick(s, w, r.score, kTickSeconds);
    CHECK(s.points == 100);
    CHECK(s.fighters == 1);
    w.events.clear();
    for (int i = 0; i < 19 * kTicksPerSecond; ++i) {
        score_tick(s, w, r.score, kTickSeconds);
    }
    CHECK(s.chain == 1); // still going
    w.events = {down(1)};
    score_tick(s, w, r.score, kTickSeconds);
    CHECK(s.points == 250); // 100 + 150
    w.events.clear();
    for (int i = 0; i < 21 * kTicksPerSecond; ++i) {
        score_tick(s, w, r.score, kTickSeconds);
    }
    CHECK(s.chain == 0); // gone
    score_kill(s, Role::Bomber, r.score);
    CHECK(s.points == 500);
    CHECK(s.bombers == 1);
}

TEST_CASE("Waves count as they arrive") {
    const Rules r;
    World w = two_planes(r);
    RunScore s;
    w.events = {Event{EventKind::WaveArrived, 1, -1, 0.0f, 0.0f, 0.0f, 1.0f}};
    score_tick(s, w, r.score, kTickSeconds);
    score_tick(s, w, r.score, kTickSeconds);
    CHECK(s.waves == 2);
}

TEST_CASE("Points are banked only at home, with the condition and wave bonuses") {
    const Rules r;
    Body b = new_plane(r, PlaneId::P12E, 0.0f, 0.0f, 180.0f);
    const DamageParams& d = plane_of(r, b).damage;
    CHECK(condition_of(b, d) == doctest::Approx(1.0f));
    RunScore s;
    s.points = 1000;
    s.waves = 6;
    b.outcome = Outcome::ReturnedHome;
    const Payout home = payout(s, b, d, r.score);
    CHECK(home.banked);
    CHECK(home.total == 2100); // 1000 x (1 + 0.5 + 0.6)
    b.outcome = Outcome::ShotDown;
    CHECK(payout(s, b, d, r.score).total == 0);
    b.outcome = Outcome::ReturnedHome;
    b.damage.wounds = 1; // a third off
    CHECK(condition_of(b, d) == doctest::Approx(2.0f / 3.0f));
    b.damage.wounds = 0;
    b.damage.fuselage = 0.0f; // 60 of 200 HP gone
    CHECK(condition_of(b, d) == doctest::Approx(140.0f / 200.0f));
}

TEST_CASE("A plane falling over the south edge isn't home") {
    const Rules r;
    Body b = new_plane(r, PlaneId::P12E, 100.0f, 10.0f, 180.0f);
    b.z = 100.0f;
    b.damage.going_down = true;
    b.y = -5.0f;
    apply_edges(b, r.edge, kTickSeconds);
    CHECK(b.outcome == Outcome::Flying);
    b.damage.going_down = false;
    apply_edges(b, r.edge, kTickSeconds);
    CHECK(b.outcome == Outcome::ReturnedHome);
    CHECK(got_home(b));
}

TEST_CASE("Each upgrade changes what it says, on every fighter") {
    const Rules r;
    for (const Nation nation : {Nation::US, Nation::Germany}) {
        for (const PlaneId id : fighter_tree(nation)) {
            CAPTURE(std::string(plane_type(id).id));
            const auto fit = [&](UpgradeSet set) {
                return fit_plane(id, set, r.flight, r.damage, r.scale, r.upgrade);
            };
            const PlaneRules stock = fit(0);
            CHECK(stock.flight == r.planes[static_cast<std::size_t>(id)].flight);

            const PlaneRules guns = fit(upgrade_bit(Upgrade::Guns));
            const bool new_fifty =
                guns.guns[1].calibre == Calibre::Heavy && stock.guns[1].calibre == Calibre::Rifle;
            const bool better_gun = guns.gun_count > stock.gun_count || new_fifty;
            CHECK(better_gun);

            const PlaneRules engine = fit(upgrade_bit(Upgrade::Engine));
            CHECK(engine.flight.max_level_u_s ==
                  doctest::Approx(stock.flight.max_level_u_s * 1.06f));
            CHECK(best_climb(engine.flight) ==
                  doctest::Approx(best_climb(stock.flight) * 1.10f).epsilon(0.03));

            const PlaneRules fuel = fit(upgrade_bit(Upgrade::Fuel));
            CHECK(fuel.flight.tank_s == doctest::Approx(stock.flight.tank_s * 1.45f));

            const PlaneRules armour = fit(upgrade_bit(Upgrade::Armour));
            CHECK(armour.damage.wound_damage == doctest::Approx(stock.damage.wound_damage * 2.0f));
            CHECK(armour.damage.leak_tank_s ==
                  doctest::Approx(stock.damage.leak_tank_s * (id == PlaneId::P36A ? 2.0f : 1.0f)));

            const PlaneRules bombs = fit(upgrade_bit(Upgrade::Bombs));
            CHECK(static_cast<float>(bombs.bombs) * bombs.bomb_lb >
                  static_cast<float>(stock.bombs) * stock.bomb_lb);

            const PlaneRules sight = fit(upgrade_bit(Upgrade::Sight));
            CHECK(sight.flight == stock.flight);
            CHECK(std::string(upgrade_info(id, Upgrade::Sight).label) == "BOMB SIGHT");
            for (int u = 0; u < kUpgrades; ++u) {
                CHECK(std::string(upgrade_info(id, static_cast<Upgrade>(u)).label).size() > 0u);
            }
        }
    }
}

TEST_CASE("A body with a fit flies the upgraded plane") {
    Rules r;
    const int fit = r.add_fit({PlaneId::P26A, upgrade_bit(Upgrade::Guns)});
    const Body mine = new_plane(r, PlaneId::P26A, 0.0f, 0.0f, 0.0f, fit);
    const Body stock = new_plane(r, PlaneId::P26A, 0.0f, 0.0f, 0.0f);
    CHECK(mine.loadout.guns[1].rounds == 200); // the .50
    CHECK(stock.loadout.guns[1].rounds == 500);
    CHECK(plane_of(r, mine).guns[1].calibre == Calibre::Heavy);
    CHECK(plane_of(r, stock).guns[1].calibre == Calibre::Rifle);
    r.rederive(); // a tuning change keeps the fit
    CHECK(plane_of(r, mine).guns[1].calibre == Calibre::Heavy);
}

TEST_CASE("Planes are bought in order, upgrades only for planes you own") {
    const Prices p;
    NationProgress n;
    CHECK(n.planes[0].owned);
    CHECK(plane_state(n, 1, p) == BuyState::TooDear);
    n.points = 20000;
    CHECK(plane_state(n, 2, p) == BuyState::Locked); // step 2 first
    CHECK(upgrade_state(n, 1, Upgrade::Guns, p) == BuyState::Locked);
    CHECK(buy_plane(n, 1, p));
    CHECK(n.points == 17000);
    CHECK_FALSE(buy_plane(n, 1, p)); // once
    CHECK(buy_plane(n, 2, p));
    CHECK(n.points == 9000);
    CHECK(buy_upgrade(n, 1, Upgrade::Sight, p));
    CHECK(n.points == 8700);
    CHECK_FALSE(buy_upgrade(n, 1, Upgrade::Sight, p));
    CHECK(has_upgrade(n.planes[1].upgrades, Upgrade::Sight));
    CHECK_FALSE(has_upgrade(n.planes[0].upgrades, Upgrade::Sight)); // that plane only
}

TEST_CASE("progress.txt keeps points, planes and upgrades for both nations") {
    Progress g;
    g.nations[0].points = 450;
    g.nations[0].planes[1].owned = true;
    g.nations[0].flying = 1;
    g.nations[0].planes[1].upgrades =
        static_cast<UpgradeSet>(upgrade_bit(Upgrade::Guns) | upgrade_bit(Upgrade::Sight));
    g.nations[1].points = 12;
    g.nations[1].planes[0].upgrades = upgrade_bit(Upgrade::Fuel);
    Progress back;
    std::string errors;
    CHECK(from_progress(to_progress(g), back, errors));
    CHECK(errors.empty());
    CHECK(back == g);
    Progress odd;
    CHECK_FALSE(from_progress("us.points = lots\nzzz = 1\nus.flying = p36a\n", odd, errors));
    CHECK(odd.nations[0].points == 0);
    CHECK(odd.nations[0].flying == 0); // not owned
}

TEST_CASE("The hangar buys with two presses, and moving cancels") {
    const Prices p;
    NationProgress n;
    n.points = 3500;
    Hangar h = open_hangar(n);
    CHECK(h.row == 0);
    hangar_move(h, 1); // the second plane
    CHECK(h.step == 1);
    CHECK(hangar_select(h, n, p) == HangarAction::None); // asks
    CHECK(h.confirm);
    hangar_move(h, 1);
    hangar_move(h, -1); // moved: the question is gone
    CHECK_FALSE(h.confirm);
    CHECK(hangar_select(h, n, p) == HangarAction::None);
    CHECK(hangar_select(h, n, p) == HangarAction::Changed);
    CHECK(n.planes[1].owned);
    CHECK(n.flying == 1);
    CHECK(n.points == 500);
    // An upgrade of the shown plane: the sight (300).
    hangar_point(h, kTreeSteps + static_cast<int>(Upgrade::Sight));
    CHECK(hangar_select(h, n, p) == HangarAction::None);
    CHECK(hangar_select(h, n, p) == HangarAction::Changed);
    CHECK(n.points == 200);
    // Back to the first plane: choosing it just selects it.
    hangar_point(h, 0);
    CHECK(hangar_select(h, n, p) == HangarAction::Changed);
    CHECK(n.flying == 0);
    hangar_point(h, kHangarFly);
    CHECK(hangar_select(h, n, p) == HangarAction::Fly);
    hangar_point(h, kHangarReset);
    CHECK(hangar_select(h, n, p) == HangarAction::None);
    CHECK(hangar_select(h, n, p) == HangarAction::Changed);
    CHECK(n == NationProgress{});
    hangar_point(h, kHangarBack);
    CHECK(hangar_select(h, n, p) == HangarAction::Back);
    hangar_move(h, 1);
    CHECK(h.row == 0); // wraps
}

TEST_CASE("Points, prices and upgrade strength are in the tuning file") {
    Tuning t;
    std::string errors;
    CHECK(from_tuning("price.step2 = 1000\nscore.chain_max = 6\nupgrade.engine_speed = 1.1\n", t,
                      errors));
    CHECK(t.rules.price.step2 == 1000);
    CHECK(t.rules.score.chain_max == doctest::Approx(6.0f));
    CHECK(t.rules.upgrade.engine_speed == doctest::Approx(1.1f));
}
