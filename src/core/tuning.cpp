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
constexpr std::array<FloatKey, 77> kFloats = {{
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
     "same), firing range (u) and cone (deg), break-off range, the floor they keep above, how far "
     "inside the edges they turn back, the dive-and-climb height, the head-on run, extend time (s)",
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
    {nullptr, "ai.break_range_u", [](Tuning& t) -> float& { return t.rules.ai.break_range_u; },
     0.0f, 500.0f},
    {nullptr, "ai.floor_u", [](Tuning& t) -> float& { return t.rules.ai.floor_u; }, 0.0f, 500.0f},
    {nullptr, "ai.edge_margin_u", [](Tuning& t) -> float& { return t.rules.ai.edge_margin_u; },
     0.0f, 1000.0f},
    {nullptr, "ai.dive_height_u", [](Tuning& t) -> float& { return t.rules.ai.dive_height_u; },
     0.0f, 500.0f},
    {nullptr, "ai.head_on_run_u", [](Tuning& t) -> float& { return t.rules.ai.head_on_run_u; },
     50.0f, 2000.0f},
    {nullptr, "ai.extend_s", [](Tuning& t) -> float& { return t.rules.ai.extend_s; }, 0.1f, 30.0f},
    {"Waves: the first and then every (s), arriving within this of your height (u), spread (u) "
     "across, and inside the north edge (u); at most wave.max_alive in the air",
     "wave.first_s", [](Tuning& t) -> float& { return t.rules.wave.first_s; }, 0.0f, 600.0f},
    {nullptr, "wave.every_s", [](Tuning& t) -> float& { return t.rules.wave.every_s; }, 1.0f,
     600.0f},
    {nullptr, "wave.height_spread_u",
     [](Tuning& t) -> float& { return t.rules.wave.height_spread_u; }, 0.0f, 500.0f},
    {nullptr, "wave.spread_u", [](Tuning& t) -> float& { return t.rules.wave.spread_u; }, 0.0f,
     1000.0f},
    {nullptr, "wave.north_inset_u", [](Tuning& t) -> float& { return t.rules.wave.north_inset_u; },
     0.0f, 1000.0f},
}};

constexpr std::array<IntKey, 37> kInts = {{
    {"Waves: enemy fighters in the air at once, at most", "wave.max_alive",
     [](Tuning& t) -> int& { return t.rules.wave.max_alive; }, 1, 64},
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
     2048},
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
