#include "game/bindings.hpp"

#include <cstddef>

namespace wof::game {

// core/settings.hpp keeps scancodes as numbers; they must be SDL's.
static_assert(SDL_SCANCODE_ESCAPE == kScanEscape && SDL_SCANCODE_DELETE == kScanDelete);
static_assert(SDL_SCANCODE_F1 == kScanF1 && SDL_SCANCODE_F12 == kScanF12);
static_assert(SDL_SCANCODE_A == 4 && SDL_SCANCODE_D == 7 && SDL_SCANCODE_W == 26 &&
              SDL_SCANCODE_S == 22 && SDL_SCANCODE_F == 9 && SDL_SCANCODE_SPACE == 44 &&
              SDL_SCANCODE_TAB == 43 && SDL_SCANCODE_LSHIFT == 225 && SDL_SCANCODE_RSHIFT == 229 &&
              SDL_SCANCODE_LCTRL == 224 && SDL_SCANCODE_RCTRL == 228 && SDL_SCANCODE_N == 17);

namespace {

[[nodiscard]] bool held(const KeyBindings& b, Action a, const bool* keys) noexcept {
    const KeyPair& k = b[static_cast<std::size_t>(a)];
    return (k.main != kNoKey && keys[k.main]) || (k.alt != kNoKey && keys[k.alt]);
}

// -127, 0 or 127 from a pair of opposing keys.
[[nodiscard]] std::int8_t axis_of(bool negative, bool positive) noexcept {
    return static_cast<std::int8_t>((positive ? kAxisFull : 0) - (negative ? kAxisFull : 0));
}

} // namespace

bool is_key(const KeyBindings& b, Action action, SDL_Scancode key) noexcept {
    const KeyPair& k = b[static_cast<std::size_t>(action)];
    const int code = static_cast<int>(key);
    return code != kNoKey && (k.main == code || k.alt == code);
}

InputCommand read_command(const Settings& s, const bool* keys, bool switch_pressed,
                          bool next_pressed) noexcept {
    const KeyBindings& b = s.keys;
    InputCommand c;
    c.turn = axis_of(held(b, Action::TurnLeft, keys), held(b, Action::TurnRight, keys));
    c.pitch = axis_of(held(b, Action::Dive, keys), held(b, Action::Climb, keys));
    c.throttle = axis_of(held(b, Action::ThrottleDown, keys), held(b, Action::ThrottleUp, keys));
    c.guns = held(b, Action::Guns, keys);
    c.heavy = held(b, Action::Heavy, keys);
    c.switch_heavy = switch_pressed;
    c.next_wave = next_pressed;
    c.level = s.auto_level;
    return c;
}

} // namespace wof::game
