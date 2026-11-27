#include "game/bindings.hpp"

namespace wof::game {
namespace {

[[nodiscard]] const Binding& binding(const Bindings& b, Action a) noexcept {
    return b[static_cast<std::size_t>(a)];
}

[[nodiscard]] bool held(const Bindings& b, Action a, const bool* keys) noexcept {
    const Binding& k = binding(b, a);
    return (k.main != SDL_SCANCODE_UNKNOWN && keys[k.main]) ||
           (k.alt != SDL_SCANCODE_UNKNOWN && keys[k.alt]);
}

// -127, 0 or 127 from a pair of opposing keys.
[[nodiscard]] std::int8_t axis_of(bool negative, bool positive) noexcept {
    return static_cast<std::int8_t>((positive ? kAxisFull : 0) - (negative ? kAxisFull : 0));
}

} // namespace

Bindings default_bindings() noexcept {
    Bindings b{};
    b[static_cast<std::size_t>(Action::TurnLeft)] = {SDL_SCANCODE_A, SDL_SCANCODE_UNKNOWN};
    b[static_cast<std::size_t>(Action::TurnRight)] = {SDL_SCANCODE_D, SDL_SCANCODE_UNKNOWN};
    b[static_cast<std::size_t>(Action::Dive)] = {SDL_SCANCODE_W, SDL_SCANCODE_UNKNOWN};
    b[static_cast<std::size_t>(Action::Climb)] = {SDL_SCANCODE_S, SDL_SCANCODE_UNKNOWN};
    b[static_cast<std::size_t>(Action::ThrottleUp)] = {SDL_SCANCODE_LSHIFT, SDL_SCANCODE_RSHIFT};
    b[static_cast<std::size_t>(Action::ThrottleDown)] = {SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL};
    b[static_cast<std::size_t>(Action::Guns)] = {SDL_SCANCODE_F, SDL_SCANCODE_UNKNOWN};
    b[static_cast<std::size_t>(Action::Heavy)] = {SDL_SCANCODE_SPACE, SDL_SCANCODE_UNKNOWN};
    b[static_cast<std::size_t>(Action::SwitchHeavy)] = {SDL_SCANCODE_TAB, SDL_SCANCODE_UNKNOWN};
    return b;
}

bool is_key(const Bindings& b, Action action, SDL_Scancode key) noexcept {
    const Binding& k = binding(b, action);
    return key != SDL_SCANCODE_UNKNOWN && (k.main == key || k.alt == key);
}

InputCommand read_command(const Bindings& b, const bool* keys, bool switch_pressed) noexcept {
    InputCommand c;
    c.turn = axis_of(held(b, Action::TurnLeft, keys), held(b, Action::TurnRight, keys));
    c.pitch = axis_of(held(b, Action::Dive, keys), held(b, Action::Climb, keys));
    c.throttle = axis_of(held(b, Action::ThrottleDown, keys), held(b, Action::ThrottleUp, keys));
    c.guns = held(b, Action::Guns, keys);
    c.heavy = held(b, Action::Heavy, keys);
    c.switch_heavy = switch_pressed;
    return c;
}

} // namespace wof::game
