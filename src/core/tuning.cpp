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
constexpr std::array<FloatKey, 6> kFloats = {{
    {"Camera: how far ahead it looks (u), and how fast that swings round after a turn (per s)",
     "camera.lead_u", [](Tuning& t) -> float& { return t.camera.lead_u; }, 0.0f, 400.0f},
    {nullptr, "camera.lead_ease", [](Tuning& t) -> float& { return t.camera.lead_ease; }, 0.1f,
     60.0f},
    {"Stand-in plane (PR 1 only: no physics; real flight replaces it)", "stand_in.speed_u_s",
     [](Tuning& t) -> float& { return t.stand_in.speed_u_s; }, 0.0f, 400.0f},
    {nullptr, "stand_in.turn_deg_s", [](Tuning& t) -> float& { return t.stand_in.turn_deg_s; },
     0.0f, 720.0f},
    {nullptr, "stand_in.climb_u_s", [](Tuning& t) -> float& { return t.stand_in.climb_u_s; }, 0.0f,
     200.0f},
    {nullptr, "stand_in.ceiling_u", [](Tuning& t) -> float& { return t.stand_in.ceiling_u; }, 1.0f,
     2000.0f},
}};

constexpr std::array<IntKey, 18> kInts = {{
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
    out += "# Every number is a (start) value; see design/flight.md.\n";
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
    return errors.size() == errors_before;
}

} // namespace wof
