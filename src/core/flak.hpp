#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/planes.hpp"
#include "core/rng.hpp"

namespace wof {

struct World;
struct Rules;

// Enemy flak (design/flight.md, Flak guns): a front line of patchy gun groups halfway up the map
// and sites beyond it. Machine guns at first; light cannon swapped in later, then heavy guns at the
// sites and strongpoints. Guns fire only at players; bombs and strafing destroy them.

enum class FlakKind : std::uint8_t { MachineGun, Cannon, Heavy };
inline constexpr int kFlakKinds = 3;

// A kind of gun's numbers for a nation, from the real guns (design/planes.md, Flak). Heights and
// ranges in u (50 ft a u).
struct FlakStats {
    const char* name = "";
    float rpm = 100.0f;
    float damage = 1.0f;    // a round, or a burst at its centre
    float reach_u = 60.0f;  // as high as it can shoot
    float range_u = 180.0f; // across the ground
    float hp = 20.0f;
    float speed_u_s = 360.0f;
    float burst_u = 0.0f; // heavy guns: the burst's radius
};
[[nodiscard]] const FlakStats& flak_stats(Nation n, FlakKind k) noexcept;

// Where the guns go and how they build up (tuning.txt `flak.*`), all (start) values.
struct FlakParams {
    int front_groups = 14; // groups across the front line
    int guns_per_group = 3;
    int strongpoints = 4; // front groups that get a heavy gun
    int sites = 6;        // sites beyond the line, each with a heavy gun later
    int guns_per_site = 3;
    int cannon_wave = 5;         // machine guns become light cannon
    int heavy_wave = 9;          // heavy guns arrive
    int reinforce_every = 2;     // a gun added every this many waves
    int reinforce_max = 8;       // up to this many
    float spread_u = 60.0f;      // guns round their group's middle
    float site_near_u = 2600.0f; // sites this far beyond the line, at least
    float site_far_u = 12600.0f; // and at most
    float gun_radius_u = 8.0f;   // a gun's size, for strafing and bombs
    float scatter_u = 4.0f;      // heavy bursts scatter this much,
    float scatter_per_u = 0.15f; // plus this for each u of your height

    friend constexpr bool operator==(const FlakParams&, const FlakParams&) = default;
};

struct FlakGun {
    FlakKind kind = FlakKind::MachineGun;
    float x = 0.0f;
    float y = 0.0f;
    float hp = 20.0f;
    bool alive = true;
    float wait_s = 0.0f; // until it can fire again
    float look_s = 0.0f; // until it looks at its target again
    float seen_age_s = 0.0f;
    float seen_x = 0.0f;
    float seen_y = 0.0f;
    float seen_z = 0.0f;
    float seen_vx = 0.0f;
    float seen_vy = 0.0f;
    float seen_vz = 0.0f;
    float off_deg = 0.0f;
    float off_pitch_deg = 0.0f;
    float since_fired_s = 1e9f; // for the muzzle flash
};

// A heavy shell on its way up to burst.
struct FlakShell {
    float x = 0.0f; // where it bursts
    float y = 0.0f;
    float z = 0.0f;
    float fuse_s = 0.0f;
    float damage = 0.0f;
    float radius_u = 0.0f;
    int gun = 0;
};

// A group on the front line, or a site beyond it.
struct FlakPlace {
    float x = 0.0f;
    float y = 0.0f;
    bool site = false;
    bool strong = false; // a front group that gets a heavy gun
};

// All the flak of a run. Empty places: no flak (tests, practice).
struct FlakField {
    Nation nation = Nation::Germany;
    std::vector<FlakPlace> places;
    std::vector<FlakGun> guns;
    std::vector<FlakShell> shells;
    int last_wave = 0; // waves dealt with: step-ups and reinforcements
    int reinforced = 0;
    int destroyed = 0;
};

// The field at a run's start: machine guns on the front line's groups and the sites.
[[nodiscard]] FlakField place_flak(Nation enemy, const FlakParams& p, Rng& rng);

// One tick: step-ups and reinforcements as waves arrive, the guns firing, shells bursting.
void update_flak(World& w, const Rules& rules);

// A player's round or bomb damaging a gun; destroying it records EventKind::FlakDown.
void hit_flak(World& w, std::size_t gun, float damage, int by);

} // namespace wof
