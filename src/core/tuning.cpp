#include "core/tuning.hpp"

#include <algorithm>
#include <array>
#include <string>

#include "core/text.hpp"

namespace wof {
namespace {

struct FloatKey {
    const char* section; // the comment line it starts, or nullptr
    const char* key;
    float& (*field)(Tuning&);
    float lo;
    float hi;
};

struct IntKey {
    const char* section;
    const char* key;
    int& (*field)(Tuning&);
    int lo;
    int hi;
};

// Every value, in the file's order. The int ranges match validate(GroundParams&).
constexpr std::array<FloatKey, 112> kFloats = {{
    {"Camera: how far ahead it looks (u), and how fast that swings round after a turn (per s)",
     "camera.lead_u", [](Tuning& t) -> float& { return t.camera.lead_u; }, 0.0f, 400.0f},
    {nullptr, "camera.lead_ease", [](Tuning& t) -> float& { return t.camera.lead_ease; }, 0.1f,
     60.0f},
    {"Flight, for every plane (each plane's speeds, climb, stall, turn, pitch rate and tank come "
     "from its real stats, below): u/s, degrees and seconds (design/flight.md, Flight)",
     "flight.gravity", [](Tuning& t) -> float& { return t.rules.flight.gravity; }, 1.0f, 1000.0f},
    {nullptr, "flight.glide_ratio", [](Tuning& t) -> float& { return t.rules.flight.glide_ratio; },
     1.0f, 50.0f},
    {nullptr, "flight.pitch_limit_deg",
     [](Tuning& t) -> float& { return t.rules.flight.pitch_limit_deg; }, 0.0f, 89.0f},
    {nullptr, "flight.assist_zone_deg",
     [](Tuning& t) -> float& { return t.rules.flight.assist_zone_deg; }, 0.0f, 40.0f},
    {nullptr, "flight.throttle_per_s",
     [](Tuning& t) -> float& { return t.rules.flight.throttle_per_s; }, 0.01f, 10.0f},
    {nullptr, "flight.stall_nose_deg",
     [](Tuning& t) -> float& { return t.rules.flight.stall_nose_deg; }, -89.0f, 0.0f},
    {nullptr, "flight.stall_nose_rate_deg_s",
     [](Tuning& t) -> float& { return t.rules.flight.stall_nose_rate_deg_s; }, 0.0f, 720.0f},
    {nullptr, "flight.stall_control",
     [](Tuning& t) -> float& { return t.rules.flight.stall_control; }, 0.0f, 1.0f},
    {nullptr, "flight.ceiling_u", [](Tuning& t) -> float& { return t.rules.flight.ceiling_u; },
     1.0f, 2000.0f},
    {nullptr, "flight.height_share",
     [](Tuning& t) -> float& { return t.rules.flight.height_share; }, 0.05f, 2.0f},
    {nullptr, "flight.idle_burn", [](Tuning& t) -> float& { return t.rules.flight.idle_burn; },
     0.0f, 1.0f},
    {nullptr, "flight.bank_deg", [](Tuning& t) -> float& { return t.rules.flight.bank_deg; }, 0.0f,
     89.0f},
    {nullptr, "flight.bank_rate_deg_s",
     [](Tuning& t) -> float& { return t.rules.flight.bank_rate_deg_s; }, 1.0f, 1440.0f},
    {"A new run starts in the air: height (u), speed (u/s), throttle (0-1)",
     "flight.start_height_u", [](Tuning& t) -> float& { return t.rules.flight.start_height_u; },
     0.0f, 2000.0f},
    {nullptr, "flight.start_speed_u_s",
     [](Tuning& t) -> float& { return t.rules.flight.start_speed_u_s; }, 0.0f, 1000.0f},
    {nullptr, "flight.start_throttle",
     [](Tuning& t) -> float& { return t.rules.flight.start_throttle; }, 0.0f, 1.0f},
    {"Edges: the warning distance from the east, west and north edges (u), and the countdown past "
     "one (s)",
     "edge.warning_u", [](Tuning& t) -> float& { return t.rules.edge.warning_u; }, 0.0f, 2000.0f},
    {nullptr, "edge.countdown_s", [](Tuning& t) -> float& { return t.rules.edge.countdown_s; },
     0.1f, 600.0f},
    {"The scale from real stats to game values (design/planes.md): u/s per mph, turn factor, u/s "
     "per ft/min of climb, dive limit and best turn speed as shares of top speed, tank s per hour "
     "of endurance, stall margin (u/s)",
     "scale.u_per_mph", [](Tuning& t) -> float& { return t.rules.scale.u_per_mph; }, 0.01f, 10.0f},
    {nullptr, "scale.turn_factor", [](Tuning& t) -> float& { return t.rules.scale.turn_factor; },
     0.1f, 50.0f},
    {nullptr, "scale.u_per_fpm", [](Tuning& t) -> float& { return t.rules.scale.u_per_fpm; },
     0.0001f, 1.0f},
    {nullptr, "scale.dive_factor", [](Tuning& t) -> float& { return t.rules.scale.dive_factor; },
     1.0f, 5.0f},
    {nullptr, "scale.best_turn_share",
     [](Tuning& t) -> float& { return t.rules.scale.best_turn_share; }, 0.1f, 1.0f},
    {nullptr, "scale.tank_s_per_hour",
     [](Tuning& t) -> float& { return t.rules.scale.tank_s_per_hour; }, 1.0f, 100000.0f},
    {nullptr, "scale.stall_margin_u_s",
     [](Tuning& t) -> float& { return t.rules.scale.stall_margin_u_s; }, 0.0f, 100.0f},
    {"Calibres: damage per hit, round speed (u/s on top of the plane's), round life (s)",
     "calibre.rifle.damage", [](Tuning& t) -> float& { return t.rules.weapon.calibres[0].damage; },
     0.0f, 1000.0f},
    {nullptr, "calibre.rifle.speed_u_s",
     [](Tuning& t) -> float& { return t.rules.weapon.calibres[0].speed_u_s; }, 1.0f, 3000.0f},
    {nullptr, "calibre.rifle.life_s",
     [](Tuning& t) -> float& { return t.rules.weapon.calibres[0].life_s; }, 0.05f, 10.0f},
    {nullptr, "calibre.heavy.damage",
     [](Tuning& t) -> float& { return t.rules.weapon.calibres[1].damage; }, 0.0f, 1000.0f},
    {nullptr, "calibre.heavy.speed_u_s",
     [](Tuning& t) -> float& { return t.rules.weapon.calibres[1].speed_u_s; }, 1.0f, 3000.0f},
    {nullptr, "calibre.heavy.life_s",
     [](Tuning& t) -> float& { return t.rules.weapon.calibres[1].life_s; }, 0.05f, 10.0f},
    {nullptr, "calibre.cannon.damage",
     [](Tuning& t) -> float& { return t.rules.weapon.calibres[2].damage; }, 0.0f, 1000.0f},
    {nullptr, "calibre.cannon.speed_u_s",
     [](Tuning& t) -> float& { return t.rules.weapon.calibres[2].speed_u_s; }, 1.0f, 3000.0f},
    {nullptr, "calibre.cannon.life_s",
     [](Tuning& t) -> float& { return t.rules.weapon.calibres[2].life_s; }, 0.05f, 10.0f},
    {"Weapons: where rounds start (u), plane size for hits (u), strafing reach (u), height bands "
     "(u, degrees), bombs (ripple s, drop u), a 100 lb bomb's blast (u, damage)",
     "weapon.muzzle_u", [](Tuning& t) -> float& { return t.rules.weapon.muzzle_u; }, 0.0f, 40.0f},
    {nullptr, "weapon.hit_radius_u",
     [](Tuning& t) -> float& { return t.rules.weapon.hit_radius_u; }, 0.5f, 100.0f},
    {nullptr, "weapon.strafe_radius_u",
     [](Tuning& t) -> float& { return t.rules.weapon.strafe_radius_u; }, 0.0f, 100.0f},
    {nullptr, "weapon.band_forgiving_u",
     [](Tuning& t) -> float& { return t.rules.weapon.band_forgiving_u; }, 0.0f, 200.0f},
    {nullptr, "weapon.band_strict_u",
     [](Tuning& t) -> float& { return t.rules.weapon.band_strict_u; }, 0.0f, 200.0f},
    {nullptr, "weapon.autoaim_cone_deg",
     [](Tuning& t) -> float& { return t.rules.weapon.autoaim_cone_deg; }, 0.0f, 90.0f},
    {nullptr, "weapon.autoaim_height_u",
     [](Tuning& t) -> float& { return t.rules.weapon.autoaim_height_u; }, 0.0f, 500.0f},
    {nullptr, "weapon.bomb_interval_s",
     [](Tuning& t) -> float& { return t.rules.weapon.bomb_interval_s; }, 0.02f, 10.0f},
    {nullptr, "weapon.bomb_drop_u", [](Tuning& t) -> float& { return t.rules.weapon.bomb_drop_u; },
     0.0f, 20.0f},
    {nullptr, "weapon.blast_radius_u",
     [](Tuning& t) -> float& { return t.rules.weapon.blast_radius_u; }, 0.0f, 500.0f},
    {nullptr, "weapon.blast_damage",
     [](Tuning& t) -> float& { return t.rules.weapon.blast_damage; }, 0.0f, 10000.0f},
    {"Practice targets: drone speed (u/s), ground target size (u)", "targets.drone_speed_u_s",
     [](Tuning& t) -> float& { return t.rules.targets.drone_speed_u_s; }, 0.0f, 600.0f},
    {nullptr, "targets.ground_radius_u",
     [](Tuning& t) -> float& { return t.rules.targets.ground_radius_u; }, 1.0f, 200.0f},
    {"Looks: how long a bomb's scorch mark takes to fade (s); the ground's zoom at the ceiling",
     "look.scorch_fade_s", [](Tuning& t) -> float& { return t.look.scorch_fade_s; }, 0.1f, 3600.0f},
    {nullptr, "look.zoom_at_ceiling", [](Tuning& t) -> float& { return t.look.zoom_at_ceiling; },
     0.1f, 1.0f},
    {"Damage (each plane's HP is its own, design/planes.md): the pilot's wound size, what's left "
     "at 1 HP (shares), the leak (s for a tank), collisions (u), going down (degrees/s, degrees)",
     "damage.wound_damage", [](Tuning& t) -> float& { return t.rules.damage.wound_damage; }, 0.1f,
     10000.0f},
    {nullptr, "damage.engine_min_share",
     [](Tuning& t) -> float& { return t.rules.damage.engine_min_share; }, 0.0f, 1.0f},
    {nullptr, "damage.wing_min_share",
     [](Tuning& t) -> float& { return t.rules.damage.wing_min_share; }, 0.0f, 1.0f},
    {nullptr, "damage.leak_tank_s", [](Tuning& t) -> float& { return t.rules.damage.leak_tank_s; },
     1.0f, 36000.0f},
    {nullptr, "damage.collision_radius_u",
     [](Tuning& t) -> float& { return t.rules.damage.collision_radius_u; }, 0.0f, 100.0f},
    {nullptr, "damage.collision_height_u",
     [](Tuning& t) -> float& { return t.rules.damage.collision_height_u; }, 0.0f, 100.0f},
    {nullptr, "damage.spin_rate_deg_s",
     [](Tuning& t) -> float& { return t.rules.damage.spin_rate_deg_s; }, 0.0f, 1440.0f},
    {nullptr, "damage.spin_pitch_deg",
     [](Tuning& t) -> float& { return t.rules.damage.spin_pitch_deg; }, -89.0f, 0.0f},
    {"Enemy pilots: aim error (degrees, wave 1, the best, sharper each wave), reaction (s, "
     "same), firing range (u) and cone (deg), the dodge look-ahead (s) and width (u), the dogfight "
     "range (u), the longest climb or line-up (s), the floor they keep above, how far inside the "
     "edges they turn back, the dive-and-climb height, the head-on run, extend time (s)",
     "ai.aim_error_first_deg", [](Tuning& t) -> float& { return t.rules.ai.aim_error_first_deg; },
     0.0f, 45.0f},
    {nullptr, "ai.aim_error_best_deg",
     [](Tuning& t) -> float& { return t.rules.ai.aim_error_best_deg; }, 0.0f, 45.0f},
    {nullptr, "ai.aim_error_step_deg",
     [](Tuning& t) -> float& { return t.rules.ai.aim_error_step_deg; }, 0.0f, 45.0f},
    {nullptr, "ai.reaction_first_s",
     [](Tuning& t) -> float& { return t.rules.ai.reaction_first_s; }, 0.02f, 5.0f},
    {nullptr, "ai.reaction_best_s", [](Tuning& t) -> float& { return t.rules.ai.reaction_best_s; },
     0.02f, 5.0f},
    {nullptr, "ai.reaction_step_s", [](Tuning& t) -> float& { return t.rules.ai.reaction_step_s; },
     0.0f, 5.0f},
    {nullptr, "ai.fire_range_u", [](Tuning& t) -> float& { return t.rules.ai.fire_range_u; }, 10.0f,
     1000.0f},
    {nullptr, "ai.fire_cone_deg", [](Tuning& t) -> float& { return t.rules.ai.fire_cone_deg; },
     0.5f, 45.0f},
    {nullptr, "ai.avoid_s", [](Tuning& t) -> float& { return t.rules.ai.avoid_s; }, 0.0f, 3.0f},
    {nullptr, "ai.avoid_u", [](Tuning& t) -> float& { return t.rules.ai.avoid_u; }, 0.0f, 200.0f},
    {nullptr, "ai.dogfight_u", [](Tuning& t) -> float& { return t.rules.ai.dogfight_u; }, 0.0f,
     2000.0f},
    {nullptr, "ai.climb_max_s", [](Tuning& t) -> float& { return t.rules.ai.climb_max_s; }, 0.5f,
     60.0f},
    {nullptr, "ai.floor_u", [](Tuning& t) -> float& { return t.rules.ai.floor_u; }, 0.0f, 500.0f},
    {nullptr, "ai.edge_margin_u", [](Tuning& t) -> float& { return t.rules.ai.edge_margin_u; },
     0.0f, 1000.0f},
    {nullptr, "ai.dive_height_u", [](Tuning& t) -> float& { return t.rules.ai.dive_height_u; },
     0.0f, 500.0f},
    {nullptr, "ai.head_on_run_u", [](Tuning& t) -> float& { return t.rules.ai.head_on_run_u; },
     50.0f, 2000.0f},
    {nullptr, "ai.extend_s", [](Tuning& t) -> float& { return t.rules.ai.extend_s; }, 0.1f, 30.0f},
    {"Waves: the first after (s); once everything's down the next after a break (s), or call it "
     "once a wave has been up this long (s). Fighters appear this far from you (u), within this of "
     "your height (u), this far apart (u); raids come in this far inside the north edge (u); at "
     "most wave.max_alive fighters in the air",
     "wave.first_s", [](Tuning& t) -> float& { return t.rules.wave.first_s; }, 0.0f, 600.0f},
    {nullptr, "wave.break_s", [](Tuning& t) -> float& { return t.rules.wave.break_s; }, 0.0f,
     600.0f},
    {nullptr, "wave.call_after_s", [](Tuning& t) -> float& { return t.rules.wave.call_after_s; },
     1.0f, 3600.0f},
    {nullptr, "wave.out_u", [](Tuning& t) -> float& { return t.rules.wave.out_u; }, 500.0f,
     20000.0f},
    {nullptr, "wave.height_spread_u",
     [](Tuning& t) -> float& { return t.rules.wave.height_spread_u; }, 0.0f, 500.0f},
    {nullptr, "wave.spread_u", [](Tuning& t) -> float& { return t.rules.wave.spread_u; }, 0.0f,
     1000.0f},
    {nullptr, "wave.edge_inset_u", [](Tuning& t) -> float& { return t.rules.wave.edge_inset_u; },
     0.0f, 2000.0f},
    {"Points: a kill within the window keeps the chain, raising the multiplier by the step up to "
     "the most; getting home adds up to the condition share, plus the wave share per wave",
     "score.chain_window_s", [](Tuning& t) -> float& { return t.rules.score.chain_window_s; }, 1.0f,
     600.0f},
    {nullptr, "score.chain_step", [](Tuning& t) -> float& { return t.rules.score.chain_step; },
     0.0f, 10.0f},
    {nullptr, "score.chain_max", [](Tuning& t) -> float& { return t.rules.score.chain_max; }, 1.0f,
     100.0f},
    {nullptr, "score.condition_bonus",
     [](Tuning& t) -> float& { return t.rules.score.condition_bonus; }, 0.0f, 10.0f},
    {nullptr, "score.wave_bonus", [](Tuning& t) -> float& { return t.rules.score.wave_bonus; },
     0.0f, 10.0f},
    {"Upgrades: top speed and climb with the engine, endurance with the tank (each times this); "
     "the shares of hits on the pilot and of a fuel leak that armour and a sealed tank let through",
     "upgrade.engine_speed", [](Tuning& t) -> float& { return t.rules.upgrade.engine_speed; }, 0.5f,
     2.0f},
    {nullptr, "upgrade.engine_climb",
     [](Tuning& t) -> float& { return t.rules.upgrade.engine_climb; }, 0.5f, 2.0f},
    {nullptr, "upgrade.fuel_endurance",
     [](Tuning& t) -> float& { return t.rules.upgrade.fuel_endurance; }, 0.5f, 4.0f},
    {nullptr, "upgrade.armour_pilot",
     [](Tuning& t) -> float& { return t.rules.upgrade.armour_pilot; }, 0.1f, 1.0f},
    {nullptr, "upgrade.sealed_leak",
     [](Tuning& t) -> float& { return t.rules.upgrade.sealed_leak; }, 0.1f, 1.0f},
    {"Gunners: a bomber's fire at planes within the range (u), a burst and a pause (s) at a time, "
     "each in its arc (degrees from the nose, or from the tail); within level_u above or below "
     "counts as level",
     "gunner.range_u", [](Tuning& t) -> float& { return t.rules.gunner.range_u; }, 0.0f, 1000.0f},
    {nullptr, "gunner.burst_s", [](Tuning& t) -> float& { return t.rules.gunner.burst_s; }, 0.05f,
     60.0f},
    {nullptr, "gunner.pause_s", [](Tuning& t) -> float& { return t.rules.gunner.pause_s; }, 0.05f,
     60.0f},
    {nullptr, "gunner.nose_arc_deg",
     [](Tuning& t) -> float& { return t.rules.gunner.nose_arc_deg; }, 0.0f, 180.0f},
    {nullptr, "gunner.dorsal_arc_deg",
     [](Tuning& t) -> float& { return t.rules.gunner.dorsal_arc_deg; }, 0.0f, 180.0f},
    {nullptr, "gunner.ventral_arc_deg",
     [](Tuning& t) -> float& { return t.rules.gunner.ventral_arc_deg; }, 0.0f, 180.0f},
    {nullptr, "gunner.level_u", [](Tuning& t) -> float& { return t.rules.gunner.level_u; }, 0.0f,
     100.0f},
    {"Bomber raids: from this height (u) up to the plane's top, in within this of your base's "
     "line, spaced across and back (u)",
     "wave.raid_low_u", [](Tuning& t) -> float& { return t.rules.wave.raid_low_u; }, 20.0f, 500.0f},
    {nullptr, "wave.raid_x_u", [](Tuning& t) -> float& { return t.rules.wave.raid_x_u; }, 0.0f,
     14000.0f},
    {nullptr, "wave.raid_spacing_u",
     [](Tuning& t) -> float& { return t.rules.wave.raid_spacing_u; }, 40.0f, 1000.0f},
    {nullptr, "wave.raid_back_u", [](Tuning& t) -> float& { return t.rules.wave.raid_back_u; },
     0.0f, 1000.0f},
    {"Escorts: fight you within the first distance of their bombers, back to them beyond the "
     "second (u)",
     "ai.escort_break_u", [](Tuning& t) -> float& { return t.rules.ai.escort_break_u; }, 0.0f,
     30000.0f},
    {nullptr, "ai.escort_return_u", [](Tuning& t) -> float& { return t.rules.ai.escort_return_u; },
     0.0f, 30000.0f},
    {"Flak: guns spread round their group's middle (u); sites between these far beyond the front "
     "line (u); a gun's size for strafing and bombs (u); heavy bursts scatter this (u), plus this "
     "per u of your height",
     "flak.spread_u", [](Tuning& t) -> float& { return t.rules.flak.spread_u; }, 0.0f, 1000.0f},
    {nullptr, "flak.site_near_u", [](Tuning& t) -> float& { return t.rules.flak.site_near_u; },
     0.0f, 14000.0f},
    {nullptr, "flak.site_far_u", [](Tuning& t) -> float& { return t.rules.flak.site_far_u; }, 0.0f,
     14000.0f},
    {nullptr, "flak.gun_radius_u", [](Tuning& t) -> float& { return t.rules.flak.gun_radius_u; },
     1.0f, 100.0f},
    {nullptr, "flak.scatter_u", [](Tuning& t) -> float& { return t.rules.flak.scatter_u; }, 0.0f,
     200.0f},
    {nullptr, "flak.scatter_per_u", [](Tuning& t) -> float& { return t.rules.flak.scatter_per_u; },
     0.0f, 5.0f},
}};

constexpr std::array<IntKey, 64> kInts = {{
    {"Waves: enemy fighters in the air at once, at most", "wave.max_alive",
     [](Tuning& t) -> int& { return t.rules.wave.max_alive; }, 1, 64},
    {"Waves: the enemy's fighters move up a stage of its aircraft tree every this many waves",
     "wave.tier_every", [](Tuning& t) -> int& { return t.rules.wave.tier_every; }, 1, 100},
    {"Weapons: one round in N, per gun, is a tracer", "weapon.tracer_every",
     [](Tuning& t) -> int& { return t.rules.weapon.tracer_every; }, 1, 100},
    {"Damage: the pilot's wounds (the last kills); where hits land, in percent of hits by where "
     "they come from",
     "damage.pilot_wounds", [](Tuning& t) -> int& { return t.rules.damage.pilot_wounds; }, 1, 100},
    {nullptr, "hit.ahead.engine", [](Tuning& t) -> int& { return t.rules.damage.table[0][0]; }, 0,
     100},
    {nullptr, "hit.ahead.pilot", [](Tuning& t) -> int& { return t.rules.damage.table[0][1]; }, 0,
     100},
    {nullptr, "hit.ahead.wings", [](Tuning& t) -> int& { return t.rules.damage.table[0][2]; }, 0,
     100},
    {nullptr, "hit.ahead.fuselage", [](Tuning& t) -> int& { return t.rules.damage.table[0][3]; }, 0,
     100},
    {nullptr, "hit.behind.engine", [](Tuning& t) -> int& { return t.rules.damage.table[1][0]; }, 0,
     100},
    {nullptr, "hit.behind.pilot", [](Tuning& t) -> int& { return t.rules.damage.table[1][1]; }, 0,
     100},
    {nullptr, "hit.behind.wings", [](Tuning& t) -> int& { return t.rules.damage.table[1][2]; }, 0,
     100},
    {nullptr, "hit.behind.fuselage", [](Tuning& t) -> int& { return t.rules.damage.table[1][3]; },
     0, 100},
    {nullptr, "hit.side.engine", [](Tuning& t) -> int& { return t.rules.damage.table[2][0]; }, 0,
     100},
    {nullptr, "hit.side.pilot", [](Tuning& t) -> int& { return t.rules.damage.table[2][1]; }, 0,
     100},
    {nullptr, "hit.side.wings", [](Tuning& t) -> int& { return t.rules.damage.table[2][2]; }, 0,
     100},
    {nullptr, "hit.side.fuselage", [](Tuning& t) -> int& { return t.rules.damage.table[2][3]; }, 0,
     100},
    {nullptr, "hit.burst.engine", [](Tuning& t) -> int& { return t.rules.damage.table[3][0]; }, 0,
     100},
    {nullptr, "hit.burst.pilot", [](Tuning& t) -> int& { return t.rules.damage.table[3][1]; }, 0,
     100},
    {nullptr, "hit.burst.wings", [](Tuning& t) -> int& { return t.rules.damage.table[3][2]; }, 0,
     100},
    {nullptr, "hit.burst.fuselage", [](Tuning& t) -> int& { return t.rules.damage.table[3][3]; }, 0,
     100},
    {"Ground (the farmland, in u; a change redraws it)", "ground.block_u",
     [](Tuning& t) -> int& { return t.ground.block_u; }, 64, 4096},
    {nullptr, "ground.road_u", [](Tuning& t) -> int& { return t.ground.road_u; }, 0, 32},
    {nullptr, "ground.road_skip_pct", [](Tuning& t) -> int& { return t.ground.road_skip_pct; }, 0,
     100},
    {nullptr, "ground.field_min_u", [](Tuning& t) -> int& { return t.ground.field_min_u; }, 8,
     1024},
    {nullptr, "ground.field_max_u", [](Tuning& t) -> int& { return t.ground.field_max_u; }, 16,
     4096},
    {nullptr, "ground.hedge_u", [](Tuning& t) -> int& { return t.ground.hedge_u; }, 0, 16},
    {nullptr, "ground.hedge_missing_pct",
     [](Tuning& t) -> int& { return t.ground.hedge_missing_pct; }, 0, 100},
    {nullptr, "ground.row_u", [](Tuning& t) -> int& { return t.ground.row_u; }, 1, 64},
    {nullptr, "ground.river_u", [](Tuning& t) -> int& { return t.ground.river_u; }, 0, 400},
    {nullptr, "ground.river_bank_u", [](Tuning& t) -> int& { return t.ground.river_bank_u; }, 0,
     64},
    {nullptr, "ground.river_step_u", [](Tuning& t) -> int& { return t.ground.river_step_u; }, 32,
     1024},
    {nullptr, "ground.river_swing_u", [](Tuning& t) -> int& { return t.ground.river_swing_u; }, 0,
     1024},
    {"Field kinds: how often each comes up, relative to the others", "ground.weight.wheat",
     [](Tuning& t) -> int& { return t.ground.weights[0]; }, 0, 1000},
    {nullptr, "ground.weight.barley", [](Tuning& t) -> int& { return t.ground.weights[1]; }, 0,
     1000},
    {nullptr, "ground.weight.ploughed", [](Tuning& t) -> int& { return t.ground.weights[2]; }, 0,
     1000},
    {nullptr, "ground.weight.pasture", [](Tuning& t) -> int& { return t.ground.weights[3]; }, 0,
     1000},
    {nullptr, "ground.weight.meadow", [](Tuning& t) -> int& { return t.ground.weights[4]; }, 0,
     1000},
    {nullptr, "ground.weight.woods", [](Tuning& t) -> int& { return t.ground.weights[5]; }, 0,
     1000},
    {"Points for a kill", "score.fighter_points",
     [](Tuning& t) -> int& { return t.rules.score.fighter_points; }, 0, 100000},
    {nullptr, "score.bomber_points", [](Tuning& t) -> int& { return t.rules.score.bomber_points; },
     0, 100000},
    {"Prices: the second and third fighters of a tree, then the upgrades", "price.step2",
     [](Tuning& t) -> int& { return t.rules.price.step2; }, 0, 1000000},
    {nullptr, "price.step3", [](Tuning& t) -> int& { return t.rules.price.step3; }, 0, 1000000},
    {nullptr, "price.guns", [](Tuning& t) -> int& { return t.rules.price.guns; }, 0, 1000000},
    {nullptr, "price.engine", [](Tuning& t) -> int& { return t.rules.price.engine; }, 0, 1000000},
    {nullptr, "price.fuel", [](Tuning& t) -> int& { return t.rules.price.fuel; }, 0, 1000000},
    {nullptr, "price.bombs", [](Tuning& t) -> int& { return t.rules.price.bombs; }, 0, 1000000},
    {nullptr, "price.armour", [](Tuning& t) -> int& { return t.rules.price.armour; }, 0, 1000000},
    {nullptr, "price.sight", [](Tuning& t) -> int& { return t.rules.price.sight; }, 0, 1000000},
    {"Bomber raids: every this many waves (0: none), the later bombers from this wave; and the "
     "gunners' shares of hits (percent) from ahead and, each, from behind",
     "wave.raid_every", [](Tuning& t) -> int& { return t.rules.wave.raid_every; }, 0, 1000},
    {nullptr, "wave.raid_later", [](Tuning& t) -> int& { return t.rules.wave.raid_later; }, 1,
     1000},
    {nullptr, "damage.nose_gunner_pct",
     [](Tuning& t) -> int& { return t.rules.damage.nose_gunner_pct; }, 0, 100},
    {nullptr, "damage.rear_gunner_pct",
     [](Tuning& t) -> int& { return t.rules.damage.rear_gunner_pct; }, 0, 50},
    {"Flak: groups on the front line and guns in each, the strongpoints among them, sites beyond "
     "and guns in each; machine guns become cannon at one wave and heavy guns arrive at another; a "
     "gun added every this many waves, up to a most",
     "flak.front_groups", [](Tuning& t) -> int& { return t.rules.flak.front_groups; }, 0, 200},
    {nullptr, "flak.guns_per_group", [](Tuning& t) -> int& { return t.rules.flak.guns_per_group; },
     0, 20},
    {nullptr, "flak.strongpoints", [](Tuning& t) -> int& { return t.rules.flak.strongpoints; }, 0,
     200},
    {nullptr, "flak.sites", [](Tuning& t) -> int& { return t.rules.flak.sites; }, 0, 100},
    {nullptr, "flak.guns_per_site", [](Tuning& t) -> int& { return t.rules.flak.guns_per_site; }, 0,
     20},
    {nullptr, "flak.cannon_wave", [](Tuning& t) -> int& { return t.rules.flak.cannon_wave; }, 1,
     1000},
    {nullptr, "flak.heavy_wave", [](Tuning& t) -> int& { return t.rules.flak.heavy_wave; }, 1,
     1000},
    {nullptr, "flak.reinforce_every",
     [](Tuning& t) -> int& { return t.rules.flak.reinforce_every; }, 0, 1000},
    {nullptr, "flak.reinforce_max", [](Tuning& t) -> int& { return t.rules.flak.reinforce_max; }, 0,
     200},
    {"Points for destroying a flak gun", "score.flak_light",
     [](Tuning& t) -> int& { return t.rules.score.flak_light; }, 0, 100000},
    {nullptr, "score.flak_heavy", [](Tuning& t) -> int& { return t.rules.score.flak_heavy; }, 0,
     100000},
    {"The front line's band of trenches across the map (u; 0: none)", "ground.front_depth_u",
     [](Tuning& t) -> int& { return t.ground.front_depth_u; }, 0, 4000},
}};

void section(std::string& out, const char* name) {
    if (name != nullptr) {
        out.append("\n# ").append(name).append("\n");
    }
}

} // namespace

std::string to_tuning(const Tuning& t) {
    Tuning copy = t; // the table's accessors take a non-const Tuning
    std::string out =
        "# Wings of Freedom live tuning: the game reloads this file when it changes.\n";
    out += "# Every number is a (start) value; see design/flight.md and design/planes.md.\n";
    out += "format = " + std::to_string(kTuningFormat) + "\n";
    for (const FloatKey& k : kFloats) {
        section(out, k.section);
        out.append(k.key).append(" = ").append(format_float(k.field(copy))).append("\n");
    }
    for (const IntKey& k : kInts) {
        section(out, k.section);
        out.append(k.key).append(" = ").append(std::to_string(k.field(copy))).append("\n");
    }
    return out;
}

int tuning_format(std::string_view text) {
    KeyValueLines lines(text);
    while (lines.next()) {
        int v = 0;
        if (!lines.bad() && lines.key() == "format" && parse_int(std::string(lines.value()), v)) {
            return v;
        }
    }
    return 1;
}

bool from_tuning(std::string_view text, Tuning& t, std::string& errors) {
    const std::size_t errors_before = errors.size();
    KeyValueLines lines(text);
    while (lines.next()) {
        if (lines.bad()) {
            report_line(errors, lines.line(), "expected key = value");
            continue;
        }
        const std::string_view key = lines.key();
        const std::string value(lines.value());
        if (key == "format") {
            continue; // which version wrote the file: see tuning_format()
        }
        const auto f = std::find_if(kFloats.begin(), kFloats.end(),
                                    [&](const FloatKey& k) { return key == k.key; });
        if (f != kFloats.end()) {
            float v = 0.0f;
            if (!parse_float(value, v)) {
                report_line(errors, lines.line(), std::string(key) + ": not a number: " + value);
                continue;
            }
            const float c = std::clamp(v, f->lo, f->hi);
            if (c != v) {
                report_line(errors, lines.line(),
                            std::string(key) + ": clamped to " + format_float(c));
            }
            f->field(t) = c;
            continue;
        }
        const auto i =
            std::find_if(kInts.begin(), kInts.end(), [&](const IntKey& k) { return key == k.key; });
        if (i != kInts.end()) {
            int v = 0;
            if (!parse_int(value, v)) {
                report_line(errors, lines.line(),
                            std::string(key) + ": not a whole number: " + value);
                continue;
            }
            const int c = std::clamp(v, i->lo, i->hi);
            if (c != v) {
                report_line(errors, lines.line(),
                            std::string(key) + ": clamped to " + std::to_string(c));
            }
            i->field(t) = c;
            continue;
        }
        report_line(errors, lines.line(), "unknown key " + std::string(key));
    }
    if (!validate(t.ground)) {
        errors += "ground: adjusted to stay usable (field_max_u at least twice field_min_u, and "
                  "at least one field weight above 0)\n";
    }
    t.rules.rederive(); // each plane's rules follow the base and the scale
    return errors.size() == errors_before;
}

} // namespace wof
