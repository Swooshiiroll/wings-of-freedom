#include "core/settings.hpp"

#include <cstddef>
#include <string>

#include <doctest/doctest.h>

using namespace wof;

namespace {
KeyPair keys_of(const Settings& s, Action a) {
    return s.keys[static_cast<std::size_t>(a)];
}
} // namespace

TEST_CASE("The default keys are the design's, auto-leveling On and both aims Forgiving") {
    const Settings s;
    CHECK(keys_of(s, Action::TurnLeft) == KeyPair{4, 0}); // A
    CHECK(keys_of(s, Action::Dive) == KeyPair{26, 0});    // W
    CHECK(keys_of(s, Action::ThrottleUp) == KeyPair{225, 229});
    CHECK(keys_of(s, Action::Heavy) == KeyPair{44, 0}); // Space
    CHECK(s.auto_level == AutoLevel::On);
    CHECK(s.your_aim == HeightRule::Forgiving);
    CHECK(s.enemy_aim == HeightRule::Forgiving);
}

TEST_CASE("The settings file round-trips") {
    Settings s;
    s.auto_level = AutoLevel::Assisted;
    s.your_aim = HeightRule::Strict;
    s.enemy_aim = HeightRule::AutoAim;
    s.panel = PanelSize::Small;
    s.units = Units::Metric;
    CHECK(bind_key(s.keys, Action::Guns, 1, 13));
    clear_key(s.keys, Action::ThrottleUp, 1);
    Settings back;
    std::string errors;
    CHECK(from_settings(to_settings(s), back, errors));
    CHECK(errors.empty());
    CHECK(back == s);
}

TEST_CASE("Bad settings lines are reported and the rest still load") {
    Settings s;
    std::string errors;
    CHECK_FALSE(from_settings("auto_level = off\n"
                              "your_aim = sideways\n"
                              "key.guns = 41 0\n" // Esc is reserved
                              "key.nothing = 4\n"
                              "colour = red\n"
                              "just words\n"
                              "key.dive = 20\n",
                              s, errors));
    CHECK(s.auto_level == AutoLevel::Off);
    CHECK(s.your_aim == HeightRule::Forgiving);
    CHECK(keys_of(s, Action::Guns) == KeyPair{9, 0});
    CHECK(keys_of(s, Action::Dive) == KeyPair{20, 0});
    CHECK(errors.find("line 2: your_aim: bad value sideways") != std::string::npos);
    CHECK(errors.find("line 3: key.guns: bad value 41 0") != std::string::npos);
    CHECK(errors.find("line 4: unknown key key.nothing") != std::string::npos);
    CHECK(errors.find("line 5: unknown key colour") != std::string::npos);
    CHECK(errors.find("line 6: expected key = value") != std::string::npos);
}

TEST_CASE("Binding a key already in use moves it here; reserved keys are refused") {
    Settings s;
    CHECK(bind_key(s.keys, Action::Guns, 0, 4)); // A, which turned left
    CHECK(keys_of(s, Action::Guns).main == 4);
    CHECK(keys_of(s, Action::TurnLeft).main == kNoKey);
    CHECK(bind_key(s.keys, Action::Guns, 1, 4)); // to the second slot of the same action
    CHECK(keys_of(s, Action::Guns) == KeyPair{0, 4});
    CHECK_FALSE(bind_key(s.keys, Action::Guns, 0, kScanEscape));
    CHECK_FALSE(bind_key(s.keys, Action::Guns, 0, kScanF1));
    CHECK_FALSE(bind_key(s.keys, Action::Guns, 0, kScanF12));
    CHECK(keys_of(s, Action::Guns) == KeyPair{0, 4});
    s.keys = default_keys();
    CHECK(s == Settings{});
}
