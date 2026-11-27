#include "core/tuning.hpp"

#include <string>

#include <doctest/doctest.h>

using namespace wof;

TEST_CASE("The tuning file round-trips every value exactly") {
    Tuning t;
    t.camera.lead_u = 37.25f;
    t.camera.lead_ease = 0.3f;
    t.rules.flight.max_level_u_s = 123.456f;
    t.rules.edge.countdown_s = 7.5f;
    t.ground.field_max_u = 199;
    t.ground.weights[2] = 7;
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
