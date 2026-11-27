// Wings of Freedom: SDL3 front end (design/flight.md). The world is drawn 1:1 in art pixels into a
// render target the size of the view, then shown at the largest whole-number scale that fits the
// window, so the pixels stay square and crisp. Screens that 270 doesn't divide evenly show more of
// the world.
//
//   WingsOfFreedom [--seed N] [--window WxH] [--shot FILE.png] [--ticks N] [--heading DEG]
//                  [--height U] [--pitch DEG] [--throttle PCT] [--at X,Y] [--overlay]
//                  [--height-rule forgiving|strict|auto] [--no-targets] [--fire] [--bomb]
//                  [--nation us|de] [--sprite-sheet FILE.png] [--sprite-scale PCT] [--version]
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
//     --sprite-sheet F  developer tool: save every baked plane pose of your nation's fighter to F
//     (PNG)
//                     and exit
//     --sprite-scale PCT  developer tool: the sprite sheet's size, 25-150 (in steps of 1/8)
//     --nation us|de  fly that nation's fighter, skipping the nation screen
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
#include <optional>
#include <string>
#include <vector>

#include "core/body.hpp"
#include "core/camera.hpp"
#include "core/damage.hpp"
#include "core/fixed_step.hpp"
#include "core/flight.hpp"
#include "core/ground.hpp"
#include "core/hash.hpp"
#include "core/iso.hpp"
#include "core/map.hpp"
#include "core/rng.hpp"
#include "core/targets.hpp"
#include "core/tuning.hpp"
#include "core/units.hpp"
#include "core/view.hpp"
#include "core/view_xf.hpp"
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
    bool hold_fire = false;       // --shot: hold the guns
    bool hold_bomb = false;       // --shot: hold the bomb release
    const char* damage = nullptr; // --damage engine=10,lwing=0,...
    bool has_nation = false;      // --nation us|de: skip the nation screen
    Nation nation = Nation::US;
    const char* sprite_sheet = nullptr;
    int sheet_step = kFullSizeStep; // --sprite-scale
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
        } else if (std::strcmp(argv[i], "--sprite-scale") == 0 && has_value) {
            float pct = 0.0f;
            if (!parse_number(argv[++i], pct) || pct < 25.0f || pct > 150.0f) {
                std::fprintf(stderr, "--sprite-scale takes a percentage, 25-150\n");
                return 2;
            }
            out.sheet_step = size_step(pct / 100.0f);
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
        } else if (std::strcmp(argv[i], "--damage") == 0 && has_value) {
            out.damage = argv[++i];
        } else if (std::strcmp(argv[i], "--nation") == 0 && has_value) {
            const std::string n = argv[++i];
            out.has_nation = n == "us" || n == "de";
            out.nation = n == "de" ? Nation::Germany : Nation::US;
            if (!out.has_nation) {
                std::fprintf(stderr, "--nation takes us or de\n");
                return 2;
            }
        } else {
            std::fprintf(
                stderr,
                "usage: WingsOfFreedom [--seed N] [--window WxH] [--shot FILE.png] "
                "[--ticks N] [--heading DEG] [--height U] [--pitch DEG] [--throttle PCT] "
                "[--at X,Y] [--overlay] [--height-rule forgiving|strict|auto] "
                "[--no-targets] [--fire] [--bomb] [--nation us|de] [--sprite-sheet FILE.png] "
                "[--sprite-scale PCT] "
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
    // The zoomed-out ground is drawn here at full size first (4 times the view, for a quarter size
    // from the ceiling), then shrunk smoothly into the view, by halves below half size.
    SDL_Texture* ground = nullptr;
    SDL_Texture* ground_half = nullptr;
    int ground_w = 0;
    int ground_h = 0;
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
    if (s.ground != nullptr) {
        SDL_DestroyTexture(s.ground);
    }
    if (s.ground_half != nullptr) {
        SDL_DestroyTexture(s.ground_half);
    }
    // Wide enough for the view at a quarter size, with a margin so smoothing never reaches past
    // what was drawn.
    s.ground_w = fit.w * 4 + 12;
    s.ground_h = fit.h * 4 + 12;
    s.ground = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, s.ground_w,
                                 s.ground_h);
    s.ground_half = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                      s.ground_w / 2, s.ground_h / 2);
    if (s.ground == nullptr || s.ground_half == nullptr) {
        SDL_Log("ground target %dx%d failed: %s", s.ground_w, s.ground_h, SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(s.ground, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureScaleMode(s.ground_half, SDL_SCALEMODE_LINEAR);
    return true;
}

// The ground for a view. At full size it's drawn straight in, pixel for pixel. Zoomed out (up
// high), it's drawn at full size into the ground target, then shrunk smoothly into the view. Below
// half size it's halved first (each pixel of the half the average of four), so it doesn't shimmer.
void draw_ground(SDL_Renderer* r, const Screen& s, game::GroundView& ground, const ViewXf& xf) {
    const ViewFit& fit = s.fit;
    const float k = xf.scale_at(0.0f);
    if (k >= 0.999f) {
        const PixelPt origin{static_cast<int>(std::lround(xf.focus.x - xf.cx)),
                             static_cast<int>(std::lround(xf.focus.y - xf.cy))};
        ground.draw(origin, fit.w, fit.h);
        return;
    }
    // Even sizes, so the halving averages exactly 2x2.
    const auto span = [k](int view, int limit) {
        const int n = static_cast<int>(std::ceil(static_cast<float>(view) / k)) + 8;
        return std::min(limit, n + n % 2);
    };
    const int w = span(fit.w, s.ground_w);
    const int h = span(fit.h, s.ground_h);
    const PixelPt origin{static_cast<int>(std::floor(xf.focus.x - static_cast<float>(w) * 0.5f)),
                         static_cast<int>(std::floor(xf.focus.y - static_cast<float>(h) * 0.5f))};
    SDL_SetRenderTarget(r, s.ground);
    ground.draw(origin, w, h);
    SDL_Texture* source = s.ground;
    SDL_FRect src{0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h)};
    if (k < 0.5f) {
        const SDL_FRect half{0.0f, 0.0f, static_cast<float>(w) * 0.5f,
                             static_cast<float>(h) * 0.5f};
        SDL_SetRenderTarget(r, s.ground_half);
        SDL_RenderTexture(r, s.ground, &src, &half);
        source = s.ground_half;
        src = half;
    }
    SDL_SetRenderTarget(r, s.target);
    const SDL_FRect dst{(static_cast<float>(origin.x) - xf.focus.x) * k + xf.cx,
                        (static_cast<float>(origin.y) - xf.focus.y) * k + xf.cy,
                        static_cast<float>(w) * k, static_cast<float>(h) * k};
    SDL_RenderTexture(r, source, &src, &dst);
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
    bool have_file = SDL_GetPathInfo(tf.path.c_str(), &info);
    if (have_file) {
        // A file of an older format holds old numbers that would quietly win over the new defaults:
        // keep it as tuning-format<N>.txt (N its format) and start from the new defaults.
        std::size_t size = 0;
        void* data = SDL_LoadFile(tf.path.c_str(), &size);
        const int format =
            data != nullptr ? tuning_format(std::string_view(static_cast<const char*>(data), size))
                            : kTuningFormat;
        SDL_free(data);
        if (format < kTuningFormat) {
            const std::string old =
                tf.path.substr(0, tf.path.size() - 4) + "-format" + std::to_string(format) + ".txt";
            SDL_RemovePath(old.c_str());
            if (SDL_RenamePath(tf.path.c_str(), old.c_str())) {
                SDL_Log("tuning.txt was from an older version: kept as %s, defaults written",
                        old.c_str());
                have_file = false;
            } else {
                SDL_Log("can't set the old tuning file aside: %s", SDL_GetError());
            }
        }
    }
    if (have_file) {
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

[[nodiscard]] SDL_FPoint on_view(const ViewXf& xf, float x, float y, float z) {
    const ScreenPt p = xf.at(x, y, z);
    return {p.x, p.y};
}

// A small mark sized for full size, `w` x `h` px, at a depth scale (never under 1 px).
[[nodiscard]] SDL_FRect scaled_mark(SDL_FPoint at, float w, float h, float k) {
    const float sw = std::max(1.0f, std::round(w * k));
    const float sh = std::max(1.0f, std::round(h * k));
    return {std::round(at.x - sw * 0.5f), std::round(at.y - sh * 0.5f), sw, sh};
}

// A practice ground target: a white and red bullseye, glowing yellow just after a hit.
void draw_bullseye(SDL_Renderer* r, const ViewXf& xf, const Target& t, float radius) {
    const SDL_FColor white{0.94f, 0.93f, 0.88f, 1.0f};
    const SDL_FColor red{0.78f, 0.16f, 0.12f, 1.0f};
    game::fill_ground_disc(r, xf, t.x, t.y, 0.0f, radius, white);
    game::fill_ground_disc(r, xf, t.x, t.y, 0.0f, radius * 0.7f, red);
    game::fill_ground_disc(r, xf, t.x, t.y, 0.0f, radius * 0.42f, white);
    game::fill_ground_disc(r, xf, t.x, t.y, 0.0f, radius * 0.16f, red);
    if (t.since_hit_s < 0.15f) {
        game::fill_ground_disc(r, xf, t.x, t.y, 0.0f, radius * 1.15f,
                               SDL_FColor{1.0f, 0.9f, 0.3f, 0.7f});
    }
}

// Everything above the ground, in order: scorch marks, ground targets, shadows, dust and blasts,
// bombs, tracers, then the drones and planes from the lowest (the farthest) up. Everything is sized
// by its depth (core/view_xf.hpp).
void draw_scene(SDL_Renderer* r, const ViewXf& xf, const World& world, const Body& plane,
                game::PlaneSprites& sprites, const game::Effects& effects, const Tuning& tuning,
                bool marker) {
    effects.draw_ground(r, xf, tuning.look.scorch_fade_s);
    for (const Target& t : world.targets) {
        if (t.kind == TargetKind::Ground) {
            draw_bullseye(r, xf, t, tuning.rules.targets.ground_radius_u);
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
        sprites.draw(f.body, xf, true);
    }
    for (const Bomb& m : world.bombs) {
        const SDL_FRect shadow =
            scaled_mark(on_view(xf, m.x, m.y, 0.0f), 2.0f, 1.0f, xf.scale_at(0.0f));
        SDL_SetRenderDrawColor(r, 0, 0, 0, 90);
        SDL_RenderFillRect(r, &shadow);
    }
    effects.draw_air(r, xf);
    for (const Bomb& m : world.bombs) {
        const SDL_FRect bomb =
            scaled_mark(on_view(xf, m.x, m.y, m.z), 3.0f, 2.0f, xf.scale_at(m.z));
        SDL_SetRenderDrawColor(r, 36, 38, 30, 255);
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
        const SDL_FPoint a = on_view(xf, round.x, round.y, round.z);
        const SDL_FPoint b = on_view(xf, round.x - round.vx * back, round.y - round.vy * back,
                                     round.z - round.vz * back);
        SDL_RenderLine(r, std::round(a.x), std::round(a.y), std::round(b.x), std::round(b.y));
    }
    if (marker && plane.outcome == Outcome::Flying) {
        const GroundSpot spot = bomb_impact(plane, tuning.rules.weapon, tuning.rules.flight);
        const SDL_FPoint s = on_view(xf, spot.x, spot.y, 0.0f);
        SDL_SetRenderDrawColor(r, 255, 80, 60, 255);
        SDL_RenderLine(r, s.x - 4.0f, s.y, s.x + 4.0f, s.y);
        SDL_RenderLine(r, s.x, s.y - 2.0f, s.x, s.y + 2.0f);
    }
    std::sort(fliers.begin(), fliers.end(), [](const Flier& a, const Flier& b) {
        if (a.body.z != b.body.z) {
            return a.body.z < b.body.z;
        }
        return to_screen(a.body.x, a.body.y).y < to_screen(b.body.x, b.body.y).y;
    });
    for (const Flier& f : fliers) {
        if (!f.drone && f.body.outcome == Outcome::Crashed) {
            continue;
        }
        sprites.draw(f.body, xf, false,
                     f.drone ? SDL_Color{255, 120, 90, 255} : SDL_Color{255, 255, 255, 255},
                     f.flash);
    }
    // A target's hit count, for two seconds after a hit.
    SDL_SetRenderDrawColor(r, 255, 244, 190, 255);
    for (const Target& t : world.targets) {
        if (t.since_hit_s < 2.0f) {
            const SDL_FPoint s = on_view(xf, t.x, t.y, t.z);
            const std::string n = std::to_string(t.hits);
            SDL_RenderDebugText(r, std::round(s.x) - static_cast<float>(n.size()) * 4.0f,
                                std::round(s.y - 22.0f * xf.scale_at(t.z)), n.c_str());
        }
    }
}

// The nation screen (design/planes.md): until PR 7's menus, a plain choice between v0.1's two
// nations. You fly that nation's fighter; the other nation is the enemy. Empty if the window was
// closed or Esc pressed.
std::optional<Nation> choose_nation(SDL_Renderer* r) {
    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                return std::nullopt;
            }
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                if (e.key.key == SDLK_ESCAPE) {
                    return std::nullopt;
                }
                if (e.key.key == SDLK_1 || e.key.key == SDLK_KP_1) {
                    return Nation::US;
                }
                if (e.key.key == SDLK_2 || e.key.key == SDLK_KP_2) {
                    return Nation::Germany;
                }
            }
        }
        int w = 0;
        int h = 0;
        SDL_GetRenderOutputSize(r, &w, &h);
        const ViewFit fit = fit_view(w, h);
        SDL_SetRenderTarget(r, nullptr);
        SDL_SetRenderScale(r, static_cast<float>(fit.scale), static_cast<float>(fit.scale));
        SDL_SetRenderDrawColor(r, 22, 30, 40, 255);
        SDL_RenderClear(r);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        draw_text_centred(r, fit.w, static_cast<float>(fit.h) * 0.5f - 50.0f,
                          {"WINGS OF FREEDOM", "", "CHOOSE YOUR NATION"},
                          SDL_Color{255, 236, 170, 255});
        const PlaneType& us = plane_type(fighter_of(Nation::US));
        const PlaneType& de = plane_type(fighter_of(Nation::Germany));
        draw_text_centred(
            r, fit.w, static_cast<float>(fit.h) * 0.5f,
            {format("1  UNITED STATES  %.*s", static_cast<int>(us.name.size()), us.name.data()),
             format("2  GERMANY        %.*s", static_cast<int>(de.name.size()), de.name.data()), "",
             "The other nation is the enemy."},
            SDL_Color{236, 232, 200, 255});
        SDL_SetRenderScale(r, 1.0f, 1.0f);
        SDL_RenderPresent(r);
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
    case Outcome::ShotDown:
        return "SHOT DOWN";
    case Outcome::PilotKilled:
        return "PILOT KILLED";
    case Outcome::Flying:
        break;
    }
    return "";
}

