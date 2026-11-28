#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "core/ai.hpp"
#include "core/body.hpp"
#include "core/damage.hpp"
#include "core/flak.hpp"
#include "core/flight.hpp"
#include "core/gunners.hpp"
#include "core/input.hpp"
#include "core/plane_rules.hpp"
#include "core/planes.hpp"
#include "core/progress.hpp"
#include "core/rng.hpp"
#include "core/score.hpp"
#include "core/targets.hpp"
#include "core/upgrades.hpp"
#include "core/waves.hpp"
#include "core/weapons.hpp"

namespace wof {

inline constexpr int kTicksPerSecond = 60;
inline constexpr float kTickSeconds = 1.0f / static_cast<float>(kTicksPerSecond);

// The battle area's edges (design/flight.md, World and view; tuning.txt `edge.*`).
struct EdgeParams {
    float warning_u = 200.0f;  // a warning shows this close to the east, west or north edge
    float countdown_s = 10.0f; // past one of them, this long to fly back in

    friend constexpr bool operator==(const EdgeParams&, const EdgeParams&) = default;
};

// A plane with upgrades: a type and what's fitted to it (design/aircraft_tree.md).
struct Fit {
    PlaneId type = PlaneId::P12E;
    UpgradeSet upgrades = 0;

    friend constexpr bool operator==(const Fit&, const Fit&) = default;
};

// Everything step() needs to know about the rules.
// - `flight` and `damage` are the base: what every plane shares (gravity, the ceiling, the hit
//   table...).
// - `planes` holds each plane type's own rules, worked out from its real stats with `scale`
//   (core/plane_rules.hpp). Call rederive() after changing the base or the scale.
// - `fitted` holds the rules of each plane with upgrades in `fits`, which a body flies by its
//   `fit` (players' planes; add_fit() adds one).
struct Rules {
    FlightParams flight;
    EdgeParams edge;
    WeaponParams weapon;
    TargetParams targets;
    DamageParams damage;
    PlaneScale scale;
    AiParams ai;
    WaveParams wave;
    GunnerParams gunner;
    FlakParams flak;
    UpgradeParams upgrade;
    ScoreParams score;
    Prices price;
    // Game options (settings.txt), not tuning: how the players' and the AI pilots' rounds judge
    // height.
    HeightRule height_rule = HeightRule::Forgiving;
    HeightRule enemy_height_rule = HeightRule::Forgiving;
    bool player_collisions = true; // off: nothing collides with a player's plane in mid-air
    std::vector<PlaneRules> planes = derive_roster(flight, damage, scale);
    std::vector<Fit> fits;
    std::vector<PlaneRules> fitted;

    void rederive() {
        planes = derive_roster(flight, damage, scale);
        fitted.clear();
        for (const Fit& f : fits) {
            fitted.push_back(fit_plane(f.type, f.upgrades, flight, damage, scale, upgrade));
        }
    }
    // Adds a fitted plane; returns the index bodies fly it by.
    int add_fit(const Fit& f) {
        fits.push_back(f);
        fitted.push_back(fit_plane(f.type, f.upgrades, flight, damage, scale, upgrade));
        return static_cast<int>(fits.size()) - 1;
    }

    friend bool operator==(const Rules&, const Rules&) = default;
};

// A plane's own rules: with its upgrades if it flies a fit of its type, else the stock plane's.
[[nodiscard]] const PlaneRules& plane_of(const Rules& rules, const Body& b) noexcept;

// Everything that flies, and what it fires. Any number of planes; nothing here assumes there is
// only one player.
struct World {
    std::vector<Body> bodies;
    std::vector<Round> rounds;
    std::vector<Bomb> bombs;
    std::vector<Target> targets; // practice targets
    std::vector<Event> events;   // what happened in the last tick
    std::vector<Pilot> pilots;   // the enemy planes' AI pilots
    WaveState waves;             // when enemies arrive, and the kill count
    FlakField flak;              // the enemy's guns on the ground
    Rng rng{0x5eedu};            // where hits land; seed it per run
    std::int64_t tick = 0;
};

// A plane of a type, ready for a run: in the air with the start values, armed with its guns and
// bombs, and undamaged. `fit` is its upgrades (Rules::add_fit), or -1 for the stock plane.
[[nodiscard]] Body new_plane(const Rules& rules, PlaneId type, float x, float y, float heading_deg,
                             int fit = -1) noexcept;

// Plane-to-plane collisions (design/flight.md, Collisions): two flying planes within twice the
// collision radius across and the collision height of each other touch. The one with less total
// HP is destroyed (ShotDown); the other takes damage equal to what it had left, spread evenly over
// its parts. Equal totals destroy both. Practice drones don't collide. With player_collisions off,
// pairs with a player's plane in them pass through each other; enemies still collide.
void collide(World& w, const Rules& rules);

// Developer tool (F5, F6): `count` hits of `damage` on a plane from `from`, placed by the hit
// table as if they were real.
void debug_hits(World& w, std::size_t body, HitFrom from, float damage, int count,
                const Rules& rules);

// The ground and the edges, after a plane has moved:
// - touching the ground is a crash;
// - crossing the south edge is a return to base;
// - near the east, west or north edge a warning shows, and past one a countdown runs: flying
//   back in stops it (it starts from the top next time), and at 0 the plane self-destructs.
void apply_edges(Body& b, const EdgeParams& e, float dt) noexcept;

// Advances the world one tick (1/60 s): the targets move, each plane flies and fires, then the
// rounds and bombs move and hit. commands[i] drives bodies[i]; a body without a command gets no
// input. Bodies whose flight has ended stay where they are. `events` holds this tick's events.
void step(World& world, std::span<const InputCommand> commands, const Rules& rules);

} // namespace wof
