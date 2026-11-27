#include "core/dashboard.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/fighter.hpp"
#include "core/pixel_font.hpp"
#include "core/wireframe.hpp"

namespace wof {
namespace {

constexpr float kToRad = std::numbers::pi_v<float> / 180.0f;
constexpr float kClimbMaxFtMin = 160000.0f;
constexpr float kClimbMaxMs = 800.0f;

// A panel's colours. The pre-war US panel is olive drab with brass-rimmed black dials; the
// German one RLM grey with dark rims.
struct PanelStyle {
    Colour panel;
    Colour edge;
    Colour face;
    Colour rim;
    Colour ink;
    Colour dim;
    Colour needle;
    Colour accent;
};

[[nodiscard]] PanelStyle style_for(Nation n) noexcept {
    if (nation_units(n) == UnitSystem::Imperial) {
        return {rgb(78, 82, 54),    rgb(40, 42, 28),   rgb(18, 18, 16),    rgb(150, 138, 96),
                rgb(230, 226, 204), rgb(118, 116, 98), rgb(250, 250, 240), rgb(240, 188, 60)};
    }
    return {rgb(112, 116, 108), rgb(62, 66, 60),    rgb(14, 16, 16),    rgb(46, 50, 48),
            rgb(216, 224, 214), rgb(104, 112, 104), rgb(236, 236, 226), rgb(226, 84, 60)};
}

struct Dial {
    float cx = 0.0f;
    float cy = 0.0f;
    float r = 0.0f;
};

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

[[nodiscard]] Point polar(const Dial& d, float angle_deg, float radius) noexcept {
    const float a = angle_deg * kToRad;
    return {d.cx + std::sin(a) * radius, d.cy - std::cos(a) * radius};
}

// A round face with a rim.
Dial face(Canvas& c, Rect r, const PanelStyle& st) {
    const Dial d{static_cast<float>(r.x) + static_cast<float>(r.w) * 0.5f,
                 static_cast<float>(r.y) + static_cast<float>(r.h) * 0.5f,
                 static_cast<float>(std::min(r.w, r.h)) * 0.5f};
    c.disc(d.cx, d.cy, d.r, st.rim);
    c.disc(d.cx, d.cy, d.r - 1.5f, st.face);
    return d;
}

void tick(Canvas& c, const Dial& d, float angle, float length, Colour colour) {
    const Point a = polar(d, angle, d.r - 2.5f);
    const Point b = polar(d, angle, d.r - 2.5f - length);
    c.line(a.x, a.y, b.x, b.y, colour);
}

void label(Canvas& c, const Dial& d, float angle, float radius, std::string_view text,
           Colour colour) {
    const Point p = polar(d, angle, radius);
    c.text_centred(p.x, static_cast<int>(std::floor(p.y)) - 2, text, colour);
}

// The name under the centre, if the dial has room.
void name(Canvas& c, const Dial& d, std::string_view text, Colour colour) {
    c.text_centred(d.cx, static_cast<int>(d.cy + d.r * 0.28f), text, colour);
}

void needle(Canvas& c, const Dial& d, float angle, float length, Colour colour) {
    const Point tip = polar(d, angle, length);
    const Point tail = polar(d, angle + 180.0f, length * 0.2f);
    c.line(tail.x, tail.y, tip.x, tip.y, colour);
    if (d.r >= 18.0f) {
        const Point side = polar({0.0f, 0.0f, 0.0f}, angle + 90.0f, 0.8f);
        c.line(d.cx + side.x, d.cy + side.y, tip.x, tip.y, colour);
    }
}

void hub(Canvas& c, const Dial& d, const PanelStyle& st) {
    c.disc(d.cx, d.cy, d.r >= 18.0f ? 2.5f : 1.5f, st.rim);
}

[[nodiscard]] bool labelled(const Dial& d) noexcept {
    return d.r >= 18.0f; // room for numbers round the face (96 and 80 px panels)
}

[[nodiscard]] bool roomy(const Dial& d) noexcept {
    return d.r >= 22.0f; // the 96 px panel's big dials
}

void draw_airspeed(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    const Dial d = face(c, r, st);
    const UnitSystem u = nation_units(s.nation);
    const bool imperial = u == UnitSystem::Imperial;
    const int step = imperial ? 50 : 100;
    const int top = imperial ? 300 : 500;
    for (int v = 0; v <= top; v += step) {
        const float mph = imperial ? static_cast<float>(v) : static_cast<float>(v) / kKmhPerMph;
        const float a = airspeed_angle(mph, u);
        tick(c, d, a, 2.0f, st.ink);
        if (labelled(d) && v % (step * 2) == 0 && v > 0 && (roomy(d) || v < top)) {
            label(c, d, a, d.r - 9.5f, std::to_string(v), st.ink);
        }
    }
    name(c, d, imperial ? "MPH" : "KM/H", st.dim);
    needle(c, d, airspeed_angle(s.speed_mph, u), d.r - 3.0f, st.needle);
    hub(c, d, st);
}

void draw_altimeter(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    const Dial d = face(c, r, st);
    const UnitSystem u = nation_units(s.nation);
    const bool imperial = u == UnitSystem::Imperial;
    const int top = imperial ? 30 : 10; // thousands of feet or metres
    const int every = imperial ? 5 : 2;
    for (int v = 0; v < top; ++v) {
        const float a = static_cast<float>(v) / static_cast<float>(top) * 360.0f;
        if (v % every == 0) {
            tick(c, d, a, 2.0f, st.ink);
            if (labelled(d) && (roomy(d) || v % (every * 2) == 0)) {
                label(c, d, a, d.r - 8.0f, std::to_string(v), st.ink);
            }
        } else if (!imperial || d.r >= 18.0f) {
            tick(c, d, a, 0.5f, st.dim);
        }
    }
    name(c, d, imperial ? "FT" : "M", st.dim);
    const AltimeterAngles a = altimeter_angles(s.height_u, u);
    needle(c, d, a.hundreds, d.r - 3.0f, st.needle);
    needle(c, d, a.thousands, d.r * 0.55f, st.accent);
    hub(c, d, st);
}

void draw_compass(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    const Dial d = face(c, r, st);
    const float card = -s.heading_deg; // the card turns under the lubber mark
    for (int a = 0; a < 360; a += 30) {
        if (a % 90 != 0) {
            tick(c, d, card + static_cast<float>(a), 2.0f, st.dim);
        }
    }
    const float lr = std::max(3.0f, d.r - 6.0f);
    label(c, d, card, lr, "N", st.accent);
    label(c, d, card + 90.0f, lr, "E", st.ink);
    label(c, d, card + 180.0f, lr, "S", st.ink);
    label(c, d, card + 270.0f, lr, "W", st.ink);
    // The lubber mark: a small pointer at the top of the rim, clear of the letters.
    c.line(d.cx - 2.0f, d.cy - d.r, d.cx, d.cy - d.r + 2.5f, st.needle);
    c.line(d.cx + 2.0f, d.cy - d.r, d.cx, d.cy - d.r + 2.5f, st.needle);
}

void draw_turn_bank(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    const Dial d = face(c, r, st);
    for (const float a : {-45.0f, 0.0f, 45.0f}) {
        tick(c, d, a, 3.0f, st.ink);
    }
    needle(c, d, turn_angle(s.turn_deg_s), d.r - 3.0f, st.needle);
    hub(c, d, st);
    // The ball in its tube: centred in a coordinated turn, thrown about in a stall or a spin.
    const float half = d.r * 0.5f;
    const float ty = d.cy + d.r * 0.5f;
    const Rect tube{static_cast<int>(d.cx - half), static_cast<int>(ty) - 2,
                    static_cast<int>(half * 2.0f) + 1, 5};
    c.fill(tube, st.rim);
    float ball = 0.0f;
    if (s.going_down) {
        ball = s.turn_deg_s < 0.0f ? 1.0f : -1.0f;
    } else if (s.stall) {
        ball = std::sin(static_cast<float>(s.tick) * 0.25f);
    }
    c.disc(d.cx + ball * (half - 2.0f), ty + 0.5f, 2.0f, st.needle);
}

void draw_climb(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    const Dial d = face(c, r, st);
    const UnitSystem u = nation_units(s.nation);
    const bool imperial = u == UnitSystem::Imperial;
    // u/s for a dial figure: thousands of feet a minute, or metres a second.
    const auto vz_of = [imperial](float v) {
        return imperial ? v * 1000.0f / 60.0f / kFeetPerU : v / kMetresPerFoot / kFeetPerU;
    };
    const std::array<float, 4> marks = imperial ? std::array<float, 4>{0.0f, 10.0f, 40.0f, 160.0f}
                                                : std::array<float, 4>{0.0f, 50.0f, 200.0f, 800.0f};
    for (const float m : marks) {
        for (const float sign : {1.0f, -1.0f}) {
            tick(c, d, climb_angle(vz_of(m * sign), u), 2.0f, st.ink);
        }
    }
    if (labelled(d)) {
        label(c, d, -90.0f, d.r - 7.0f, "0", st.ink);
        const std::string mid = std::to_string(static_cast<int>(marks[2]));
        label(c, d, climb_angle(vz_of(marks[2]), u), d.r - 8.0f, mid, st.ink);
        label(c, d, climb_angle(vz_of(-marks[2]), u), d.r - 8.0f, mid, st.ink);
    }
    if (roomy(d)) {
        c.text_centred(d.cx + d.r * 0.35f, static_cast<int>(d.cy + d.r * 0.2f),
                       imperial ? "KFPM" : "M/S", st.dim);
    }
    needle(c, d, climb_angle(s.vz_u_s, u), d.r - 3.0f, st.needle);
    hub(c, d, st);
}

void draw_pitch(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    const Dial d = face(c, r, st);
    if (s.assisted) {
        // Assisted auto-leveling's zone, round the rim at level.
        for (float p = -s.assist_deg; p <= s.assist_deg; p += 0.5f) {
            const Point a = polar(d, pitch_gauge_angle(p), d.r - 3.0f);
            c.set(static_cast<int>(a.x), static_cast<int>(a.y), rgb(90, 180, 255));
            const Point b = polar(d, pitch_gauge_angle(p), d.r - 4.0f);
            c.set(static_cast<int>(b.x), static_cast<int>(b.y), rgb(90, 180, 255));
        }
    }
    for (int p = -40; p <= 40; p += 10) {
        tick(c, d, pitch_gauge_angle(static_cast<float>(p)), p == 0 ? 4.0f : 2.0f, st.ink);
    }
    if (labelled(d)) {
        label(c, d, pitch_gauge_angle(40.0f), d.r - 10.0f, "UP", st.dim);
        label(c, d, pitch_gauge_angle(-40.0f), d.r - 10.0f, "DN", st.dim);
    }
    needle(c, d, pitch_gauge_angle(s.pitch_deg), d.r - 3.0f, st.needle);
    hub(c, d, st);
}

void draw_tach(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    const Dial d = face(c, r, st);
    for (int v = 0; v <= 3000; v += 500) {
        tick(c, d, rpm_angle(static_cast<float>(v)), v % 1000 == 0 ? 2.0f : 1.0f, st.ink);
    }
    if (d.r >= 12.0f) {
        name(c, d, "RPM", st.dim);
    }
    for (int e = 0; e < std::min(s.engines, 2); ++e) {
        const auto i = static_cast<std::size_t>(e);
        needle(c, d, rpm_angle(engine_rpm(s.throttle, s.engine_running[i])), d.r - 3.0f,
               e == 0 ? st.needle : st.accent);
    }
    hub(c, d, st);
}

// Fuel and oil: a needle from empty (left) to full (right).
void draw_half_gauge(Canvas& c, Rect r, std::string_view title, const float* values, int count,
                     const PanelStyle& st) {
    const Dial d = face(c, r, st);
    for (const float v : {0.0f, 0.5f, 1.0f}) {
        tick(c, d, gauge_angle(v), 2.0f, st.ink);
    }
    // The empty end in red.
    for (float v = 0.0f; v <= 0.2f; v += 0.02f) {
        const Point p = polar(d, gauge_angle(v), d.r - 2.5f);
        c.set(static_cast<int>(p.x), static_cast<int>(p.y), rgb(220, 70, 50));
    }
    name(c, d, title, st.dim);
    for (int i = 0; i < count; ++i) {
        needle(c, d, gauge_angle(values[i]), d.r - 3.0f, i == 0 ? st.needle : st.accent);
    }
    hub(c, d, st);
}

void draw_clock(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    const Dial d = face(c, r, st);
    for (int h = 0; h < 12; ++h) {
        tick(c, d, static_cast<float>(h) * 30.0f, h % 3 == 0 ? 2.0f : 0.5f, st.ink);
    }
    // The inner dial: time since the run started (minutes in the accent colour, seconds).
    const Dial inner{d.cx, d.cy + d.r * 0.4f, d.r * 0.36f};
    c.ring(inner.cx, inner.cy, inner.r, st.dim);
    const float seconds = std::fmod(s.run_s, 60.0f);
    const float minutes = std::fmod(s.run_s / 60.0f, 60.0f);
    const Point sp = polar(inner, seconds * 6.0f, inner.r);
    c.line(inner.cx, inner.cy, sp.x, sp.y, st.ink);
    const Point mp = polar(inner, minutes * 6.0f, inner.r * 0.7f);
    c.line(inner.cx, inner.cy, mp.x, mp.y, st.accent);
    // The main hands: the real time.
    const float hour = static_cast<float>(s.clock_h % 12) + static_cast<float>(s.clock_m) / 60.0f;
    needle(c, d, hour * 30.0f, d.r * 0.5f, st.accent);
    needle(c, d, static_cast<float>(s.clock_m) * 6.0f, d.r - 3.0f, st.needle);
    hub(c, d, st);
}

void draw_ammo(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    const int n = std::max(1, s.ammo_count);
    const int row_h = std::min(11, r.h / n);
    const int top = r.y + (r.h - row_h * n) / 2;
    for (int i = 0; i < s.ammo_count; ++i) {
        const AmmoCounter& a = s.ammo[static_cast<std::size_t>(i)];
        const int y = top + i * row_h;
        c.text(r.x, y + (row_h - kGlyphH) / 2, a.label, st.ink);
        // Four drums of one digit each.
        const std::string digits = std::to_string(10000 + std::clamp(a.rounds, 0, 9999)).substr(1);
        const int x0 = r.x + 20;
        for (int k = 0; k < 4; ++k) {
            const Rect drum{x0 + k * 6, y + (row_h - 9) / 2, 5, std::min(9, row_h - 1)};
            c.fill(drum, st.face);
            c.text(drum.x + 1, drum.y + (drum.h - kGlyphH) / 2,
                   digits.substr(static_cast<std::size_t>(k), 1),
                   a.rounds > 0 ? st.needle : rgb(220, 70, 50));
        }
    }
}

void draw_bombs(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    if (s.bombs_full <= 0) {
        return;
    }
    const int per_row = std::max(1, std::min(s.bombs_full, 6));
    const float gap = static_cast<float>(r.w) / static_cast<float>(per_row);
    const bool titled = r.h >= 24;
    if (titled) {
        c.text_centred(static_cast<float>(r.x) + static_cast<float>(r.w) * 0.5f, r.y + 3, "BOMBS",
                       st.ink);
    }
    const float y = static_cast<float>(r.y) + static_cast<float>(r.h) * (titled ? 0.65f : 0.5f);
    for (int i = 0; i < std::min(s.bombs_full, 6); ++i) {
        const float x = static_cast<float>(r.x) + gap * (static_cast<float>(i) + 0.5f);
        c.disc(x, y, 3.0f, st.face);
        if (i < s.bombs) {
            c.disc(x, y, 2.0f, st.accent);
        }
    }
}

struct Lamp {
    std::string_view text;
    bool lit;
    Colour colour;
};

void draw_lamps(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    bool engine_out = false;
    for (int e = 0; e < std::min(s.engines, 2); ++e) {
        const auto i = static_cast<std::size_t>(e);
        engine_out = engine_out || !s.engine_running[i];
    }
    const Colour amber = rgb(244, 176, 40);
    const Colour red = rgb(232, 60, 44);
    const std::array<Lamp, 6> lamps = {{{"STALL", s.stall, amber},
                                        {"ENGINE", engine_out, red},
                                        {"FUEL", s.low_fuel, amber},
                                        {"EDGE", s.edge, amber},
                                        {"DOWN", s.going_down, red},
                                        {"BLAST", s.own_blast, red}}};
    const int w = r.w / 3;
    const int h = r.h / 2;
    for (int i = 0; i < 6; ++i) {
        const Lamp& l = lamps[static_cast<std::size_t>(i)];
        const Rect box{r.x + (i % 3) * w + 1, r.y + (i / 3) * h, w - 2, h - 1};
        c.fill(box, l.lit ? l.colour : st.face);
        c.text_centred(static_cast<float>(box.x) + static_cast<float>(box.w) * 0.5f,
                       box.y + (box.h - kGlyphH) / 2, l.text, l.lit ? rgb(20, 14, 8) : st.dim);
    }
}

void draw_damage(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    c.fill(r, st.face);
    c.frame(r, st.rim);
    const float cx = static_cast<float>(r.x) + static_cast<float>(r.w) * 0.5f;
    const float cy = static_cast<float>(r.y) + static_cast<float>(r.h) * 0.5f;
    const float scale = static_cast<float>(std::min(r.w, r.h) - 8) / 28.0f; // about a 28 u span
    const std::vector<WireLine> lines = top_wireframe(plane_model(s.model));
    float cockpit_y = 0.0f;
    int cockpit_n = 0;
    for (const WireLine& l : lines) {
        const Stage stage = s.stages[static_cast<std::size_t>(l.part)];
        if (l.part == Part::Cockpit) {
            cockpit_y += l.y0;
            ++cockpit_n;
            continue; // the pilot is a mark of its own
        }
        if (stage == Stage::Destroyed) {
            continue;
        }
        // The nose points up; the model's x (right wing) goes right.
        c.line(cx + l.x0 * scale, cy - l.y0 * scale, cx + l.x1 * scale, cy - l.y1 * scale,
               stage_tint(stage));
    }
    if (cockpit_n > 0) {
        const float y = cy - cockpit_y / static_cast<float>(cockpit_n) * scale;
        c.disc(cx, y, std::max(1.5f, scale * 0.9f),
               stage_tint(s.stages[static_cast<std::size_t>(Part::Cockpit)]));
    }
}

void draw_message(Canvas& c, Rect r, const PanelState& s, const PanelStyle& st) {
    c.fill(r, st.face);
    const Colour colour = s.tone == Tone::Warning  ? rgb(255, 110, 80)
                          : s.tone == Tone::Notice ? rgb(244, 188, 60)
                                                   : st.ink;
    c.text_centred(static_cast<float>(r.x) + static_cast<float>(r.w) * 0.5f,
                   r.y + (r.h - kGlyphH) / 2, s.message, colour);
}

} // namespace

DashLayout dash_layout(PanelSize size) noexcept {
    DashLayout l;
    l.height = panel_height(size);
    const int big = size == PanelSize::Large ? 48 : size == PanelSize::Medium ? 38 : 30;
    const int small = size == PanelSize::Large ? 30 : size == PanelSize::Medium ? 24 : 18;
    const auto at = [&l](Instrument i) -> Rect& { return l.rects[static_cast<std::size_t>(i)]; };

    // The damage box fills the right end.
    const int damage = l.height - 4;
    const int damage_x = kPanelW - damage - 2;
    at(Instrument::Damage) = {damage_x, 2, damage, damage};

    // Six big dials across the top.
    constexpr std::array<Instrument, 6> kTop = {Instrument::Airspeed, Instrument::Altimeter,
                                                Instrument::Compass,  Instrument::TurnBank,
                                                Instrument::Climb,    Instrument::Pitch};
    const int cell = (damage_x - 4) / 6;
    for (int i = 0; i < 6; ++i) {
        at(kTop[static_cast<std::size_t>(i)]) = {2 + i * cell + (cell - big) / 2, 2, big, big};
    }
    // Below: four small dials, then the ammunition, bombs and lamps.
    const int y = 2 + big + 2;
    constexpr std::array<Instrument, 4> kSmall = {Instrument::Tach, Instrument::Fuel,
                                                  Instrument::Oil, Instrument::Clock};
    for (int i = 0; i < 4; ++i) {
        at(kSmall[static_cast<std::size_t>(i)]) = {4 + i * (small + 4), y, small, small};
    }
    const int x = 4 + 4 * (small + 4) + 2;
    at(Instrument::Ammo) = {x, y, 46, small};
    at(Instrument::Bombs) = {x + 50, y, 40, small};
    at(Instrument::Lamps) = {x + 94, y, damage_x - 2 - (x + 94), small};
    at(Instrument::Message) = {2, l.height - 11, damage_x - 4, 9};
    return l;
}

float airspeed_angle(float mph, UnitSystem units) noexcept {
    const float v = units == UnitSystem::Imperial ? mph / 300.0f : mph * kKmhPerMph / 500.0f;
    return -150.0f + 300.0f * std::clamp(v, 0.0f, 1.0f);
}

AltimeterAngles altimeter_angles(float height_u, UnitSystem units) noexcept {
    const float h = std::max(0.0f, height_in(height_u, units));
    const float once = units == UnitSystem::Imperial ? 30000.0f : 10000.0f;
    return {std::min(h / once, 1.0f) * 360.0f, std::fmod(h, 1000.0f) / 1000.0f * 360.0f};
}

float turn_angle(float turn_deg_s) noexcept {
    return std::clamp(turn_deg_s / 120.0f, -1.0f, 1.0f) * 45.0f;
}

float climb_angle(float vz_u_s, UnitSystem units) noexcept {
    const float feet_s = vz_u_s * kFeetPerU;
    const float share = units == UnitSystem::Imperial
                            ? std::abs(feet_s * 60.0f) / kClimbMaxFtMin
                            : std::abs(feet_s * kMetresPerFoot) / kClimbMaxMs;
    const float swing = std::sqrt(std::min(share, 1.0f)) * 170.0f;
    return -90.0f + (vz_u_s >= 0.0f ? swing : -swing);
}

float pitch_gauge_angle(float pitch_deg) noexcept {
    return 90.0f - std::clamp(pitch_deg, -40.0f, 40.0f) * 1.25f;
}

float engine_rpm(float throttle, bool running) noexcept {
    return running ? 600.0f + 1800.0f * std::clamp(throttle, 0.0f, 1.0f) : 0.0f;
}

float rpm_angle(float rpm) noexcept {
    return -150.0f + 300.0f * std::clamp(rpm / 3000.0f, 0.0f, 1.0f);
}

float gauge_angle(float share) noexcept {
    return -60.0f + 120.0f * std::clamp(share, 0.0f, 1.0f);
}

Colour stage_tint(Stage s) noexcept {
    switch (s) {
    case Stage::Ok:
        return rgb(100, 206, 96);
    case Stage::Warning:
        return rgb(236, 214, 70);
    case Stage::Danger:
        return rgb(242, 138, 46);
    case Stage::Critical:
        return rgb(234, 62, 46);
    case Stage::Destroyed:
        return rgb(70, 70, 70);
    }
    return rgb(255, 255, 255);
}

void draw_panel(Canvas& c, const PanelState& s, const DashLayout& layout) {
    const PanelStyle st = style_for(s.nation);
    c.clear(st.panel);
    c.fill({0, 0, c.width(), 1}, st.edge);

    draw_airspeed(c, layout.of(Instrument::Airspeed), s, st);
    draw_altimeter(c, layout.of(Instrument::Altimeter), s, st);
    draw_compass(c, layout.of(Instrument::Compass), s, st);
    draw_turn_bank(c, layout.of(Instrument::TurnBank), s, st);
    draw_climb(c, layout.of(Instrument::Climb), s, st);
    draw_pitch(c, layout.of(Instrument::Pitch), s, st);
    draw_tach(c, layout.of(Instrument::Tach), s, st);
    const float fuel = s.fuel;
    draw_half_gauge(c, layout.of(Instrument::Fuel), "FUEL", &fuel, 1, st);
    // Oil pressure falls with the engine's damage, and flutters when it's critical.
    std::array<float, 2> oil{};
    for (int e = 0; e < std::min(s.engines, 2); ++e) {
        const auto i = static_cast<std::size_t>(e);
        const float share = s.engine_running[i] ? s.engine_share[i] : 0.0f;
        float p = 0.7f * share;
        if (share > 0.0f && share < 0.25f) {
            p += 0.08f * std::sin(static_cast<float>(s.tick) * 0.5f + static_cast<float>(e));
        }
        oil[i] = p;
    }
    draw_half_gauge(c, layout.of(Instrument::Oil), "OIL", oil.data(), std::min(s.engines, 2), st);
    draw_clock(c, layout.of(Instrument::Clock), s, st);
    draw_ammo(c, layout.of(Instrument::Ammo), s, st);
    draw_bombs(c, layout.of(Instrument::Bombs), s, st);
    draw_lamps(c, layout.of(Instrument::Lamps), s, st);
    draw_message(c, layout.of(Instrument::Message), s, st);
    draw_damage(c, layout.of(Instrument::Damage), s, st);
}

Message panel_message(const MessageInput& in) {
    if (in.edge_countdown_s >= 0.0f) {
        return {"RETURN TO THE BATTLE AREA " +
                    std::to_string(static_cast<int>(std::ceil(in.edge_countdown_s))),
                Tone::Warning};
    }
    if (in.own_blast_s > 0.0f) {
        return {"OWN BLAST -" + std::to_string(static_cast<int>(std::lround(in.own_blast_damage))),
                Tone::Warning};
    }
    if (in.going_down) {
        return {"GOING DOWN", Tone::Warning};
    }
    if (in.edge_warning) {
        return {"EDGE OF THE BATTLE AREA", Tone::Notice};
    }
    if (in.notice_s > 0.0f && !in.notice.empty()) {
        return {in.notice, Tone::Notice};
    }
    return {in.status, Tone::Status};
}

} // namespace wof