// Developer tool (--damage): sets parts' HP (and the pilot's damage) from "engine=10,lwing=0,...",
// then lets the damage model react (a lost wing sends the plane down, and so on). False on a bad
// spec.
bool apply_damage_spec(const char* spec, Body& b, const DamageParams& d) {
    std::string s(spec);
    std::size_t at = 0;
    while (at < s.size()) {
        const std::size_t comma = s.find(',', at);
        const std::string item = s.substr(at, comma == std::string::npos ? comma : comma - at);
        at = comma == std::string::npos ? s.size() : comma + 1;
        const std::size_t eq = item.find('=');
        float v = 0.0f;
        if (eq == std::string::npos || !parse_number(item.substr(eq + 1).c_str(), v)) {
            return false;
        }
        const std::string key = item.substr(0, eq);
        if (key == "engine") {
            b.damage.engine = v;
            apply_hit(b, Part::Engine, 0.0f, d);
        } else if (key == "lwing") {
            b.damage.left_wing = v;
            apply_hit(b, Part::LeftWing, 0.0f, d);
        } else if (key == "rwing") {
            b.damage.right_wing = v;
            apply_hit(b, Part::RightWing, 0.0f, d);
        } else if (key == "fuselage") {
            b.damage.fuselage = std::max(0.001f, v);
            apply_hit(b, Part::Fuselage, v <= 0.0f ? 0.001f : 0.0f, d);
        } else if (key == "tail") {
            b.damage.tail = v;
            apply_hit(b, Part::Tail, 0.0f, d);
        } else if (key == "pilot") {
            apply_hit(b, Part::Cockpit, v, d);
        } else {
            return false;
        }
    }
    return true;
}

