#include "core/dashboard.hpp"

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>

#include "core/camera.hpp"
#include "core/canvas.hpp"
#include "core/pixel_font.hpp"
#include "core/view_xf.hpp"
#include "core/wireframe.hpp"

#include <doctest/doctest.h>

using namespace wof;

namespace {

bool overlap(const Rect& a, const Rect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

// How many pixels of `colour` the rectangle holds.
int count(const Canvas& c, const Rect& r, Colour colour) {
    int n = 0;
    for (int y = r.y; y < r.y + r.h; ++y) {
        for (int x = r.x; x < r.x + r.w; ++x) {
            n += c.at(x, y) == colour ? 1 : 0;
        }
    }
    return n;
}

} // namespace

TEST_CASE("Units: gauges read the nation's, and the setting rules arrows and banners") {
    CHECK(nation_units(Nation::US) == UnitSystem::Imperial);
    CHECK(nation_units(Nation::Germany) == UnitSystem::Metric);
    CHECK(units_of(Units::Nation, Nation::Germany) == UnitSystem::Metric);
    CHECK(units_of(Units::Imperial, Nation::Germany) == UnitSystem::Imperial);
    CHECK(units_of(Units::Metric, Nation::US) == UnitSystem::Metric);
    CHECK(height_text(140.0f, UnitSystem::Imperial) == "7,000 FT");
    CHECK(height_text(140.0f, UnitSystem::Metric) == "2,100 M");
    CHECK(height_diff_text(40.0f, UnitSystem::Imperial) == "+2,000");
    CHECK(height_diff_text(-12.0f, UnitSystem::Imperial) == "-600");
    CHECK(height_diff_text(0.5f, UnitSystem::Imperial) == "LEVEL");
    CHECK(grouped(1005.0f, 1) == "1,005");
}

TEST_CASE("Needles: the readings sit where the dial faces mark them, and stop at the ends") {
    const UnitSystem us = UnitSystem::Imperial;
    const UnitSystem de = UnitSystem::Metric;
    CHECK(airspeed_angle(0.0f, us) == doctest::Approx(-150.0f));
    CHECK(airspeed_angle(150.0f, us) == doctest::Approx(0.0f));
    CHECK(airspeed_angle(900.0f, us) == doctest::Approx(150.0f));
    CHECK(std::abs(airspeed_angle(250.0f / kKmhPerMph, de)) < 0.01f); // 250 of 500 km/h
    // 15,000 ft is half round the thousands; 15,250 ft is a quarter round the hundreds.
    CHECK(altimeter_angles(300.0f, us).thousands == doctest::Approx(180.0f));
    CHECK(altimeter_angles(305.0f, us).hundreds == doctest::Approx(90.0f));
    // 5,000 m is half round the metric dial.
    CHECK(altimeter_angles(5000.0f / kMetresPerFoot / kFeetPerU, de).thousands ==
          doctest::Approx(180.0f));
    CHECK(turn_angle(120.0f) == doctest::Approx(45.0f));
    CHECK(turn_angle(-400.0f) == doctest::Approx(-45.0f));
    CHECK(climb_angle(0.0f, us) == doctest::Approx(-90.0f)); // 9 o'clock
    CHECK(climb_angle(5.0f, us) > -50.0f);                   // 15,000 ft/min reads clearly
    CHECK(climb_angle(-5.0f, us) < -130.0f);
    CHECK(climb_angle(1000.0f, us) == doctest::Approx(80.0f)); // stops at the end
    CHECK(climb_angle(-1000.0f, de) == doctest::Approx(-260.0f));
    CHECK(pitch_gauge_angle(0.0f) == doctest::Approx(90.0f));
    CHECK(pitch_gauge_angle(40.0f) == doctest::Approx(40.0f));
    CHECK(engine_rpm(1.0f, true) == doctest::Approx(2400.0f));
    CHECK(engine_rpm(0.0f, true) == doctest::Approx(600.0f));
    CHECK(engine_rpm(1.0f, false) == doctest::Approx(0.0f));
    CHECK(rpm_angle(1500.0f) == doctest::Approx(0.0f));
    CHECK(gauge_angle(0.0f) == doctest::Approx(-60.0f));
    CHECK(gauge_angle(1.0f) == doctest::Approx(60.0f));
}

TEST_CASE("Every panel size lays out its instruments inside it without overlapping") {
    for (int s = 0; s < kPanelSizes; ++s) {
        const auto size = static_cast<PanelSize>(s);
        const DashLayout l = dash_layout(size);
        CHECK(l.height == panel_height(size));
        CAPTURE(l.height);
        for (std::size_t i = 0; i < kInstruments; ++i) {
            const Rect& a = l.rects[i];
            CAPTURE(i);
            CHECK(a.w > 0);
            CHECK(a.h >= 7);
            CHECK(a.x >= 0);
            CHECK(a.y >= 1);
            CHECK(a.x + a.w <= kPanelW);
            CHECK(a.y + a.h <= l.height);
            for (std::size_t j = i + 1; j < kInstruments; ++j) {
                CAPTURE(j);
                CHECK_FALSE(overlap(a, l.rects[j]));
            }
        }
    }
    CHECK(panel_height(PanelSize::Large) == 96);
    CHECK(panel_height(PanelSize::Medium) == 80);
    CHECK(panel_height(PanelSize::Small) == 64);
}

TEST_CASE("The message strip says the loudest thing first") {
    MessageInput in;
    in.status = "WAVE 1";
    CHECK(panel_message(in).text == "WAVE 1");
    in.notice = "ENEMY DOWN";
    in.notice_s = 1.0f;
    CHECK(panel_message(in).text == "ENEMY DOWN");
    CHECK(panel_message(in).tone == Tone::Notice);
    in.edge_warning = true;
    CHECK(panel_message(in).text == "EDGE OF THE BATTLE AREA");
    in.going_down = true;
    CHECK(panel_message(in).text == "GOING DOWN");
    in.own_blast_s = 1.0f;
    in.own_blast_damage = 12.4f;
    CHECK(panel_message(in).text == "OWN BLAST -12");
    in.edge_countdown_s = 6.2f;
    CHECK(panel_message(in).text == "RETURN TO THE BATTLE AREA 7");
    CHECK(panel_message(in).tone == Tone::Warning);
}

TEST_CASE("Lamps light for their warnings, and a dead engine lights ENGINE") {
    const DashLayout l = dash_layout(PanelSize::Large);
    const Rect lamps = l.of(Instrument::Lamps);
    const Colour amber = rgb(244, 176, 40);
    const Colour red = rgb(232, 60, 44);
    PanelState s;
    Canvas c(kPanelW, l.height);
    draw_panel(c, s, l);
    CHECK(count(c, lamps, amber) == 0);
    CHECK(count(c, lamps, red) == 0);
    s.stall = true;
    draw_panel(c, s, l);
    CHECK(count(c, lamps, amber) > 20);
    s.stall = false;
    s.engines = 2;
    s.engine_share = {1.0f, 0.0f};
    s.engine_running = {true, false};
    draw_panel(c, s, l);
    CHECK(count(c, lamps, red) > 20);
}

TEST_CASE("The wireframe colours each part by its damage and drops what's gone") {
    const DashLayout l = dash_layout(PanelSize::Large);
    const Rect box = l.of(Instrument::Damage);
    PanelState s;
    Canvas c(kPanelW, l.height);
    draw_panel(c, s, l);
    const int green = count(c, box, stage_tint(Stage::Ok));
    CHECK(green > 100);
    CHECK(count(c, box, stage_tint(Stage::Critical)) == 0);
    s.stages[static_cast<std::size_t>(Part::LeftWing)] = Stage::Critical;
    draw_panel(c, s, l);
    CHECK(count(c, box, stage_tint(Stage::Critical)) > 20);
    CHECK(count(c, box, stage_tint(Stage::Ok)) < green);
    // A destroyed wing isn't drawn: less on the left half of the box than before.
    const Rect left{box.x + 1, box.y + 1, box.w / 2 - 8, box.h - 2};
    const int before =
        count(c, left, stage_tint(Stage::Critical)) + count(c, left, stage_tint(Stage::Ok));
    s.stages[static_cast<std::size_t>(Part::LeftWing)] = Stage::Destroyed;
    draw_panel(c, s, l);
    const int after =
        count(c, left, stage_tint(Stage::Critical)) + count(c, left, stage_tint(Stage::Ok));
    CHECK(after < before / 2);
    for (const WireLine& line : top_wireframe(fighter_model())) {
        CHECK(std::abs(line.x0) < 14.0f);
        CHECK(std::abs(line.y0) < 14.0f);
    }
}

TEST_CASE("The pixel font draws digits and capitals; lower case draws as capitals") {
    CHECK(glyph_of('a') == glyph_of('A'));
    CHECK(glyph_of('0') != 0);
    CHECK(glyph_of('~') == 0);
    CHECK(text_width("ABC") == 11);
    Canvas c(8, 8);
    c.clear(rgb(0, 0, 0));
    c.text(1, 1, "1", rgb(255, 255, 255));
    CHECK(c.at(2, 1) == rgb(255, 255, 255)); // the 1's tip, in the middle column
    CHECK(c.at(1, 1) == rgb(0, 0, 0));
}

TEST_CASE("With each panel size, the camera keeps your plane and shadow in view to the top") {
    CameraParams p;
    for (const int size : {96, 80, 64}) {
        const float view_h = static_cast<float>(270 - size);
        for (float heading = 0.0f; heading < 360.0f; heading += 45.0f) {
            for (float z = 0.0f; z <= 600.0f; z += 100.0f) {
                Body b;
                b.x = 1000.0f;
                b.y = 2000.0f;
                b.z = z;
                b.heading_deg = heading;
                Camera cam;
                cam.snap_to(b, p, view_h);
                const ViewXf xf =
                    make_view(cam.focus(), z, 600.0f, 0.25f, 480, static_cast<int>(view_h));
                const ScreenPt plane = xf.at(b.x, b.y, z);
                const ScreenPt shadow = xf.at(b.x, b.y, 0.0f);
                CAPTURE(size);
                CAPTURE(heading);
                CAPTURE(z);
                CHECK(plane.y >= kCameraPlaneHalf - 1.0f);
                CHECK(plane.y <= view_h - 4.0f);
                CHECK(shadow.y <= view_h);
            }
        }
    }
}
