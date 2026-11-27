// Wings of Freedom: SDL3 front end (design/flight.md). The world is drawn 1:1 in art pixels into a
// render target the size of the view, then shown at the largest whole-number scale that fits the
// window, so the pixels stay square and crisp. Screens that 270 doesn't divide evenly show more of
// the world.
//
//   WingsOfFreedom [--seed N] [--window WxH] [--shot FILE.png] [--ticks N] [--heading DEG]
//                  [--height U] [--pitch DEG] [--throttle PCT] [--at X,Y] [--overlay]
//                  [--height-rule forgiving|strict|auto] [--no-targets] [--fire] [--bomb]
//                  [--sprite-sheet FILE.png] [--version]
//     --seed N        developer tool: the ground from seed N (decimal, or 0x hex)
//     --window WxH    developer tool: open the window at W x H
//     --shot FILE     developer tool: fly N ticks with no input (--ticks, default 120), save the
//                     view to FILE (PNG) and exit
//     --heading DEG   developer tool: the plane's starting heading (0 north, 90 east)
//     --height U      developer tool: the plane's starting height in u (default: tuning.txt)
//     --pitch DEG     developer tool: the plane's starting pitch (it eases back to level)
//     --throttle PCT  developer tool: the plane's starting throttle, 0-100
//     --at X,Y        developer tool: the plane's starting ground position in u (default: the
//                     middle of the battle area's south end, 1440,480)
//     --overlay       developer tool: start with the F1 overlay shown
//     --height-rule R the gunnery height rule: forgiving (default), strict or auto
//     --no-targets    developer tool: start without the practice targets
//     --fire, --bomb  developer tool: with --shot, hold the guns or the bomb release throughout
//     --sprite-sheet F  developer tool: save every baked plane pose to F (PNG) and exit
//     --version       print the version and exit
//
// Keys (design/flight.md, Controls): A/D turn, W dive, S climb, Shift/Ctrl throttle up/down, F
// guns, Space bombs (hold to ripple), Tab switch heavy weapon. Until PR 6's menus: F2 practice
// targets on or off, F3 the height rule, F4 the bomb marker. F1 shows the developer overlay, F11
// toggles fullscreen, and Esc quits (the pause menu comes in PR 6). When a run ends, Enter or R
// starts a new one. The tuning file (tuning.txt next to the executable) is reloaded when it
// changes.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/body.hpp"
#include "core/camera.hpp"
#include "core/fixed_step.hpp"
#include "core/flight.hpp"
#include "core/ground.hpp"
#include "core/hash.hpp"
#include "core/iso.hpp"
#include "core/map.hpp"
#include "core/targets.hpp"
#include "core/tuning.hpp"
#include "core/units.hpp"
#include "core/view.hpp"
#include "core/weapons.hpp"
#include "core/world.hpp"
#include "game/bindings.hpp"
#include "game/effects.hpp"
#include "game/ground_view.hpp"
#include "game/plane_sprites.hpp"

