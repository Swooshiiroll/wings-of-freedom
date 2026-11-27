#include "core/settings.hpp"

#include "core/text.hpp"

namespace wof {
namespace {

// The actions' names in the file, in Action order.
constexpr std::array<const char*, kActions> kFileNames = {
    "turn_left",     "turn_right", "dive",  "climb",        "throttle_up",
    "throttle_down", "guns",       "heavy", "switch_heavy",
};

constexpr int kMaxScancode = 511;

[[nodiscard]] int& slot_of(KeyPair& pair, int slot) noexcept {
    return slot == 0 ? pair.main : pair.alt;
}

// "4 0" into a pair of scancodes.
[[nodiscard]] bool parse_pair(std::string_view value, KeyPair& out) {
    const std::string_view v = trim(value);
    const auto space = v.find_first_of(" \t");
    int main = 0;
    int alt = 0;
    if (!parse_int(v.substr(0, space), main) ||
        (space != std::string_view::npos && !parse_int(v.substr(space), alt))) {
        return false;
    }
    if (main < 0 || main > kMaxScancode || alt < 0 || alt > kMaxScancode || reserved_key(main) ||
        reserved_key(alt)) {
        return false;
    }
    out = {main, alt};
    return true;
}

} // namespace

const char* action_name(Action a) noexcept {
    switch (a) {
    case Action::TurnLeft:
        return "Turn left";
    case Action::TurnRight:
        return "Turn right";
    case Action::Dive:
        return "Dive";
    case Action::Climb:
        return "Climb";
    case Action::ThrottleUp:
        return "Throttle up";
    case Action::ThrottleDown:
        return "Throttle down";
    case Action::Guns:
        return "Fire guns";
    case Action::Heavy:
        return "Heavy weapon";
    case Action::SwitchHeavy:
        return "Switch heavy";
    }
    return "?";
}

KeyBindings default_keys() noexcept {
    KeyBindings k{};
    k[static_cast<std::size_t>(Action::TurnLeft)] = {4, kNoKey};     // A
    k[static_cast<std::size_t>(Action::TurnRight)] = {7, kNoKey};    // D
    k[static_cast<std::size_t>(Action::Dive)] = {26, kNoKey};        // W
    k[static_cast<std::size_t>(Action::Climb)] = {22, kNoKey};       // S
    k[static_cast<std::size_t>(Action::ThrottleUp)] = {225, 229};    // the Shifts
    k[static_cast<std::size_t>(Action::ThrottleDown)] = {224, 228};  // the Ctrls
    k[static_cast<std::size_t>(Action::Guns)] = {9, kNoKey};         // F
    k[static_cast<std::size_t>(Action::Heavy)] = {44, kNoKey};       // Space
    k[static_cast<std::size_t>(Action::SwitchHeavy)] = {43, kNoKey}; // Tab
    return k;
}

bool reserved_key(int key) noexcept {
    return key == kScanEscape || (key >= kScanF1 && key <= kScanF12);
}

bool bind_key(KeyBindings& keys, Action a, int slot, int key) noexcept {
    if (key <= kNoKey || key > kMaxScancode || reserved_key(key)) {
        return false;
    }
    for (KeyPair& pair : keys) {
        if (pair.main == key) {
            pair.main = kNoKey;
        }
        if (pair.alt == key) {
            pair.alt = kNoKey;
        }
    }
    slot_of(keys[static_cast<std::size_t>(a)], slot) = key;
    return true;
}

void clear_key(KeyBindings& keys, Action a, int slot) noexcept {
    slot_of(keys[static_cast<std::size_t>(a)], slot) = kNoKey;
}

const char* auto_level_name(AutoLevel level) noexcept {
    switch (level) {
    case AutoLevel::On:
        return "on";
    case AutoLevel::Assisted:
        return "assisted";
    case AutoLevel::Off:
        return "off";
    }
    return "?";
}

bool parse_auto_level(std::string_view text, AutoLevel& out) noexcept {
    for (int i = 0; i < kAutoLevels; ++i) {
        const auto level = static_cast<AutoLevel>(i);
        if (text == auto_level_name(level)) {
            out = level;
            return true;
        }
    }
    return false;
}

const char* units_name(Units u) noexcept {
    switch (u) {
    case Units::Nation:
        return "nation";
    case Units::Imperial:
        return "imperial";
    case Units::Metric:
        return "metric";
    }
    return "?";
}

std::string to_settings(const Settings& s) {
    std::string out = "# Wings of Freedom settings: written by the options menu.\n";
    out += std::string("auto_level = ") + auto_level_name(s.auto_level) + "\n";
    out += std::string("your_aim = ") + height_rule_name(s.your_aim) + "\n";
    out += std::string("enemy_aim = ") + height_rule_name(s.enemy_aim) + "\n";
    out += "panel_size = " + std::to_string(panel_height(s.panel)) + "\n";
    out += std::string("units = ") + units_name(s.units) + "\n";
    out += "# Keys: SDL scancode numbers, main and second (0 = none)\n";
    for (std::size_t i = 0; i < kActions; ++i) {
        out += std::string("key.") + kFileNames[i] + " = " + std::to_string(s.keys[i].main) + " " +
               std::to_string(s.keys[i].alt) + "\n";
    }
    return out;
}

bool from_settings(std::string_view text, Settings& s, std::string& errors) {
    const std::size_t errors_before = errors.size();
    KeyValueLines lines(text);
    while (lines.next()) {
        if (lines.bad()) {
            report_line(errors, lines.line(), "expected key = value");
            continue;
        }
        const std::string_view key = lines.key();
        const std::string_view value = lines.value();
        bool ok = false;
        if (key == "auto_level") {
            ok = parse_auto_level(value, s.auto_level);
        } else if (key == "your_aim") {
            ok = parse_height_rule(value, s.your_aim);
        } else if (key == "enemy_aim") {
            ok = parse_height_rule(value, s.enemy_aim);
        } else if (key == "panel_size") {
            for (int i = 0; i < kPanelSizes; ++i) {
                const auto size = static_cast<PanelSize>(i);
                if (value == std::to_string(panel_height(size))) {
                    s.panel = size;
                    ok = true;
                }
            }
        } else if (key == "units") {
            for (int i = 0; i < kUnits; ++i) {
                const auto u = static_cast<Units>(i);
                if (value == units_name(u)) {
                    s.units = u;
                    ok = true;
                }
            }
        } else if (key.substr(0, 4) == "key.") {
            std::size_t i = 0;
            while (i < kActions && key.substr(4) != kFileNames[i]) {
                ++i;
            }
            if (i == kActions) {
                report_line(errors, lines.line(), "unknown key " + std::string(key));
                continue;
            }
            ok = parse_pair(value, s.keys[i]);
        } else {
            report_line(errors, lines.line(), "unknown key " + std::string(key));
            continue;
        }
        if (!ok) {
            report_line(errors, lines.line(),
                        std::string(key) + ": bad value " + std::string(value));
        }
    }
    return errors.size() == errors_before;
}

} // namespace wof
