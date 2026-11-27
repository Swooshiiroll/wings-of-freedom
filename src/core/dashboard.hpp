#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "core/body.hpp"
#include "core/canvas.hpp"
#include "core/damage.hpp"
#include "core/planes.hpp"
#include "core/units.hpp"

namespace wof {

// The cockpit dashboard (design/dashboard.md): a panel across the bottom of the screen, drawn from
// a plain state. Styles and instruments follow the aircraft's era; v0.1 has the pre-war panels of
// the US and Germany, each reading its own units. No SDL: the front end uploads the pixels.

enum class PanelSize : std::uint8_t { Large, Medium, Small };
inline constexpr int kPanelSizes = 3;
inline constexpr int kPanelW = 480;

[[nodiscard]] constexpr int panel_height(PanelSize s) noexcept {
    return s == PanelSize::Large ? 96 : s == PanelSize::Medium ? 80 : 64;
}

enum class Instrument : std::uint8_t {
    Airspeed,
    Altimeter,
    Compass,
    TurnBank,
    Climb,
    Pitch,
    Tach,
    Fuel,
    Oil,
    Clock,
    Ammo,
    Bombs,
    Lamps,
    Message,
    Damage,
};
inline constexpr std::size_t kInstruments = 15;

// Where everything sits on a panel kPanelW wide and the size's height.
struct DashLayout {
    int height = 96;
    std::array<Rect, kInstruments> rects{};

    [[nodiscard]] const Rect& of(Instrument i) const noexcept {
        return rects[static_cast<std::size_t>(i)];
    }
};
[[nodiscard]] DashLayout dash_layout(PanelSize size) noexcept;

// Needle angles in degrees, 0 straight up, clockwise.
[[nodiscard]] float airspeed_angle(float mph, UnitSystem units) noexcept; // 300 mph / 500 km/h
struct AltimeterAngles {
    float thousands = 0.0f; // once round: 30,000 ft or 10,000 m
    float hundreds = 0.0f;  // once round: 1,000 ft or 1,000 m
};
[[nodiscard]] AltimeterAngles altimeter_angles(float height_u, UnitSystem units) noexcept;
[[nodiscard]] float turn_angle(float turn_deg_s) noexcept; // 120°/s is full needle
// Climbing and diving as the altimeter changes: zero at 9 o'clock, up clockwise. Tight near zero,
// wide at the ends, stopping there (160,000 ft/min or 800 m/s).
[[nodiscard]] float climb_angle(float vz_u_s, UnitSystem units) noexcept;
[[nodiscard]] float pitch_gauge_angle(float pitch_deg) noexcept; // level at 3 o'clock
[[nodiscard]] float engine_rpm(float throttle, bool running) noexcept;
[[nodiscard]] float rpm_angle(float rpm) noexcept;     // 0 to 3,000
[[nodiscard]] float gauge_angle(float share) noexcept; // fuel and oil: empty to full

// What the damage wireframe and the lamps colour a stage.
[[nodiscard]] Colour stage_tint(Stage s) noexcept;

struct AmmoCounter {
    std::string label; // ".30"
    int rounds = 0;
};

// How loud the message strip is.
enum class Tone : std::uint8_t { Status, Notice, Warning };

// Everything the panel shows.
struct PanelState {
    Nation nation = Nation::US;
    float speed_mph = 0.0f;
    float height_u = 0.0f;
    float heading_deg = 0.0f;
    float turn_deg_s = 0.0f;
    float vz_u_s = 0.0f;
    float pitch_deg = 0.0f;
    float assist_deg = 5.0f;
    bool assisted = false;
    float throttle = 0.0f;
    int engines = 1;
    std::array<float, 2> engine_share{1.0f, 1.0f}; // HP left, 0..1
    std::array<bool, 2> engine_running{true, true};
    float fuel = 1.0f;
    int clock_h = 0;
    int clock_m = 0;
    float run_s = 0.0f;
    std::array<AmmoCounter, 3> ammo{};
    int ammo_count = 0;
    int bombs = 0;
    int bombs_full = 0;
    bool stall = false;
    bool low_fuel = false;
    bool edge = false;
    bool going_down = false;
    bool own_blast = false;
    std::string message;
    Tone tone = Tone::Status;
    std::array<Stage, kParts> stages{}; // by Part
    ModelId model = ModelId::P26;
    std::uint32_t tick = 0; // for the lamps' flash and the oil needle's flutter
};

// Draws the panel onto a canvas kPanelW x the layout's height.
void draw_panel(Canvas& c, const PanelState& s, const DashLayout& layout);

// What the message strip says, loudest first: the edge countdown, an own blast, going down, the
// area edge, a notice (a wave, an enemy down) while it lasts, else the status line.
struct MessageInput {
    float edge_countdown_s = -1.0f;
    float own_blast_s = 0.0f;
    float own_blast_damage = 0.0f;
    bool going_down = false;
    bool edge_warning = false;
    std::string notice;
    float notice_s = 0.0f;
    std::string status;
};
struct Message {
    std::string text;
    Tone tone = Tone::Status;
};
[[nodiscard]] Message panel_message(const MessageInput& in);

} // namespace wof