// What damaged planes leave behind each tick: engine smoke (grey at DANGER, black at CRITICAL or
// when it's gone), a fuel spray while the fuselage leaks, heavy black smoke while going down, and a
// fireball where a plane's flight ends in a crash or in the air.
void emit_trails(const World& w, const std::vector<Body>& before, game::Effects& fx,
                 const Rules& rules) {
    using Trail = game::Effects::Trail;
    for (std::size_t i = 0; i < w.bodies.size(); ++i) {
        const Body& b = w.bodies[i];
        const bool ended_badly = b.outcome == Outcome::Crashed || b.outcome == Outcome::ShotDown ||
                                 b.outcome == Outcome::PilotKilled;
        if (i < before.size() && before[i].outcome == Outcome::Flying && ended_badly) {
            fx.explode(b.x, b.y, b.z, 16.0f);
        }
        if (b.outcome != Outcome::Flying) {
            continue;
        }
        const DamageParams& d = plane_of(rules, b).damage;
        // Puffs come from just behind the nose (or a twin's engine on each wing), so they trail as
        // the plane flies on.
        const float h = b.heading_deg * 3.14159265f / 180.0f;
        const float x = b.x + std::sin(h) * 3.0f;
        const float y = b.y + std::cos(h) * 3.0f;
        const std::int64_t t = w.tick;
        if (b.damage.going_down) {
            if (t % 2 == 0) {
                fx.trail(x, y, b.z, Trail::Black);
            }
        } else {
            const bool twin = b.damage.engines > 1;
            for (const Part engine_part : {Part::Engine, Part::Engine2}) {
                if (engine_part == Part::Engine2 && !twin) {
                    continue;
                }
                // A twin's engines sit out on the wings: left (Engine) and right (Engine2).
                const float side = !twin ? 0.0f : (engine_part == Part::Engine ? -5.0f : 5.0f);
                const float ex = x + std::cos(h) * side;
                const float ey = y - std::sin(h) * side;
                const Stage engine = part_stage(b, engine_part, d);
                if (engine == Stage::Critical || engine == Stage::Destroyed) {
                    if (t % 3 == 0) {
                        fx.trail(ex, ey, b.z, Trail::Black);
                    }
                } else if (engine == Stage::Danger && t % 6 == 0) {
                    fx.trail(ex, ey, b.z, Trail::Grey);
                }
            }
        }
        if (b.damage.fuselage > 0.0f && b.damage.fuselage < d.fuselage_hp && b.fuel > 0.0f &&
            t % 4 == 0) {
            fx.trail(b.x - std::sin(h) * 4.0f, b.y - std::cos(h) * 4.0f, b.z - 1.0f, Trail::Spray);
        }
    }
}