namespace {

using namespace wof;

constexpr std::int64_t kTickNs = 1'000'000'000 / kTicksPerSecond;
constexpr int kMaxTicksPerFrame = 8;
constexpr Uint64 kTuningCheckNs = 500'000'000; // the tuning file is checked this often
constexpr float kGlyph = 8.0f;                 // the debug font's character size

struct Options {
    bool has_seed = false;
    std::uint64_t seed = 0;
    int window_w = 0; // 0: the largest 480x270 multiple that fits the screen
    int window_h = 0;
    const char* shot = nullptr;
    long ticks = 120;
    float heading = 0.0f;
    bool has_height = false;
    float height = 0.0f;
    bool has_pitch = false;
    float pitch = 0.0f;
    bool has_throttle = false;
    float throttle_pct = 0.0f;
    bool has_at = false;
    int at_x = 0;
    int at_y = 0;
    bool overlay = false;
    HeightRule height_rule = HeightRule::Forgiving;
    bool no_targets = false;
    bool hold_fire = false; // --shot: hold the guns
    bool hold_bomb = false; // --shot: hold the bomb release
    const char* sprite_sheet = nullptr;
};

// Parses "A<sep>B" into two whole numbers. False on anything else.
bool parse_pair(const char* text, char sep, int& a, int& b) {
    char* end = nullptr;
    const long x = std::strtol(text, &end, 10);
    if (end == text || *end != sep) {
        return false;
    }
    const char* rest = end + 1;
    const long y = std::strtol(rest, &end, 10);
    if (end == rest || *end != '\0') {
        return false;
    }
    a = static_cast<int>(x);
    b = static_cast<int>(y);
    return true;
}

bool parse_number(const char* text, float& out) {
    char* end = nullptr;
    const double v = std::strtod(text, &end);
    if (end == text || *end != '\0' || !std::isfinite(v)) {
        return false;
    }
    out = static_cast<float>(v);
    return true;
}

// 0: carry on; 1: exit with success (--version); 2: bad arguments.
int parse_options(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const bool has_value = i + 1 < argc;
        if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("Wings of Freedom %s\n", WOF_VERSION);
            return 1;
        }
        if (std::strcmp(argv[i], "--seed") == 0 && has_value) {
            char* end = nullptr;
            out.seed = std::strtoull(argv[++i], &end, 0);
            out.has_seed = end != nullptr && *end == '\0';
            if (!out.has_seed) {
                std::fprintf(stderr, "--seed takes a number (decimal or 0x hex)\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--window") == 0 && has_value) {
            if (!parse_pair(argv[++i], 'x', out.window_w, out.window_h) || out.window_w < 1 ||
                out.window_h < 1) {
                std::fprintf(stderr, "--window takes a size as WxH\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--shot") == 0 && has_value) {
            out.shot = argv[++i];
        } else if (std::strcmp(argv[i], "--sprite-sheet") == 0 && has_value) {
            out.sprite_sheet = argv[++i];
        } else if (std::strcmp(argv[i], "--ticks") == 0 && has_value) {
            out.ticks = std::strtol(argv[++i], nullptr, 10);
            if (out.ticks < 1) {
                std::fprintf(stderr, "--ticks takes a whole number above 0\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--heading") == 0 && has_value) {
            if (!parse_number(argv[++i], out.heading)) {
                std::fprintf(stderr, "--heading takes degrees (0 north, 90 east)\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--height") == 0 && has_value) {
            out.has_height = parse_number(argv[++i], out.height) && out.height >= 0.0f;
            if (!out.has_height) {
                std::fprintf(stderr, "--height takes a height in u, 0 or more\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--pitch") == 0 && has_value) {
            out.has_pitch = parse_number(argv[++i], out.pitch);
            if (!out.has_pitch) {
                std::fprintf(stderr, "--pitch takes degrees (nose up is positive)\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--throttle") == 0 && has_value) {
            out.has_throttle = parse_number(argv[++i], out.throttle_pct) &&
                               out.throttle_pct >= 0.0f && out.throttle_pct <= 100.0f;
            if (!out.has_throttle) {
                std::fprintf(stderr, "--throttle takes a percentage, 0-100\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--at") == 0 && has_value) {
            if (!parse_pair(argv[++i], ',', out.at_x, out.at_y)) {
                std::fprintf(stderr, "--at takes a ground position in u as X,Y\n");
                return 2;
            }
            out.has_at = true;
        } else if (std::strcmp(argv[i], "--overlay") == 0) {
            out.overlay = true;
        } else if (std::strcmp(argv[i], "--height-rule") == 0 && has_value) {
            if (!parse_height_rule(argv[++i], out.height_rule)) {
                std::fprintf(stderr, "--height-rule takes forgiving, strict or auto\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--no-targets") == 0) {
            out.no_targets = true;
        } else if (std::strcmp(argv[i], "--fire") == 0) {
            out.hold_fire = true;
        } else if (std::strcmp(argv[i], "--bomb") == 0) {
            out.hold_bomb = true;
        } else {
            std::fprintf(stderr,
                         "usage: WingsOfFreedom [--seed N] [--window WxH] [--shot FILE.png] "
                         "[--ticks N] [--heading DEG] [--height U] [--pitch DEG] [--throttle PCT] "
                         "[--at X,Y] [--overlay] [--height-rule forgiving|strict|auto] "
                         "[--no-targets] [--fire] [--bomb] [--sprite-sheet FILE.png] "
                         "[--version]\n");
            return 2;
        }
    }
    return 0;
}

// Normal runs get a fresh random seed; the simulation itself never reads the clock.
std::uint64_t random_seed() {
    return splitmix64(SDL_GetPerformanceCounter() ^ (SDL_GetTicksNS() << 17u));
}

// The window's view: its scale and size, and the render target the world is drawn into.
// Recreated whenever the window's pixel size changes the fit.
struct Screen {
    int out_w = 0;
    int out_h = 0;
    ViewFit fit{};
    SDL_Texture* target = nullptr;
};

bool refresh_screen(SDL_Renderer* r, Screen& s) {
    int w = 0;
    int h = 0;
    SDL_GetRenderOutputSize(r, &w, &h);
    s.out_w = w;
    s.out_h = h;
    const ViewFit fit = fit_view(w, h);
    if (s.target != nullptr && fit.w == s.fit.w && fit.h == s.fit.h) {
        s.fit = fit;
        return true;
    }
    if (s.target != nullptr) {
        SDL_DestroyTexture(s.target);
    }
    s.fit = fit;
    s.target =
        SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, fit.w, fit.h);
    if (s.target == nullptr) {
        SDL_Log("view target %dx%d failed: %s", fit.w, fit.h, SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(s.target, SDL_SCALEMODE_NEAREST);
    return true;
}

// Saves the current render target as a PNG.
bool save_png(SDL_Renderer* r, const char* path) {
    SDL_Surface* shot = SDL_RenderReadPixels(r, nullptr);
    if (shot == nullptr) {
        SDL_Log("reading pixels failed: %s", SDL_GetError());
        return false;
    }
    const bool ok = SDL_SavePNG(shot, path);
    if (!ok) {
        SDL_Log("saving %s failed: %s", path, SDL_GetError());
    } else {
        SDL_Log("saved %s (%dx%d)", path, shot->w, shot->h);
    }
    SDL_DestroySurface(shot);
    return ok;
}

// The live-tuning file: every Tuning value, next to the executable.
struct TuningFile {
    std::string path;
    SDL_Time modified = 0;
    Uint64 last_check = 0;
};

// Reads the tuning file over the defaults (so the file is the whole truth).
void load_tuning(Tuning& tuning, const TuningFile& tf) {
    std::size_t size = 0;
    void* data = SDL_LoadFile(tf.path.c_str(), &size);
    if (data == nullptr) {
        SDL_Log("can't read the tuning file: %s", SDL_GetError());
        return;
    }
    const std::string text(static_cast<const char*>(data), size);
    SDL_free(data);
    Tuning t;
    std::string errors;
    const bool ok = from_tuning(text, t, errors);
    tuning = t;
    if (ok) {
        SDL_Log("tuning file loaded");
    } else {
        SDL_Log("tuning file loaded with problems:\n%s", errors.c_str());
    }
}

// Loads the tuning file if it's there, or writes the defaults to it if not.
void open_tuning(Tuning& tuning, TuningFile& tf) {
    SDL_PathInfo info;
    if (SDL_GetPathInfo(tf.path.c_str(), &info)) {
        tf.modified = info.modify_time;
        load_tuning(tuning, tf);
    } else {
        const std::string text = to_tuning(tuning);
        if (SDL_SaveFile(tf.path.c_str(), text.data(), text.size())) {
            SDL_Log("wrote the defaults to %s", tf.path.c_str());
            if (SDL_GetPathInfo(tf.path.c_str(), &info)) {
                tf.modified = info.modify_time;
            }
        } else {
            SDL_Log("can't write %s: %s", tf.path.c_str(), SDL_GetError());
        }
    }
    tf.last_check = SDL_GetTicksNS();
}

// Reloads the tuning file when it has changed on disk. True if it was reloaded.
bool watch_tuning(Tuning& tuning, TuningFile& tf) {
    const Uint64 now = SDL_GetTicksNS();
    if (now - tf.last_check < kTuningCheckNs) {
        return false;
    }
    tf.last_check = now;
    SDL_PathInfo info;
    if (SDL_GetPathInfo(tf.path.c_str(), &info) && info.modify_time != tf.modified) {
        tf.modified = info.modify_time;
        load_tuning(tuning, tf);
        return true;
    }
    return false;
}

// The shorter way round from angle a to angle b, in degrees.
float angle_between(float a, float b) {
    float d = b - a;
    if (d > 180.0f) {
        d -= 360.0f;
    } else if (d < -180.0f) {
        d += 360.0f;
    }
    return d;
}

// A body partway between two ticks, for smooth drawing.
Body between(const Body& a, const Body& b, float t) {
    Body out = b;
    out.x = a.x + (b.x - a.x) * t;
    out.y = a.y + (b.y - a.y) * t;
    out.z = a.z + (b.z - a.z) * t;
    out.heading_deg = wrap_heading(a.heading_deg + angle_between(a.heading_deg, b.heading_deg) * t);
    out.pitch_deg = a.pitch_deg + (b.pitch_deg - a.pitch_deg) * t;
    out.bank_deg = a.bank_deg + (b.bank_deg - a.bank_deg) * t;
    return out;
}

std::string format(const char* fmt, ...) SDL_PRINTF_VARARG_FUNC(1);
std::string format(const char* fmt, ...) {
    char buf[160];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    return buf;
}

// Lines of debug-font text on a dark backing, with the block's top-left at (x, y).
void draw_text_block(SDL_Renderer* r, float x, float y, const std::vector<std::string>& lines,
                     SDL_Color colour) {
    std::size_t widest = 0;
    for (const std::string& l : lines) {
        widest = l.size() > widest ? l.size() : widest;
    }
    const SDL_FRect back{x, y, static_cast<float>(widest) * kGlyph + 4.0f,
                         static_cast<float>(lines.size()) * 10.0f + 2.0f};
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_RenderFillRect(r, &back);
    SDL_SetRenderDrawColor(r, colour.r, colour.g, colour.b, colour.a);
    float line_y = y + 2.0f;
    for (const std::string& l : lines) {
        SDL_RenderDebugText(r, x + 2.0f, line_y, l.c_str());
        line_y += 10.0f;
    }
}

// The same, centred across a view `view_w` pixels wide.
void draw_text_centred(SDL_Renderer* r, int view_w, float y, const std::vector<std::string>& lines,
                       SDL_Color colour) {
    std::size_t widest = 0;
    for (const std::string& l : lines) {
        widest = l.size() > widest ? l.size() : widest;
    }
    const float w = static_cast<float>(widest) * kGlyph + 4.0f;
    draw_text_block(r, std::floor((static_cast<float>(view_w) - w) * 0.5f), y, lines, colour);
}

[[nodiscard]] SDL_FPoint on_view(PixelPt origin, float x, float y, float z) {
    const ScreenPt s = to_screen(x, y, z);
    return {s.x - static_cast<float>(origin.x), s.y - static_cast<float>(origin.y)};
}

// A practice ground target: a white and red bullseye, glowing yellow just after a hit.
void draw_bullseye(SDL_Renderer* r, PixelPt origin, const Target& t, float radius) {
    const SDL_FColor white{0.94f, 0.93f, 0.88f, 1.0f};
    const SDL_FColor red{0.78f, 0.16f, 0.12f, 1.0f};
    game::fill_ground_disc(r, origin, t.x, t.y, 0.0f, radius, white);
    game::fill_ground_disc(r, origin, t.x, t.y, 0.0f, radius * 0.7f, red);
    game::fill_ground_disc(r, origin, t.x, t.y, 0.0f, radius * 0.42f, white);
    game::fill_ground_disc(r, origin, t.x, t.y, 0.0f, radius * 0.16f, red);
    if (t.since_hit_s < 0.15f) {
        game::fill_ground_disc(r, origin, t.x, t.y, 0.0f, radius * 1.15f,
                               SDL_FColor{1.0f, 0.9f, 0.3f, 0.7f});
    }
}

// Everything above the ground, in order: scorch marks, ground targets, shadows, dust and blasts,
// bombs, tracers, then the drones and planes from the back of the view to the front.
void draw_scene(SDL_Renderer* r, PixelPt origin, const World& world, const Body& plane,
                const game::PlaneSprites& sprites, const game::Effects& effects,
                const Tuning& tuning, bool marker) {
    effects.draw_ground(r, origin, tuning.look.scorch_fade_s);
    for (const Target& t : world.targets) {
        if (t.kind == TargetKind::Ground) {
            draw_bullseye(r, origin, t, tuning.rules.targets.ground_radius_u);
        }
    }

    // The fliers: the drones (orange-red, banking round their orbits) and the player's plane.
    struct Flier {
        Body body;
        bool drone = false;
        bool flash = false;
    };
    std::vector<Flier> fliers;
    for (const Target& t : world.targets) {
        if (t.kind != TargetKind::Drone) {
            continue;
        }
        Body d;
        d.x = t.x;
        d.y = t.y;
        d.z = t.z;
        d.heading_deg = t.heading_deg;
        d.bank_deg = t.clockwise ? 30.0f : -30.0f;
        fliers.push_back(Flier{d, true, t.since_hit_s < 0.12f});
    }
    fliers.push_back(Flier{plane, false, false});
    for (const Flier& f : fliers) {
        sprites.draw(f.body, origin, true);
    }
    for (const Bomb& m : world.bombs) {
        const SDL_FPoint s = on_view(origin, m.x, m.y, 0.0f);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 90);
        const SDL_FRect shadow{std::round(s.x) - 1.0f, std::round(s.y), 2.0f, 1.0f};
        SDL_RenderFillRect(r, &shadow);
    }
    effects.draw_air(r, origin);
    for (const Bomb& m : world.bombs) {
        const SDL_FPoint s = on_view(origin, m.x, m.y, m.z);
        SDL_SetRenderDrawColor(r, 36, 38, 30, 255);
        const SDL_FRect bomb{std::round(s.x) - 1.0f, std::round(s.y) - 1.0f, 3.0f, 2.0f};
        SDL_RenderFillRect(r, &bomb);
    }
    // Tracers: a short streak behind each one, along its flight.
    SDL_SetRenderDrawColor(r, 255, 238, 150, 255);
    for (const Round& round : world.rounds) {
        if (!round.tracer) {
            continue;
        }
        const float len =
            std::sqrt(round.vx * round.vx + round.vy * round.vy + round.vz * round.vz);
        const float back = len > 0.0f ? 7.0f / len : 0.0f;
        const SDL_FPoint a = on_view(origin, round.x, round.y, round.z);
        const SDL_FPoint b = on_view(origin, round.x - round.vx * back, round.y - round.vy * back,
                                     round.z - round.vz * back);
        SDL_RenderLine(r, std::round(a.x), std::round(a.y), std::round(b.x), std::round(b.y));
    }
    if (marker && plane.outcome == Outcome::Flying) {
        const GroundSpot spot =
            bomb_impact(plane, tuning.rules.weapon, tuning.rules.flight.gravity);
        const SDL_FPoint s = on_view(origin, spot.x, spot.y, 0.0f);
        SDL_SetRenderDrawColor(r, 255, 80, 60, 255);
        SDL_RenderLine(r, s.x - 4.0f, s.y, s.x + 4.0f, s.y);
        SDL_RenderLine(r, s.x, s.y - 2.0f, s.x, s.y + 2.0f);
    }
    std::sort(fliers.begin(), fliers.end(), [](const Flier& a, const Flier& b) {
        return to_screen(a.body.x, a.body.y).y < to_screen(b.body.x, b.body.y).y;
    });
    for (const Flier& f : fliers) {
        if (!f.drone && f.body.outcome == Outcome::Crashed) {
            continue;
        }
        sprites.draw(f.body, origin, false,
                     f.drone ? SDL_Color{255, 120, 90, 255} : SDL_Color{255, 255, 255, 255},
                     f.flash);
    }
    // A target's hit count, for two seconds after a hit.
    SDL_SetRenderDrawColor(r, 255, 244, 190, 255);
    for (const Target& t : world.targets) {
        if (t.since_hit_s < 2.0f) {
            const SDL_FPoint s = on_view(origin, t.x, t.y, t.z);
            const std::string n = std::to_string(t.hits);
            SDL_RenderDebugText(r, std::round(s.x) - static_cast<float>(n.size()) * 4.0f,
                                std::round(s.y) - 22.0f, n.c_str());
        }
    }
}

const char* banner_of(Outcome o) {
    switch (o) {
    case Outcome::Crashed:
        return "CRASHED";
    case Outcome::LeftArea:
        return "LEFT THE BATTLE AREA";
    case Outcome::ReturnedHome:
        return "RETURNED TO BASE";
    case Outcome::Flying:
        break;
    }
    return "";
}

} // namespace

int main(int argc, char** argv) {
    Options opts;
    if (const int parsed = parse_options(argc, argv, opts); parsed != 0) {
        return parsed == 1 ? 0 : 2;
    }
    const bool shot_mode = opts.shot != nullptr;
    const bool tool_mode = shot_mode || opts.sprite_sheet != nullptr;

    SDL_SetAppMetadata("Wings of Freedom", WOF_VERSION, "io.github.swooshiiroll.wingsoffreedom");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // The window opens at the largest 480x270 multiple that fits the screen (or --window).
    WindowSize size{opts.window_w, opts.window_h};
    if (size.w == 0) {
        SDL_Rect usable{0, 0, 1920, 1040};
        if (!SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable)) {
            SDL_Log("no display bounds (%s): assuming 1080p", SDL_GetError());
        }
        size = start_window(usable.w, usable.h);
    }
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (tool_mode) {
        flags |= SDL_WINDOW_HIDDEN;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Wings of Freedom", size.w, size.h, flags, &window,
                                     &renderer)) {
        SDL_Log("window/renderer failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    if (!tool_mode) {
        SDL_SetRenderVSync(renderer, 1);
    }
    const char* backend = SDL_GetRendererName(renderer);
    SDL_Log("Wings of Freedom %s, SDL %d.%d.%d, renderer: %s", WOF_VERSION,
            SDL_VERSIONNUM_MAJOR(SDL_GetVersion()), SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
            SDL_VERSIONNUM_MICRO(SDL_GetVersion()), backend);

    int exit_code = 0;
    {
        game::PlaneSprites sprites(renderer);
        if (!sprites.bake()) {
            exit_code = 1;
        }
        if (opts.sprite_sheet != nullptr) {
            const bool ok = exit_code == 0 && sprites.save_sheet(opts.sprite_sheet);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return ok ? 0 : 1;
        }

        Tuning tuning;
        TuningFile tuning_file;
        const char* base_path = SDL_GetBasePath();
        tuning_file.path = std::string(base_path != nullptr ? base_path : "") + "tuning.txt";
        open_tuning(tuning, tuning_file);

        const std::uint64_t seed = opts.has_seed ? opts.seed : random_seed();
        SDL_Log("ground seed 0x%016llx", static_cast<unsigned long long>(seed));
        game::GroundView ground(renderer);
        ground.reset(tuning.ground, seed);

        // A run: one player for now, in the air near the south edge heading north (or where the
        // options say). The world itself takes any number of planes.
        World world;
        std::vector<Body> previous;
        Camera camera;
        game::Effects effects;
        HeightRule height_rule = opts.height_rule; // a game option (PR 6 puts it in a menu)
        bool targets_on = !opts.no_targets;
        bool marker = false;      // the bomb marker (a developer switch until it's an upgrade)
        float own_blast_s = 0.0f; // how long "OWN BLAST" still shows
        float own_blast_damage = 0.0f;
        const auto new_run = [&]() {
            const float x =
                opts.has_at ? static_cast<float>(opts.at_x) : static_cast<float>(kMapWidthU) * 0.5f;
            const float y = opts.has_at ? static_cast<float>(opts.at_y) : 480.0f;
            Body b = start_body(tuning.rules.flight, x, y, opts.heading);
            if (opts.has_height) {
                b.z = std::fmin(opts.height, tuning.rules.flight.ceiling_u);
            }
            if (opts.has_pitch) {
                b.pitch_deg = opts.pitch;
            }
            if (opts.has_throttle) {
                b.throttle = opts.throttle_pct / 100.0f;
            }
            arm(b, tuning.rules.weapon);
            world = World{};
            world.bodies.push_back(b);
            if (targets_on) {
                world.targets = practice_targets(tuning.rules.targets);
            }
            previous = world.bodies;
            camera.snap_to(world.bodies[0], tuning.camera);
            effects.clear();
            own_blast_s = 0.0f;
        };
        new_run();

        const game::Bindings bindings = game::default_bindings();
        bool switch_pressed = false;
        bool overlay = opts.overlay;
        Screen screen;
        if (exit_code == 0 && !refresh_screen(renderer, screen)) {
            exit_code = 1;
        }

        FixedStep step(kTickNs, kMaxTicksPerFrame);
        Uint64 last = SDL_GetTicksNS();
        Uint64 fps_since = last;
        int fps_frames = 0;
        int fps = 0;
        bool running = exit_code == 0;

        while (running) {
            const bool run_over = world.bodies[0].outcome != Outcome::Flying;
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                    if (e.key.key == SDLK_ESCAPE) {
                        running = false;
                    } else if (e.key.key == SDLK_F1) {
                        overlay = !overlay;
                    } else if (e.key.key == SDLK_F11) {
                        const bool full = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
                        SDL_SetWindowFullscreen(window, !full);
                    } else if (e.key.key == SDLK_F2) {
                        targets_on = !targets_on;
                        world.targets = targets_on ? practice_targets(tuning.rules.targets)
                                                   : std::vector<Target>{};
                    } else if (e.key.key == SDLK_F3) {
                        height_rule = static_cast<HeightRule>((static_cast<int>(height_rule) + 1) %
                                                              kHeightRules);
                    } else if (e.key.key == SDLK_F4) {
                        marker = !marker;
                    } else if (run_over && (e.key.key == SDLK_RETURN ||
                                            e.key.key == SDLK_KP_ENTER || e.key.key == SDLK_R)) {
                        new_run();
                    } else if (game::is_key(bindings, game::Action::SwitchHeavy, e.key.scancode)) {
                        switch_pressed = true;
                    }
                }
            }
            if (!refresh_screen(renderer, screen)) {
                exit_code = 1;
                break;
            }
            if (watch_tuning(tuning, tuning_file)) {
                ground.reset(tuning.ground, seed);
            }

            // The simulation's ticks: real time normally, exactly one a frame for --shot (and with
            // no input), so a screenshot is the same every time. When the run is over, it waits.
            const Uint64 now = SDL_GetTicksNS();
            const int ticks = shot_mode ? 1 : step.advance(static_cast<std::int64_t>(now - last));
            last = now;
            const bool* keys = SDL_GetKeyboardState(nullptr);
            tuning.rules.height_rule = height_rule;
            InputCommand scripted;
            scripted.guns = opts.hold_fire;
            scripted.heavy = opts.hold_bomb;
            for (int t = 0; t < ticks && world.bodies[0].outcome == Outcome::Flying; ++t) {
                previous = world.bodies;
                const std::array<InputCommand, 1> commands = {
                    shot_mode ? scripted : game::read_command(bindings, keys, switch_pressed)};
                switch_pressed = false;
                wof::step(world, commands, tuning.rules);
                camera.follow(world.bodies[0], tuning.camera);
                effects.add(world.events, tuning.rules.weapon.blast_radius_u);
                effects.tick(kTickSeconds, tuning.look.scorch_fade_s);
                own_blast_s = std::max(0.0f, own_blast_s - kTickSeconds);
                for (const Event& ev : world.events) {
                    if (ev.kind == EventKind::BlastHitPlane && ev.who == 0 && ev.by == 0) {
                        own_blast_s = 2.0f;
                        own_blast_damage = ev.damage;
                    }
                }
            }
            const float alpha = shot_mode ? 1.0f : step.alpha();

            ++fps_frames;
            if (now - fps_since >= 1'000'000'000) {
                fps = fps_frames;
                fps_frames = 0;
                fps_since = now;
            }

            // The world, 1:1 in art pixels, into the view target.
            const ViewFit& fit = screen.fit;
            const PixelPt origin = view_origin(camera.focus(alpha), fit.w, fit.h);
            SDL_SetRenderTarget(renderer, screen.target);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            ground.draw(origin, fit.w, fit.h);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            const Body plane = between(previous[0], world.bodies[0], alpha);
            draw_scene(renderer, origin, world, plane, sprites, effects, tuning, marker);

            // A temporary readout until PR 6's HUD: throttle, speed, height, fuel, warnings.
            std::string flags_line;
            if (plane.stalled) {
                flags_line += "STALL  ";
            }
            if (!plane.engine_on) {
                flags_line += "ENGINE OFF  ";
            }
            if (own_blast_s > 0.0f) {
                flags_line += format("OWN BLAST -%.0f", static_cast<double>(own_blast_damage));
            }
            int hits = 0;
            for (const Target& t : world.targets) {
                hits += t.hits;
            }
            std::vector<std::string> readout = {
                format("THR %3.0f%%  %3.0f MPH  %5.0f FT  FUEL %3.0f%%",
                       static_cast<double>(plane.throttle * 100.0f),
                       static_cast<double>(plane.speed_u_s * kMphPerUs),
                       static_cast<double>(plane.z * kFeetPerU),
                       static_cast<double>(plane.fuel * 100.0f)),
                format("GUN %3d  BOMBS %d  [%s]  RULE %s  HITS %d", plane.loadout.rounds,
                       plane.loadout.bombs, heavy_name(plane.loadout.heavy),
                       height_rule_name(height_rule), hits)};
            if (!flags_line.empty()) {
                readout.push_back(flags_line);
            }
            const float block_h = static_cast<float>(readout.size()) * 10.0f + 2.0f;
            draw_text_block(renderer, 2.0f, static_cast<float>(fit.h) - block_h - 2.0f, readout,
                            SDL_Color{236, 232, 200, 255});
            if (plane.edge_countdown_s >= 0.0f && plane.outcome == Outcome::Flying) {
                draw_text_centred(renderer, fit.w, 40.0f,
                                  {format("RETURN TO THE BATTLE AREA  %d",
                                          static_cast<int>(std::ceil(plane.edge_countdown_s)))},
                                  SDL_Color{255, 120, 90, 255});
            } else if (plane.edge_warning) {
                draw_text_centred(renderer, fit.w, 40.0f, {"EDGE OF THE BATTLE AREA"},
                                  SDL_Color{255, 220, 120, 255});
            }
            if (plane.outcome != Outcome::Flying) {
                const long secs = static_cast<long>(world.tick / kTicksPerSecond);
                draw_text_centred(renderer, fit.w, static_cast<float>(fit.h) * 0.5f - 20.0f,
                                  {banner_of(plane.outcome),
                                   format("time flown %ld:%02ld", secs / 60, secs % 60),
                                   "ENTER OR R: NEW RUN"},
                                  SDL_Color{255, 236, 170, 255});
            }

            if (overlay) {
                const GroundSample under =
                    sample_ground(tuning.ground, seed, static_cast<int>(std::floor(plane.x)),
                                  static_cast<int>(std::floor(plane.y)));
                const game::PlaneSprites::Cell cell = game::PlaneSprites::cell_of(plane);
                draw_text_block(
                    renderer, 0.0f, 0.0f,
                    {format("Wings of Freedom %s  %d fps  %s", WOF_VERSION, fps, backend),
                     format("window %dx%d  scale %dx  view %dx%d", screen.out_w, screen.out_h,
                            fit.scale, fit.w, fit.h),
                     format("camera %d,%d  lead %.0f,%.0f u", origin.x, origin.y,
                            static_cast<double>(camera.lead().x),
                            static_cast<double>(camera.lead().y)),
                     format("plane %.0f,%.0f  %.0f u  %.1f u/s", static_cast<double>(plane.x),
                            static_cast<double>(plane.y), static_cast<double>(plane.z),
                            static_cast<double>(plane.speed_u_s)),
                     format("heading %.0f  pitch %.0f  bank %.0f  pose %d,%d",
                            static_cast<double>(plane.heading_deg),
                            static_cast<double>(plane.pitch_deg),
                            static_cast<double>(plane.bank_deg), cell.column, cell.row),
                     format("throttle %.0f%%  fuel %.1f%%  %s%s",
                            static_cast<double>(plane.throttle * 100.0f),
                            static_cast<double>(plane.fuel * 100.0f),
                            plane.stalled ? "stalled " : "", plane.engine_on ? "" : "engine off"),
                     format("ground %s  tiles %d  tick %lld  sprites %.0f ms",
                            ground_name(under.kind), ground.cached(),
                            static_cast<long long>(world.tick), sprites.bake_ms()),
                     format("rounds in flight %zu  bombs falling %zu  targets %zu%s",
                            world.rounds.size(), world.bombs.size(), world.targets.size(),
                            marker ? "  marker on" : "")},
                    SDL_Color{236, 232, 200, 255});
            }
            if (shot_mode && (world.tick >= opts.ticks || plane.outcome != Outcome::Flying)) {
                if (!save_png(renderer, opts.shot)) {
                    exit_code = 1;
                }
                running = false;
            }

            // The view target, at the whole-number scale, from the window's top-left (the last
            // partial art pixel is cropped).
            SDL_SetRenderTarget(renderer, nullptr);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            const SDL_FRect dst{0.0f, 0.0f, static_cast<float>(fit.w * fit.scale),
                                static_cast<float>(fit.h * fit.scale)};
            SDL_RenderTexture(renderer, screen.target, nullptr, &dst);
            SDL_RenderPresent(renderer);
        }
        if (screen.target != nullptr) {
            SDL_DestroyTexture(screen.target);
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exit_code;
}
