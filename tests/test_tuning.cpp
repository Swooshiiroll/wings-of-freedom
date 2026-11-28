#include "core/tuning.hpp"

#include <initializer_list>
#include <string>

#include <doctest/doctest.h>

using namespace wof;

TEST_CASE("The tuning file round-trips every value exactly") {
    Tuning t;
    t.camera.lead_u = 37.25f;
    t.camera.lead_ease = 0.3f;
    t.rules.flight.ceiling_u = 543.25f;
    t.rules.scale.u_per_mph = 0.7f;
    t.rules.edge.countdown_s = 7.5f;
    t.ground.field_max_u = 199;
    t.ground.weights[2] = 7;
    t.rules.rederive(); // the planes follow the base and the scale, as loading does
    const std::string text = to_tuning(t);
    Tuning back;
    std::string errors;
    CHECK(from_tuning(text, back, errors));
    CHECK(errors.empty());
    CHECK(back == t);
}

TEST_CASE("The default tuning file reads back as the defaults") {
    Tuning back;
    back.camera.lead_u = 1.0f;
    std::string errors;
    CHECK(from_tuning(to_tuning(Tuning{}), back, errors));
    CHECK(back == Tuning{});
}

TEST_CASE("Keys a tuning text leaves out keep their values") {
    Tuning t;
    t.rules.flight.turn_rate_deg_s = 55.0f;
    std::string errors;
    CHECK(from_tuning("# only the camera\ncamera.lead_u = 12\n", t, errors));
    CHECK(t.camera.lead_u == doctest::Approx(12.0f));
    CHECK(t.rules.flight.turn_rate_deg_s == doctest::Approx(55.0f));
}

TEST_CASE("Unknown keys, bad values and bad lines are reported by line") {
    Tuning t;
    std::string errors;
    CHECK_FALSE(from_tuning("camera.lead_u = 20\n"
                            "camera.nope = 3\n"
                            "flight.gravity = fast\n"
                            "ground.block_u = 1.5\n"
                            "just words\n",
                            t, errors));
    CHECK(t.camera.lead_u == doctest::Approx(20.0f));
    CHECK(t.rules.flight.gravity == doctest::Approx(90.0f)); // unchanged
    CHECK(t.ground.block_u == 512);
    CHECK(errors.find("line 2: unknown key camera.nope") != std::string::npos);
    CHECK(errors.find("line 3: flight.gravity: not a number") != std::string::npos);
    CHECK(errors.find("line 4: ground.block_u: not a whole number") != std::string::npos);
    CHECK(errors.find("line 5: expected key = value") != std::string::npos);
}

TEST_CASE("Out-of-range values are clamped and reported") {
    Tuning t;
    std::string errors;
    CHECK_FALSE(from_tuning("camera.lead_u = -5\nground.road_skip_pct = 400\n", t, errors));
    CHECK(t.camera.lead_u == doctest::Approx(0.0f));
    CHECK(t.ground.road_skip_pct == 100);
    CHECK(errors.find("line 1: camera.lead_u: clamped to 0") != std::string::npos);
    CHECK(errors.find("line 2: ground.road_skip_pct: clamped to 100") != std::string::npos);
}

TEST_CASE("Ground values that don't fit together are fixed and reported") {
    Tuning t;
    std::string errors;
    CHECK_FALSE(from_tuning("ground.field_min_u = 100\nground.field_max_u = 150\n", t, errors));
    CHECK(t.ground.field_max_u == 200);
    CHECK(errors.find("ground: adjusted") != std::string::npos);
}

TEST_CASE("An old tuning file's stand-in keys are reported, and the rest still loads") {
    Tuning t;
    std::string errors;
    CHECK_FALSE(from_tuning("stand_in.speed_u_s = 110\ncamera.lead_u = 33\n", t, errors));
    CHECK(errors.find("line 1: unknown key stand_in.speed_u_s") != std::string::npos);
    CHECK(t.camera.lead_u == doctest::Approx(33.0f));
}

TEST_CASE("The file says its format; files from before real planes are format 1") {
    const std::string text = to_tuning(Tuning{});
    CHECK(text.find("format = 6\n") != std::string::npos);
    CHECK(tuning_format(text) == kTuningFormat);
    CHECK(tuning_format("# old\ncamera.lead_u = 40\nflight.ceiling_u = 240\n") == 1);
    CHECK(tuning_format("") == 1);
    // alpha.5's file (format 2) is older too, so it's set aside.
    CHECK(tuning_format("format = 2\nscale.tank_s_per_hour = 133.33\n") == 2);
    CHECK(2 < kTuningFormat);
    CHECK(3 < kTuningFormat); // alpha.9's too
    CHECK(4 < kTuningFormat); // v0.1.0-pre's too
    CHECK(5 < kTuningFormat); // and pre.3's
    CHECK(text.find("flight.height_share = ") != std::string::npos);
    Tuning t;
    std::string errors;
    CHECK(from_tuning(text, t, errors)); // the format line itself is no error
    CHECK(errors.empty());
}

TEST_CASE("Each plane's own flight numbers aren't in the file: they come from its real stats") {
    const std::string text = to_tuning(Tuning{});
    for (const char* gone : {"flight.max_level_u_s", "flight.turn_rate_deg_s", "flight.stall_u_s",
                             "flight.tank_s", "flight.best_glide_u_s", "weapon.rounds"}) {
        CAPTURE(gone);
        CHECK(text.find(gone) == std::string::npos);
    }
    CHECK(text.find("flight.ceiling_u = 600\n") != std::string::npos);
}