// Rounds left per gun type, for the readout: ".30 500  .50 200".
std::string guns_text(const Body& b, const PlaneRules& plane) {
    std::string out;
    for (int c = 0; c < kCalibres; ++c) {
        const auto cal = static_cast<Calibre>(c);
        bool carried = false;
        for (int g = 0; g < plane.gun_count; ++g) {
            const GunSpec& s = plane.guns[static_cast<std::size_t>(g)];
            carried = carried || (s.mount == Mount::Fixed && s.calibre == cal);
        }
        if (carried) {
            out += format("%s%s %d", out.empty() ? "" : "  ", calibre_name(cal),
                          rounds_left(b, plane, cal));
        }
    }
    return out.empty() ? std::string("NO GUNS") : out;
}

// The colour a part's stage is shown in.
SDL_Color stage_colour(Stage s) {
    switch (s) {
    case Stage::Ok:
        return {120, 220, 120, 255};
    case Stage::Warning:
        return {240, 220, 90, 255};
    case Stage::Danger:
        return {255, 160, 60, 255};
    case Stage::Critical:
        return {255, 80, 60, 255};
    case Stage::Destroyed:
        return {150, 150, 150, 255};
    }
    return {255, 255, 255, 255};
}

// A line of text in coloured pieces.
struct Piece {
    std::string text;
    SDL_Color colour;
};
using RichLine = std::vector<Piece>;

