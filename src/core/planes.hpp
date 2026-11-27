#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace wof {

// Real planes (design/planes.md): every plane is a real aircraft. Its real stats are data here;
// the scale (PlaneScale, tuning.txt `scale.*`) turns them into game values, keeping their real
// proportions ("real readouts, scaled action").

enum class Nation : std::uint8_t { US, UK, Germany, Italy, Japan, USSR };
[[nodiscard]] const char* nation_name(Nation n) noexcept;

enum class Role : std::uint8_t { Fighter, Bomber };

// Gun calibres, which set the damage a hit does (WeaponParams::calibres).
enum class Calibre : std::uint8_t { Rifle, Heavy, Cannon20 }; // .30 / 7.92 mm, .50 / 12.7 mm, 20 mm
inline constexpr int kCalibres = 3;
[[nodiscard]] const char* calibre_name(Calibre c) noexcept; // ".30", ".50", "20mm"

// Where a gun is: fixed, firing along the nose (the pilot's), or a gunner's position.
enum class Mount : std::uint8_t { Fixed, Nose, Dorsal, Ventral };

struct GunSpec {
    Calibre calibre = Calibre::Rifle;
    float rpm = 1000.0f; // real rounds per minute
    int rounds = 500;    // real ammunition
    float side_u = 0.0f; // fixed guns: offset from the nose, toward the right wing
    Mount mount = Mount::Fixed;

    friend constexpr bool operator==(const GunSpec&, const GunSpec&) = default;
};
inline constexpr int kMaxGuns = 4;

// Which blocky model a plane is drawn with (core/fighter.hpp).
enum class ModelId : std::uint8_t { P26, He51 };

// The planes in the roster, in order (Body::type).
enum class PlaneId : std::uint8_t { P26A, He51B, B10B, He111B2 };
inline constexpr int kPlaneTypes = 4;

// One real aircraft: its real stats, and how it's armed, built and drawn.
struct PlaneType {
    std::string_view id;   // for the command line: p26a, he51b, b10b, he111b
    std::string_view name; // as shown
    Nation nation = Nation::US;
    Role role = Role::Fighter;
    int year = 1934;
    float top_mph = 0.0f;     // real top speed
    float climb_fpm = 0.0f;   // real initial climb, feet per minute
    float stall_mph = 0.0f;   // real stall speed
    float turn_s = 0.0f;      // real sustained full circle, seconds
    float endurance_h = 0.0f; // real endurance
    float pitch_rate_deg_s = 60.0f;
    int engines = 1;
    std::array<GunSpec, kMaxGuns> guns{};
    int gun_count = 0;
    float bomb_lb = 0.0f; // each bomb's weight
    int bombs = 0;
    float engine_hp = 40.0f; // each engine
    float wing_hp = 40.0f;
    float fuselage_hp = 60.0f;
    float tail_hp = 20.0f;
    ModelId model = ModelId::P26;
};

// The roster: every plane in the game, by PlaneId.
[[nodiscard]] const PlaneType& plane_type(PlaneId id) noexcept;
[[nodiscard]] const PlaneType& plane_type(int index) noexcept; // clamped into the roster
bool find_plane(std::string_view id, PlaneId& out) noexcept;

// A nation's fighter in v0.1 (US: P-26A, Germany: He 51B).
[[nodiscard]] PlaneId fighter_of(Nation n) noexcept;

// The scale from real stats to game values (tuning.txt `scale.*`), all (start) values.
struct PlaneScale {
    float u_per_mph = 0.641f;        // the P-26A's real 234 mph is today's 150 u/s
    float turn_factor = 4.44f;       // the P-26A's (est.) 16 s full circle takes 3.6 s
    float u_per_fpm = 0.00636f;      // the P-26A's 2,360 ft/min is 15 u/s
    float dive_factor = 1.6f;        // dive limit, as a multiple of top speed
    float tank_s_per_hour = 266.67f; // the P-26A's 1.8 h is a 480 s (8 minute) tank
    float best_turn_share = 0.73f;   // best turn speed, as a share of top speed
    float stall_margin_u_s = 8.0f;   // a stall ends this far above the stall speed

    friend constexpr bool operator==(const PlaneScale&, const PlaneScale&) = default;
};

// Game units to the readouts: u/s to mph.
[[nodiscard]] constexpr float mph_of(float u_s, const PlaneScale& s) noexcept {
    return s.u_per_mph > 0.0f ? u_s / s.u_per_mph : 0.0f;
}

} // namespace wof
