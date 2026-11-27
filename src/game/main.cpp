// Wings of Freedom: SDL3 front end (design/flight.md). The world is drawn 1:1 in art pixels into a
// render target the size of the view, then shown at the largest whole-number scale that fits the
// window, so the pixels stay square and crisp. Screens that 270 doesn't divide evenly show more of
// the world.
//
//   WingsOfFreedom [--seed N] [--window WxH] [--shot FILE.png] [--ticks N] [--heading DEG]
//                  [--height U] [--pitch DEG] [--throttle PCT] [--at X,Y] [--overlay]
//                  [--height-rule forgiving|strict|auto] [--targets] [--wave N] [--fire] [--bomb]
//                  [--nation us|de] [--plane ID] [--sprite-sheet FILE.png] [--sprite-scale PCT]
//                  [--version]
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
//     --height-rule R your gun aim: forgiving, strict or auto (else settings.txt's)
//     --menu PAGE     developer tool: start with a menu page open (pause, options, controls), or
//                     with --shot, planes: the plane screen
//     --panel N       the dashboard 96, 80 or 64 px tall (else settings.txt)
//     --units U       arrows and banners in nation, imperial or metric units (else settings.txt)
//     --targets       developer tool: start with the practice targets (also F2)
//     --wave N        developer tool: the first wave is wave N
//     --fire, --bomb  developer tool: with --shot, hold the guns or the bomb release throughout
//     --sprite-sheet F  developer tool: save every baked pose of your fighter to F (PNG) and exit
//     --sprite-scale PCT  developer tool: the sprite sheet's size, 25-150 (in steps of 1/8)
//     --nation us|de  fly that nation's first fighter, skipping the plane screen
//     --plane ID      fly that fighter: p12e, p26a, p36a (US), ar65, he51b, bf109b (Germany)
//     --version       print the version and exit
//
// Keys (design/flight.md, Controls; rebindable in the menu): A/D turn, W dive, S climb, Shift/Ctrl
// throttle up/down, F guns, Space bombs (hold to ripple), Tab switch heavy weapon, Esc the pause
// menu. Developer keys: F1 overlay, F2 practice targets, F3 your gun aim, F4 bomb marker, F11
// fullscreen. When a run ends, Enter or R
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
#include "core/dashboard.hpp"
#include "core/fixed_step.hpp"
#include "core/flight.hpp"
#include "core/ground.hpp"
#include "core/hash.hpp"
#include "core/iso.hpp"
#include "core/map.hpp"
#include "core/menu.hpp"
#include "core/rng.hpp"
#include "core/settings.hpp"
#include "core/targets.hpp"
#include "core/tuning.hpp"
#include "core/units.hpp"
#include "core/view.hpp"
#include "core/view_xf.hpp"
#include "core/weapons.hpp"
#include "core/world.hpp"
#include "game/bindings.hpp"
#include "game/dashboard_view.hpp"
#include "game/effects.hpp"
#include "game/ground_view.hpp"
#include "game/menu_view.hpp"
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
    bool has_height_rule = false; // --height-rule: your gun aim, over settings.txt's
    HeightRule height_rule = HeightRule::Forgiving;
    MenuPage menu = MenuPage::Closed; // --menu: start with a menu page open (for screenshots)
    bool targets = false;             // practice targets (off since the enemies came)
    int wave = 1;                     // --wave N: the first wave's number
    bool hold_fire = false;           // --shot: hold the guns
    bool hold_bomb = false;           // --shot: hold the bomb release
    const char* damage = nullptr;     // --damage engine=10,lwing=0,...
    bool has_nation = false;          // --nation us|de: skip the plane screen
    bool has_plane = false;           // --plane ID: the fighter, skipping the plane screen
    bool show_planes = false;         // --menu planes: --shot the plane screen
    PlaneId plane = PlaneId::P12E;
    Nation nation = Nation::US;
    const char* sprite_sheet = nullptr;
    int sheet_step = kFullSizeStep; // --sprite-scale
    bool has_panel = false;         // --panel 96|80|64: the dashboard's height
    PanelSize panel = PanelSize::Large;
    bool has_units = false; // --units: for arrows and banners
    Units units = Units::Nation;
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
            out.has_height_rule = true;
        } else if (std::strcmp(argv[i], "--menu") == 0 && has_value) {
            const std::string page = argv[++i];
            out.show_planes = page == "planes";
            out.menu = page == "pause"      ? MenuPage::Pause
                       : page == "options"  ? MenuPage::Options
                       : page == "controls" ? MenuPage::Controls
                                            : MenuPage::Closed;
            if (out.menu == MenuPage::Closed && !out.show_planes) {
                std::fprintf(stderr, "--menu takes pause, options, controls or planes\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--no-targets") == 0) {
            out.targets = false;
        } else if (std::strcmp(argv[i], "--targets") == 0) {
            out.targets = true;
        } else if (std::strcmp(argv[i], "--wave") == 0 && has_value) {
            float n = 0.0f;
            if (!parse_number(argv[++i], n) || n < 1.0f || n > 99.0f) {
                std::fprintf(stderr, "--wave takes a wave number, 1-99\n");
                return 2;
            }
            out.wave = static_cast<int>(n);
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
        } else if (std::strcmp(argv[i], "--plane") == 0 && has_value) {
            const char* id = argv[++i];
            out.has_plane =
                find_plane(id, out.plane) && plane_type(out.plane).role == Role::Fighter;
            if (!out.has_plane) {
                std::fprintf(stderr, "--plane takes p12e, p26a, p36a, ar65, he51b or bf109b\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--panel") == 0 && has_value) {
            const std::string v = argv[++i];
            out.has_panel = v == "96" || v == "80" || v == "64";
            out.panel = v == "80"   ? PanelSize::Medium
                        : v == "64" ? PanelSize::Small
                                    : PanelSize::Large;
            if (!out.has_panel) {
                std::fprintf(stderr, "--panel takes 96, 80 or 64\n");
                return 2;
            }
        } else if (std::strcmp(argv[i], "--units") == 0 && has_value) {
            const std::string v = argv[++i];
            out.has_units = v == "nation" || v == "imperial" || v == "metric";
            out.units = v == "imperial" ? Units::Imperial
                        : v == "metric" ? Units::Metric
                                        : Units::Nation;
            if (!out.has_units) {
                std::fprintf(stderr, "--units takes nation, imperial or metric\n");
                return 2;
            }
        } else {
            std::fprintf(stderr,
                         "usage: WingsOfFreedom [--seed N] [--window WxH] [--shot FILE.png] "
                         "[--ticks N] [--heading DEG] [--height U] [--pitch DEG] [--throttle PCT] "
                         "[--at X,Y] [--overlay] [--height-rule forgiving|strict|auto] "
                         "[--targets] [--wave N] [--menu pause|options|controls] [--fire] [--bomb] "
                         "[--nation us|de] [--plane ID] [--panel 96|80|64] "
                         "[--units nation|imperial|metric] "
                         "[--sprite-sheet FILE.png] "
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
void draw_ground(SDL_Renderer* r, const Screen& s, game::GroundView& ground, const ViewXf& xf,
                 int view_w, int view_h) {
    const float k = xf.scale_at(0.0f);
    if (k >= 0.999f) {
        const PixelPt origin{static_cast<int>(std::lround(xf.focus.x - xf.cx)),
                             static_cast<int>(std::lround(xf.focus.y - xf.cy))};
        ground.draw(origin, view_w, view_h);
        return;
    }
    // Even sizes, so the halving averages exactly 2x2.
    const auto span = [k](int view, int limit) {
        const int n = static_cast<int>(std::ceil(static_cast<float>(view) / k)) + 8;
        return std::min(limit, n + n % 2);
    };
    const int w = span(view_w, s.ground_w);
    const int h = span(view_h, s.ground_h);
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

// settings.txt next to the game: the options menu's choices, read at start.
void load_settings(Settings& s, const std::string& path) {
    std::size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (data == nullptr) {
        return; // none yet: the defaults
    }
    std::string errors;
    if (!from_settings(std::string_view(static_cast<const char*>(data), size), s, errors)) {
        SDL_Log("settings file has problems:\n%s", errors.c_str());
    }
    SDL_free(data);
}

void save_settings(const Settings& s, const std::string& path) {
    const std::string text = to_settings(s);
    if (!SDL_SaveFile(path.c_str(), text.data(), text.size())) {
        SDL_Log("can't save %s: %s", path.c_str(), SDL_GetError());
    }
}

// What a key does in a menu.
std::optional<MenuInput> menu_input_of(SDL_Scancode key) {
    switch (key) {
    case SDL_SCANCODE_UP:
    case SDL_SCANCODE_W:
        return MenuInput::Up;
    case SDL_SCANCODE_DOWN:
    case SDL_SCANCODE_S:
        return MenuInput::Down;
    case SDL_SCANCODE_LEFT:
    case SDL_SCANCODE_A:
        return MenuInput::Left;
    case SDL_SCANCODE_RIGHT:
    case SDL_SCANCODE_D:
        return MenuInput::Right;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
    case SDL_SCANCODE_SPACE:
        return MenuInput::Select;
    case SDL_SCANCODE_ESCAPE:
        return MenuInput::Back;
    case SDL_SCANCODE_DELETE:
        return MenuInput::Clear;
    default:
        return std::nullopt;
    }
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

// Arrows at the view's edge toward the enemies off screen: one per direction, with a mark for
// higher, level (within about 1,000 ft) or lower than you (design/flight.md, Enemies). `view_h` is
// the world's rows, above the dashboard.
void draw_enemy_arrows(SDL_Renderer* r, const ViewXf& xf, const Body& plane,
                       const std::vector<Body>& enemies, int view_w, int view_h) {
    std::vector<Sighting> seen;
    std::vector<EdgeArrow> arrows;
    const float cx = static_cast<float>(view_w) * 0.5f;
    const float cy = static_cast<float>(view_h) * 0.5f;
    for (const Body& b : enemies) {
        if (b.outcome != Outcome::Flying) {
            continue;
        }
        const SDL_FPoint at = on_view(xf, b.x, b.y, b.z);
        const EdgeArrow a = edge_arrow(at.x, at.y, view_w, view_h, 12.0f);
        if (a.on_screen) {
            continue;
        }
        seen.push_back({std::atan2(at.y - cy, at.x - cx), std::hypot(b.x - plane.x, b.y - plane.y),
                        b.z - plane.z});
        arrows.push_back(a);
    }
    const SDL_FColor red{1.0f, 0.38f, 0.25f, 1.0f};
    const SDL_FColor mark_colour{1.0f, 0.8f, 0.7f, 1.0f};
    const auto triangle = [r](SDL_FPoint a, SDL_FPoint b, SDL_FPoint c, SDL_FColor colour) {
        const std::array<SDL_Vertex, 3> v{
            {{a, colour, {0.0f, 0.0f}}, {b, colour, {0.0f, 0.0f}}, {c, colour, {0.0f, 0.0f}}}};
        SDL_RenderGeometry(r, nullptr, v.data(), 3, nullptr, 0);
    };
    // 0.45 rad: about 25 degrees either way share an arrow; 20 u is 1,000 ft.
    for (const ArrowGroup& g : group_arrows(seen, 0.45f, 20.0f)) {
        const EdgeArrow& a = arrows[g.nearest];
        const float c = std::cos(a.angle_rad);
        const float s = std::sin(a.angle_rad);
        triangle({a.x + c * 7.0f, a.y + s * 7.0f},
                 {a.x - c * 3.0f - s * 5.0f, a.y - s * 3.0f + c * 5.0f},
                 {a.x - c * 3.0f + s * 5.0f, a.y - s * 3.0f - c * 5.0f}, red);
        // The height mark, just inside the arrow.
        const float x = std::round(std::clamp(a.x - c * 14.0f, 6.0f, cx * 2.0f - 6.0f));
        const float y = std::round(std::clamp(a.y - s * 14.0f, 6.0f, cy * 2.0f - 6.0f));
        const SDL_FRect back{x - 6.0f, y - 6.0f, 12.0f, 12.0f};
        SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
        SDL_RenderFillRect(r, &back);
        if (g.mark == HeightMark::Higher) {
            triangle({x, y - 4.0f}, {x + 4.0f, y + 3.0f}, {x - 4.0f, y + 3.0f}, mark_colour);
        } else if (g.mark == HeightMark::Lower) {
            triangle({x, y + 4.0f}, {x - 4.0f, y - 3.0f}, {x + 4.0f, y - 3.0f}, mark_colour);
        } else {
            SDL_SetRenderDrawColor(r, 255, 204, 178, 255);
            const SDL_FRect upper{x - 4.0f, y - 3.0f, 8.0f, 2.0f};
            const SDL_FRect lower{x - 4.0f, y + 1.0f, 8.0f, 2.0f};
            SDL_RenderFillRect(r, &upper);
            SDL_RenderFillRect(r, &lower);
        }
    }
}

// F1: each enemy's style and what it's doing, above its plane.
void draw_pilot_labels(SDL_Renderer* r, const ViewXf& xf, const World& world,
                       const std::vector<Body>& enemies) {
    for (const Pilot& p : world.pilots) {
        const auto i = static_cast<std::size_t>(p.body) - 1;
        if (i >= enemies.size() || enemies[i].outcome != Outcome::Flying) {
            continue;
        }
        const Body& b = enemies[i];
        const SDL_FPoint at = on_view(xf, b.x, b.y, b.z);
        const std::string text = format("%s: %s", style_name(p.style), phase_name(p.doing));
        draw_text_block(r, std::round(at.x - static_cast<float>(text.size()) * kGlyph * 0.5f),
                        std::round(at.y - 34.0f), {text}, SDL_Color{255, 190, 160, 255});
    }
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
// by its depth (core/view_xf.hpp). `plane` is the player's and `enemies` the others (bodies 1 on),
// as drawn this frame.
void draw_scene(SDL_Renderer* r, const ViewXf& xf, const World& world, const Body& plane,
                const std::vector<Body>& enemies, game::PlaneSprites& sprites,
                const game::Effects& effects, const Tuning& tuning, bool marker) {
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
    for (const Body& b : enemies) {
        // A hit this tick: the plane flashes.
        const auto index = static_cast<int>(&b - enemies.data()) + 1;
        const bool hit = std::any_of(world.events.begin(), world.events.end(), [&](const Event& e) {
            return e.kind == EventKind::RoundHitPlane && e.who == index;
        });
        if (b.outcome == Outcome::Flying) {
            fliers.push_back(Flier{b, false, hit});
        }
    }
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

// A mouse event's position in art pixels (the view, shown at fit.scale from the window's corner).
SDL_FPoint mouse_view(SDL_Renderer* r, const SDL_Event& e, const ViewFit& fit) {
    SDL_Event c = e;
    SDL_ConvertEventToRenderCoordinates(r, &c);
    const float x = c.type == SDL_EVENT_MOUSE_MOTION ? c.motion.x : c.button.x;
    const float y = c.type == SDL_EVENT_MOUSE_MOTION ? c.motion.y : c.button.y;
    const auto scale = static_cast<float>(fit.scale);
    return {x / scale, y / scale};
}

// The plane screen (design/aircraft_tree.md): until the hangar, every fighter of both nations,
// listed along its tree, by 1-6, the arrows and Enter, or the mouse. The other nation is the
// enemy. Empty if the window was closed or Esc pressed. With `shot`, saves the screen there and
// returns instead.
std::optional<PlaneId> choose_plane(SDL_Renderer* r, const char* shot = nullptr) {
    const std::array<Nation, 2> nations{Nation::US, Nation::Germany};
    std::vector<PlaneId> planes;          // the choices, in order
    std::vector<std::size_t> plane_lines; // the line each is on
    std::vector<std::string> lines;
    for (const Nation n : nations) {
        lines.emplace_back(n == Nation::US ? "UNITED STATES" : "GERMANY");
        for (const PlaneId id : fighter_tree(n)) {
            const PlaneType& t = plane_type(id);
            plane_lines.push_back(lines.size());
            planes.push_back(id);
            lines.push_back(format("%d  %-22.*s %d  %3.0f MPH", static_cast<int>(planes.size()),
                                   static_cast<int>(t.name.size()), t.name.data(), t.year,
                                   static_cast<double>(t.top_mph)));
        }
        lines.emplace_back("");
    }
    lines.emplace_back("The other nation is the enemy.");
    const int count = static_cast<int>(planes.size());
    int pick = 0;
    for (;;) {
        int w = 0;
        int h = 0;
        SDL_GetRenderOutputSize(r, &w, &h);
        const ViewFit fit = fit_view(w, h);
        const float list_y = std::floor(static_cast<float>(fit.h) * 0.5f - 50.0f);
        // The plane's line under a view position, or -1 (a line is 10 px, from 2 px down).
        const auto line_at = [&](float y) {
            const float row = (y - list_y - 2.0f) / 10.0f;
            for (int i = 0; i < count; ++i) {
                const auto line = static_cast<float>(plane_lines[static_cast<std::size_t>(i)]);
                if (row >= line && row < line + 1.0f) {
                    return i;
                }
            }
            return -1;
        };
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                return std::nullopt;
            }
            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                const SDL_Scancode key = e.key.scancode;
                if (key == SDL_SCANCODE_ESCAPE) {
                    return std::nullopt;
                }
                if (key >= SDL_SCANCODE_1 && key < SDL_SCANCODE_1 + count) {
                    return planes[static_cast<std::size_t>(key - SDL_SCANCODE_1)];
                }
                if (key >= SDL_SCANCODE_KP_1 && key < SDL_SCANCODE_KP_1 + count) {
                    return planes[static_cast<std::size_t>(key - SDL_SCANCODE_KP_1)];
                }
                if (key == SDL_SCANCODE_UP || key == SDL_SCANCODE_W) {
                    pick = (pick + count - 1) % count;
                } else if (key == SDL_SCANCODE_DOWN || key == SDL_SCANCODE_S) {
                    pick = (pick + 1) % count;
                } else if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER ||
                           key == SDL_SCANCODE_SPACE) {
                    return planes[static_cast<std::size_t>(pick)];
                }
            } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
                const int at = line_at(mouse_view(r, e, fit).y);
                pick = at >= 0 ? at : pick;
            } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                       e.button.button == SDL_BUTTON_LEFT) {
                if (const int at = line_at(mouse_view(r, e, fit).y); at >= 0) {
                    return planes[static_cast<std::size_t>(at)];
                }
            }
        }
        SDL_SetRenderTarget(r, nullptr);
        SDL_SetRenderScale(r, static_cast<float>(fit.scale), static_cast<float>(fit.scale));
        SDL_SetRenderDrawColor(r, 22, 30, 40, 255);
        SDL_RenderClear(r);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        draw_text_centred(r, fit.w, list_y - 30.0f, {"WINGS OF FREEDOM", "CHOOSE YOUR PLANE"},
                          SDL_Color{255, 236, 170, 255});
        draw_text_centred(r, fit.w, list_y, lines, SDL_Color{236, 232, 200, 255});
        // The highlighted plane.
        SDL_SetRenderDrawColor(r, 255, 220, 120, 70);
        const auto line = static_cast<float>(plane_lines[static_cast<std::size_t>(pick)]);
        const SDL_FRect bar{std::floor(static_cast<float>(fit.w) * 0.5f) - 170.0f,
                            list_y + 1.0f + line * 10.0f, 340.0f, 10.0f};
        SDL_RenderFillRect(r, &bar);
        SDL_SetRenderScale(r, 1.0f, 1.0f);
        if (shot != nullptr) {
            return save_png(r, shot) ? std::optional<PlaneId>{planes.front()} : std::nullopt;
        }
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

// The computer's time of day, for the cockpit clock.
struct ClockTime {
    int hour = 0;
    int minute = 0;
};
ClockTime local_time() {
    SDL_Time now = 0;
    SDL_DateTime date{};
    if (!SDL_GetCurrentTime(&now) || !SDL_TimeToDateTime(now, &date, true)) {
        return {};
    }
    return {date.hour, date.minute};
}

// The dashboard's readings for the player's plane (`plane` is it as drawn, between `before` and
// now). The lamps and message the game adds itself.
PanelState panel_state(const World& w, const Body& before, const Body& plane, const Rules& rules,
                       Nation nation) {
    PanelState s;
    const Body& now = w.bodies.front();
    const PlaneRules& pr = plane_of(rules, now);
    s.nation = nation;
    s.speed_mph = mph_of(plane.speed_u_s, rules.scale);
    s.height_u = plane.z;
    s.heading_deg = plane.heading_deg;
    s.turn_deg_s = angle_between(before.heading_deg, now.heading_deg) * kTicksPerSecond;
    s.vz_u_s = (now.z - before.z) * kTicksPerSecond;
    s.pitch_deg = plane.pitch_deg;
    s.assist_deg = rules.flight.assist_zone_deg;
    s.throttle = plane.throttle;
    s.engines = std::clamp(now.damage.engines, 1, 2);
    for (int e = 0; e < s.engines; ++e) {
        const auto i = static_cast<std::size_t>(e);
        const float hp = e == 0 ? now.damage.engine : now.damage.engine2;
        s.engine_share[i] =
            pr.damage.engine_hp > 0.0f ? std::clamp(hp / pr.damage.engine_hp, 0.0f, 1.0f) : 0.0f;
        s.engine_running[i] = now.engine_on && s.engine_share[i] > 0.0f;
    }
    s.fuel = now.fuel;
    s.run_s = static_cast<float>(w.tick) / static_cast<float>(kTicksPerSecond);
    // Rounds per gun calibre the pilot fires.
    for (int c = 0; c < kCalibres && s.ammo_count < static_cast<int>(s.ammo.size()); ++c) {
        const auto cal = static_cast<Calibre>(c);
        bool carried = false;
        for (int g = 0; g < pr.gun_count; ++g) {
            const GunSpec& gun = pr.guns[static_cast<std::size_t>(g)];
            carried = carried || (gun.mount == Mount::Fixed && gun.calibre == cal);
        }
        if (carried) {
            // German panels name the rifle calibre in millimetres.
            const bool mm = cal == Calibre::Rifle && nation_units(nation) == UnitSystem::Metric;
            s.ammo[static_cast<std::size_t>(s.ammo_count++)] = {mm ? "7.9" : calibre_name(cal),
                                                                rounds_left(now, pr, cal)};
        }
    }
    s.bombs = now.loadout.bombs;
    s.bombs_full = pr.bombs;
    s.stall = now.stalled;
    s.low_fuel = now.fuel < 0.15f;
    s.edge = now.edge_warning || now.edge_countdown_s >= 0.0f;
    s.going_down = now.damage.going_down;
    for (int p = 0; p < kParts; ++p) {
        s.stages[static_cast<std::size_t>(p)] = part_stage(now, static_cast<Part>(p), pr.damage);
    }
    s.model = plane_type(now.type).model;
    s.tick = static_cast<std::uint32_t>(w.tick);
    return s;
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
        // The player's fighter (design/aircraft_tree.md): chosen on the plane screen, or by
        // --plane, or --nation (its first fighter).
        PlaneId player_type =
            opts.has_plane ? opts.plane
                           : fighter_tree(opts.has_nation ? opts.nation : Nation::US).front();
        if (opts.show_planes && shot_mode) {
            const bool ok = choose_plane(renderer, opts.shot).has_value();
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return ok ? 0 : 1;
        }
        if (!opts.has_plane && !opts.has_nation && !tool_mode) {
            const std::optional<PlaneId> chosen = choose_plane(renderer);
            if (!chosen) {
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                SDL_Quit();
                return 0;
            }
            player_type = *chosen;
        }
        const Nation nation = plane_type(player_type).nation;
        const Nation enemy_nation = nation == Nation::US ? Nation::Germany : Nation::US;
        const FighterTree enemy_tree = fighter_tree(enemy_nation);
        const ModelId player_model = plane_type(player_type).model;
        if (opts.sprite_sheet != nullptr) {
            const bool ok =
                game::PlaneSprites::save_sheet(opts.sprite_sheet, player_model, opts.sheet_step);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return ok ? 0 : 1;
        }
        // The planes' sprites: full size now, the other sizes in the background (in line for a
        // screenshot, so it's exact).
        game::PlaneSprites sprites(renderer, shot_mode);
        // Yours and the enemy's first fighter at every size; the enemy's later ones start with
        // their full size, in the background, and bake their other sizes as they're seen. The
        // practice drones (the P-26) too, when they're on.
        if (!sprites.bake(player_model) || !sprites.bake(plane_type(enemy_tree[0]).model) ||
            (opts.targets && !sprites.bake(ModelId::P26))) {
            exit_code = 1;
        }
        for (std::size_t step = 1; step < enemy_tree.size(); ++step) {
            sprites.queue(plane_type(enemy_tree[step]).model);
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
        // The player's choices (the options menu); screenshots use the defaults.
        const std::string settings_path =
            std::string(base_path != nullptr ? base_path : "") + "settings.txt";
        Settings settings;
        if (!shot_mode) {
            load_settings(settings, settings_path);
        }
        if (opts.has_height_rule) {
            settings.your_aim = opts.height_rule;
        }
        Menu menu;
        if (opts.menu != MenuPage::Closed) {
            open_menu(menu);
            menu.page = opts.menu;
        }
        if (opts.has_panel) {
            settings.panel = opts.panel;
        }
        if (opts.has_units) {
            settings.units = opts.units;
        }
        Screen screen;
        // The rows of the view the world has, above the dashboard.
        const auto world_height = [&]() {
            return std::max(60, screen.fit.h - panel_height(settings.panel));
        };
        int closed_by = 0; // the key that closed the menu: not a flight key until let go
        bool targets_on = opts.targets;
        std::string banner;       // a wave arriving, or an enemy down
        float banner_s = 0.0f;    // how long it still shows
        bool marker = false;      // the bomb marker (a developer switch until it's an upgrade)
        float own_blast_s = 0.0f; // how long "OWN BLAST" still shows
        float own_blast_damage = 0.0f;
        std::uint64_t runs = 0;
        const auto new_run = [&]() {
            const float x =
                opts.has_at ? static_cast<float>(opts.at_x) : static_cast<float>(kMapWidthU) * 0.5f;
            const float y = opts.has_at ? static_cast<float>(opts.at_y) : 480.0f;
            // Your fighter, with its real stats, guns and bombs.
            Body b = new_plane(tuning.rules, player_type, x, y, opts.heading);
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
            world.waves = start_waves(tuning.rules, enemy_tree, opts.wave);
            banner_s = 0.0f;
            if (targets_on) {
                world.targets = practice_targets(tuning.rules.targets);
            }
            previous = world.bodies;
            camera.snap_to(world.bodies[0], tuning.camera, static_cast<float>(world_height()));
            effects.clear();
            own_blast_s = 0.0f;
        };
        if (exit_code == 0 && !refresh_screen(renderer, screen)) {
            exit_code = 1;
        }
        new_run();

        bool switch_pressed = false;
        bool overlay = opts.overlay;
        game::DashboardView dashboard(renderer);

        FixedStep step(kTickNs, kMaxTicksPerFrame);
        Uint64 last = SDL_GetTicksNS();
        Uint64 fps_since = last;
        int fps_frames = 0;
        int fps = 0;
        bool running = exit_code == 0;

        // What a menu choice asks of the game.
        const auto apply_menu = [&](MenuAction action) {
            if (action == MenuAction::Restart) {
                new_run();
            } else if (action == MenuAction::Quit) {
                running = false;
            } else if (action == MenuAction::SettingsChanged && !shot_mode) {
                save_settings(settings, settings_path);
            }
        };

        while (running) {
            const bool run_over = world.bodies[0].outcome != Outcome::Flying;
            const bool paused = menu.page != MenuPage::Closed;
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                const bool in_menu = menu.page != MenuPage::Closed;
                if (e.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                    // Pause when the player looks away.
                    if (!in_menu && !tool_mode) {
                        open_menu(menu);
                    }
                } else if (in_menu && e.type == SDL_EVENT_MOUSE_MOTION) {
                    const SDL_FPoint at = mouse_view(renderer, e, screen.fit);
                    const MenuHit hit = menu_hit(menu.page, screen.fit.w, screen.fit.h, at.x, at.y);
                    menu_hover(menu, hit.row, hit.slot);
                } else if (in_menu && e.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                           e.button.button == SDL_BUTTON_LEFT) {
                    const SDL_FPoint at = mouse_view(renderer, e, screen.fit);
                    const MenuHit hit = menu_hit(menu.page, screen.fit.w, screen.fit.h, at.x, at.y);
                    apply_menu(menu_click(menu, hit.row, hit.slot, settings));
                } else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
                    if (e.key.key == SDLK_F1) {
                        overlay = !overlay;
                    } else if (e.key.key == SDLK_F11) {
                        const bool full = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
                        SDL_SetWindowFullscreen(window, !full);
                    } else if (in_menu) {
                        MenuAction action = MenuAction::None;
                        if (menu.capturing) {
                            action = menu_key(menu, static_cast<int>(e.key.scancode), settings);
                        } else if (const auto in = menu_input_of(e.key.scancode)) {
                            action = menu_input(menu, *in, settings);
                        }
                        if (menu.page == MenuPage::Closed) {
                            closed_by = static_cast<int>(e.key.scancode);
                        }
                        apply_menu(action);
                    } else if (e.key.key == SDLK_ESCAPE) {
                        open_menu(menu);
                    } else if (e.key.key == SDLK_F2) {
                        targets_on = !targets_on;
                        world.targets = targets_on ? practice_targets(tuning.rules.targets)
                                                   : std::vector<Target>{};
                    } else if (e.key.key == SDLK_F3) {
                        settings.your_aim = static_cast<HeightRule>(
                            (static_cast<int>(settings.your_aim) + 1) % kHeightRules);
                        apply_menu(MenuAction::SettingsChanged);
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
                    } else if (game::is_key(settings.keys, Action::SwitchHeavy, e.key.scancode)) {
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
            // no input), so a screenshot is the same every time. When the run is over, it waits,
            // and while a menu is open (a screenshot flies on under one).
            const Uint64 now = SDL_GetTicksNS();
            const bool held_still = menu.page != MenuPage::Closed && !shot_mode;
            const int ticks = shot_mode    ? 1
                              : held_still ? 0
                                           : step.advance(static_cast<std::int64_t>(now - last));
            const float frame_s = std::min(0.25f, static_cast<float>(now - last) / 1e9f);
            last = now;
            // The keys held, less the one that closed the menu until it's let go (so choosing
            // Resume with Space doesn't drop a bomb).
            std::array<bool, SDL_SCANCODE_COUNT> keys{};
            int key_count = 0;
            const bool* state = SDL_GetKeyboardState(&key_count);
            std::copy(state, state + std::min(key_count, static_cast<int>(keys.size())),
                      keys.begin());
            if (closed_by > 0 && closed_by < static_cast<int>(keys.size())) {
                if (keys[static_cast<std::size_t>(closed_by)]) {
                    keys[static_cast<std::size_t>(closed_by)] = false;
                } else {
                    closed_by = 0;
                }
            }
            if (paused) {
                switch_pressed = false;
            }
            tuning.rules.height_rule = settings.your_aim;
            tuning.rules.enemy_height_rule = settings.enemy_aim;
            InputCommand scripted;
            scripted.guns = opts.hold_fire;
            scripted.heavy = opts.hold_bomb;
            for (int t = 0; t < ticks && world.bodies[0].outcome == Outcome::Flying; ++t) {
                previous = world.bodies;
                const std::array<InputCommand, 1> commands = {
                    shot_mode ? scripted
                              : game::read_command(settings, keys.data(), switch_pressed)};
                switch_pressed = false;
                wof::step(world, commands, tuning.rules);
                camera.follow(world.bodies[0], tuning.camera, static_cast<float>(world_height()));
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
                banner_s = std::max(0.0f, banner_s - kTickSeconds);
                for (const Event& ev : world.events) {
                    if (ev.kind == EventKind::WaveArrived) {
                        const int count = static_cast<int>(ev.damage);
                        banner =
                            format("WAVE %d: %d ENEMY FIGHTER%s AT %s", ev.who, count,
                                   count == 1 ? "" : "S",
                                   height_text(ev.z, units_of(settings.units, nation)).c_str());
                        banner_s = 4.0f;
                    } else if (ev.kind == EventKind::EnemyDown) {
                        banner = "ENEMY DOWN";
                        banner_s = 2.0f;
                    }
                }
            }
            if (world.bodies[0].outcome != Outcome::Flying && !shot_mode && !held_still) {
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
            // The world fills the view above the dashboard.
            const ViewFit& fit = screen.fit;
            const int world_h = world_height();
            const Body plane = between(previous[0], world.bodies[0], alpha);
            const ViewXf xf = make_view(camera.focus(alpha), plane.z, tuning.rules.flight.ceiling_u,
                                        tuning.look.zoom_at_ceiling, fit.w, world_h);
            sprites.pump();
            SDL_SetRenderTarget(renderer, screen.target);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            draw_ground(renderer, screen, ground, xf, fit.w, world_h);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            // The enemies (bodies 1 on), partway between ticks like the player.
            std::vector<Body> enemies;
            for (std::size_t i = 1; i < world.bodies.size(); ++i) {
                enemies.push_back(i < previous.size() ? between(previous[i], world.bodies[i], alpha)
                                                      : world.bodies[i]);
            }
            draw_scene(renderer, xf, world, plane, enemies, sprites, effects, tuning, marker);
            if (overlay) {
                draw_pilot_labels(renderer, xf, world, enemies);
            }

            draw_enemy_arrows(renderer, xf, plane, enemies, fit.w, world_h);

            // The dashboard: every gauge and warning (design/dashboard.md).
            PanelState gauges = panel_state(world, previous[0], plane, tuning.rules, nation);
            gauges.assisted = settings.auto_level == AutoLevel::Assisted;
            gauges.own_blast = own_blast_s > 0.0f;
            // Screenshots always read ten past ten, so they come out the same.
            const ClockTime clock = shot_mode ? ClockTime{10, 10} : local_time();
            gauges.clock_h = clock.hour;
            gauges.clock_m = clock.minute;
            int hits = 0;
            for (const Target& t : world.targets) {
                hits += t.hits;
            }
            MessageInput said;
            said.edge_countdown_s =
                plane.outcome == Outcome::Flying ? plane.edge_countdown_s : -1.0f;
            said.own_blast_s = own_blast_s;
            said.own_blast_damage = own_blast_damage;
            said.going_down = plane.damage.going_down && plane.outcome == Outcome::Flying;
            said.edge_warning = plane.edge_warning;
            said.notice = banner;
            said.notice_s = banner_s;
            said.status = format("WAVE %d   KILLS %d   AIM %s   LEVEL %s", world.waves.current,
                                 world.waves.kills, height_rule_name(settings.your_aim),
                                 auto_level_name(settings.auto_level)) +
                          (world.targets.empty() ? std::string{} : format("   HITS %d", hits));
            const Message message = panel_message(said);
            gauges.message = message.text;
            gauges.tone = message.tone;
            dashboard.draw(gauges, settings.panel, fit.w, fit.h);

            if (plane.outcome != Outcome::Flying) {
                const long secs = static_cast<long>(world.tick / kTicksPerSecond);
                draw_text_centred(renderer, fit.w, static_cast<float>(world_h) * 0.5f - 20.0f,
                                  {banner_of(plane.outcome),
                                   format("time flown %ld:%02ld", secs / 60, secs % 60),
                                   format("wave %d, enemies shot down %d", world.waves.current,
                                          world.waves.kills),
                                   "ENTER OR R: NEW RUN   ESC: MENU"},
                                  SDL_Color{255, 236, 170, 255});
            }

            if (overlay) {
                const GroundCell cell_below = farmland_of(plane.x, plane.y);
                const GroundSample under =
                    sample_ground(tuning.ground, seed, cell_below.x, cell_below.y);
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
                            marker ? "  marker on" : ""),
                     format("enemies %d  wave %d (next in %.0f s)  waiting %d",
                            enemies_alive(world), world.waves.current,
                            static_cast<double>(world.waves.timer_s), world.waves.waiting)},
                    SDL_Color{236, 232, 200, 255});
            }
            game::draw_menu(renderer, menu, settings, tuning.rules.weapon, fit.w, fit.h);
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