// Lines of coloured pieces on a dark backing, with the block's bottom-left at (x, bottom).
void draw_rich_block(SDL_Renderer* r, float x, float bottom, const std::vector<RichLine>& lines) {
    std::size_t widest = 0;
    for (const RichLine& l : lines) {
        std::size_t n = 0;
        for (const Piece& p : l) {
            n += p.text.size();
        }
        widest = std::max(widest, n);
    }
    const float h = static_cast<float>(lines.size()) * 10.0f + 2.0f;
    const SDL_FRect back{x, bottom - h, static_cast<float>(widest) * kGlyph + 4.0f, h};
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_RenderFillRect(r, &back);
    float y = bottom - h + 2.0f;
    for (const RichLine& l : lines) {
        float cx = x + 2.0f;
        for (const Piece& p : l) {
            SDL_SetRenderDrawColor(r, p.colour.r, p.colour.g, p.colour.b, p.colour.a);
            SDL_RenderDebugText(r, cx, y, p.text.c_str());
            cx += static_cast<float>(p.text.size()) * kGlyph;
        }
        y += 10.0f;
    }
}

// The damage line: each part's stage in its colour.
RichLine damage_line(const Body& b, const DamageParams& d) {
    const SDL_Color label{236, 232, 200, 255};
    RichLine line;
    const auto add = [&](const char* name, Part p) {
        const Stage s = part_stage(b, p, d);
        line.push_back(Piece{name, label});
        line.push_back(Piece{stage_name(s), stage_colour(s)});
        line.push_back(Piece{"  ", label});
    };
    if (b.damage.engines > 1) {
        add("ENG L ", Part::Engine);
        add("R ", Part::Engine2);
    } else {
        add("ENG ", Part::Engine);
    }
    add("LW ", Part::LeftWing);
    add("RW ", Part::RightWing);
    add("FUS ", Part::Fuselage);
    add("TAIL ", Part::Tail);
    line.push_back(Piece{"PILOT ", label});
    line.push_back(Piece{std::to_string(b.damage.wounds) + "/" + std::to_string(d.pilot_wounds),
                         stage_colour(part_stage(b, Part::Cockpit, d))});
    return line;
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
        // The player's nation (design/planes.md): chosen on the nation screen, or by --nation.
        Nation nation = opts.has_nation ? opts.nation : Nation::US;
        if (!opts.has_nation && !tool_mode) {
            const std::optional<Nation> chosen = choose_nation(renderer);
            if (!chosen) {
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                SDL_Quit();
                return 0;
            }
            nation = *chosen;
        }
        const ModelId player_model = plane_type(fighter_of(nation)).model;
        if (opts.sprite_sheet != nullptr) {
            const bool ok =
                game::PlaneSprites::save_sheet(opts.sprite_sheet, player_model, opts.sheet_step);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return ok ? 0 : 1;
        }
        // The player's plane and the practice drones' (the P-26's model): full size now, the other
        // sizes in the background (in line for a screenshot, so it's exact).
        game::PlaneSprites sprites(renderer, shot_mode);
        if (!sprites.bake(player_model) || !sprites.bake(ModelId::P26)) {
            exit_code = 1;
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
        std::uint64_t runs = 0;
        const auto new_run = [&]() {
            const float x =
                opts.has_at ? static_cast<float>(opts.at_x) : static_cast<float>(kMapWidthU) * 0.5f;
            const float y = opts.has_at ? static_cast<float>(opts.at_y) : 480.0f;
            // The nation's fighter, with its real stats, guns and bombs.
            Body b = new_plane(tuning.rules, fighter_of(nation), x, y, opts.heading);
            const PlaneRules& plane = plane_of(tuning.rules, b);
            if (opts.has_height) {
                b.z = std::fmin(opts.height, plane.flight.ceiling_u);
            }
            if (opts.has_pitch) {
                b.pitch_deg = opts.pitch;
            }
            if (opts.has_throttle) {
                b.throttle = opts.throttle_pct / 100.0f;
            }
            if (opts.damage != nullptr && !apply_damage_spec(opts.damage, b, plane.damage)) {
                SDL_Log("--damage: expected engine=, lwing=, rwing=, fuselage=, tail= or pilot= "
                        "with numbers, separated by commas");
            }
            world = World{};
            world.rng = Rng(splitmix64(seed + runs++)); // where hits land, different each run
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
                    } else if (!run_over && e.key.key == SDLK_F5) {
                        // Developer tool: a gun burst from a random side.
                        constexpr std::array<HitFrom, 4> kSides = {HitFrom::Ahead, HitFrom::Behind,
                                                                   HitFrom::Left, HitFrom::Right};
                        const HitFrom from = kSides[world.rng.next_below(4)];
                        debug_hits(world, 0, from, tuning.rules.weapon.calibres[0].damage, 5,
                                   tuning.rules);
                        effects.add(world.events, tuning.rules.weapon.blast_radius_u);
                    } else if (!run_over && e.key.key == SDLK_F6) {
                        // Developer tool: a flak-sized hit (design/flight.md, Flak guns).
                        debug_hits(world, 0, HitFrom::Burst, 12.0f, 1, tuning.rules);
                    } else if (!run_over && e.key.key == SDLK_F7) {
                        repair(world.bodies[0], plane_of(tuning.rules, world.bodies[0]).damage);
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
            const float frame_s = std::min(0.25f, static_cast<float>(now - last) / 1e9f);
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
                emit_trails(world, previous, effects, tuning.rules);
            }
            if (world.bodies[0].outcome != Outcome::Flying && !shot_mode) {
                // The run is over and the world waits, but the fire and smoke play out.
                effects.tick(frame_s, tuning.look.scorch_fade_s);
            }
            const float alpha = shot_mode ? 1.0f : step.alpha();

            ++fps_frames;
            if (now - fps_since >= 1'000'000'000) {
                fps = fps_frames;
                fps_frames = 0;
                fps_since = now;
            }

            // The world into the view target: 1:1 in art pixels at your height, the ground
            // shrinking as you climb and everything else by its depth (design/planes.md).
            const ViewFit& fit = screen.fit;
            const Body plane = between(previous[0], world.bodies[0], alpha);
            const ViewXf xf = make_view(camera.focus(alpha), plane.z, tuning.rules.flight.ceiling_u,
                                        tuning.look.zoom_at_ceiling, fit.w, fit.h);
            sprites.pump();
            SDL_SetRenderTarget(renderer, screen.target);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            draw_ground(renderer, screen, ground, xf);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            draw_scene(renderer, xf, world, plane, sprites, effects, tuning, marker);

            // A temporary readout until PR 6's HUD: throttle, speed, height, fuel, warnings.
            std::string flags_line;
            if (plane.stalled) {
                flags_line += "STALL  ";
            }
            if (!plane.engine_on) {
                flags_line += "ENGINE OFF  ";
            }
            if (plane.damage.going_down) {
                flags_line += "GOING DOWN  ";
            }
            if (own_blast_s > 0.0f) {
                flags_line += format("OWN BLAST -%.0f", static_cast<double>(own_blast_damage));
            }
            int hits = 0;
            for (const Target& t : world.targets) {
                hits += t.hits;
            }
            const SDL_Color text{236, 232, 200, 255};
            std::vector<RichLine> readout = {
                {Piece{format("THR %3.0f%%  %3.0f MPH  %5.0f FT  FUEL %3.0f%%",
                              static_cast<double>(plane.throttle * 100.0f),
                              static_cast<double>(mph_of(plane.speed_u_s, tuning.rules.scale)),
                              static_cast<double>(plane.z * kFeetPerU),
                              static_cast<double>(plane.fuel * 100.0f)),
                       text}},
                {Piece{format("%s  BOMBS %d  [%s]  RULE %s  HITS %d",
                              guns_text(plane, plane_of(tuning.rules, plane)).c_str(),
                              plane.loadout.bombs, heavy_name(plane.loadout.heavy),
                              height_rule_name(height_rule), hits),
                       text}},
                damage_line(plane, plane_of(tuning.rules, plane).damage)};
            if (!flags_line.empty()) {
                readout.push_back({Piece{flags_line, SDL_Color{255, 150, 110, 255}}});
            }
            draw_rich_block(renderer, 2.0f, static_cast<float>(fit.h) - 2.0f, readout);
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
                     format("camera %.0f,%.0f  ground %.2f  lead %.0f,%.0f u",
                            static_cast<double>(xf.focus.x), static_cast<double>(xf.focus.y),
                            static_cast<double>(xf.scale_at(0.0f)),
                            static_cast<double>(camera.lead().x),
                            static_cast<double>(camera.lead().y)),
                     format("%.*s  %.0f,%.0f  %.0f u  %.1f u/s",
                            static_cast<int>(plane_type(plane.type).name.size()),
                            plane_type(plane.type).name.data(), static_cast<double>(plane.x),
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
                     format("ground %s  tiles %d  tick %lld", ground_name(under.kind),
                            ground.cached(), static_cast<long long>(world.tick)),
                     format("sprites %.0f ms  ready %d  queued %d", sprites.bake_ms(),
                            sprites.ready(), sprites.queued()),
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
        if (screen.ground != nullptr) {
            SDL_DestroyTexture(screen.ground);
        }
        if (screen.ground_half != nullptr) {
            SDL_DestroyTexture(screen.ground_half);
        }
        sprites.shutdown();
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exit_code;
}
