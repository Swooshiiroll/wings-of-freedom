// Wings of Freedom: SDL3 front end (design/flight.md). The world is drawn 1:1 in art pixels into a
// render target the size of the view, then shown at the largest whole-number scale that fits the
// window, so the pixels stay square and crisp. Screens that 270 doesn't divide evenly show more of
// the world.
//
//   WingsOfFreedom [--seed N] [--window WxH] [--shot FILE.png] [--ticks N] [--heading DEG]
//                  [--height U] [--at X,Y] [--overlay] [--version]
//     --seed N        developer tool: the ground from seed N (decimal, or 0x hex)
//     --window WxH    developer tool: open the window at W x H
//     --shot FILE     developer tool: fly N ticks with no input (--ticks, default 120), save the
//                     view to FILE (PNG) and exit
//     --heading DEG   developer tool: the plane's starting heading (0 north, 90 east)
//     --height U      developer tool: the plane's starting height in u (default 120)
//     --at X,Y        developer tool: the plane's starting ground position in u (default: the
//                     middle of the battle area's south end, 1440,480)
//     --overlay      developer tool: start with the F1 overlay shown
//     --version       print the version and exit
//
// Keys (design/flight.md, Controls): A/D turn, W dive, S climb. Shift/Ctrl, F, Space and Tab are
// read into the plane's input command but do nothing until their pull requests. F1 shows the
// developer overlay, F11 toggles fullscreen, and Esc quits (the pause menu comes in PR 6). The
// tuning file (tuning.txt next to the executable) is reloaded when it changes.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <array>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "core/camera.hpp"
#include "core/fixed_step.hpp"
#include "core/ground.hpp"
#include "core/hash.hpp"
#include "core/iso.hpp"
#include "core/map.hpp"
#include "core/tuning.hpp"
#include "core/units.hpp"
#include "core/view.hpp"
#include "core/world.hpp"
#include "game/bindings.hpp"
#include "game/ground_view.hpp"

namespace {

using namespace wof;

constexpr std::int64_t kTickNs = 1'000'000'000 / kTicksPerSecond;
constexpr int kMaxTicksPerFrame = 8;
constexpr Uint64 kTuningCheckNs = 500'000'000; // the tuning file is checked this often

struct Options {
    bool has_seed = false;
    std::uint64_t seed = 0;
    int window_w = 0; // 0: the largest 480x270 multiple that fits the screen
    int window_h = 0;
    const char* shot = nullptr;
    long ticks = 120;
    float heading = 0.0f;
    float height = 120.0f;
    bool has_at = false;
    int at_x = 0;
    int at_y = 0;
    bool overlay = false;
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
            if (!parse_number(argv[++i], out.height) || out.height < 0.0f) {
                std::fprintf(stderr, "--height takes a height in u, 0 or more\n");
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
        } else {
            std::fprintf(stderr,
                         "usage: WingsOfFreedom [--seed N] [--window WxH] [--shot FILE.png] "
                         "[--ticks N] [--heading DEG] [--height U] [--at X,Y] [--overlay] "
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

// A body partway between two ticks, for smooth drawing (the heading the short way round).
Body between(const Body& a, const Body& b, float t) {
    Body out = b;
    out.x = a.x + (b.x - a.x) * t;
    out.y = a.y + (b.y - a.y) * t;
    out.z = a.z + (b.z - a.z) * t;
    float turn = b.heading_deg - a.heading_deg;
    if (turn > 180.0f) {
        turn -= 360.0f;
    } else if (turn < -180.0f) {
        turn += 360.0f;
    }
    out.heading_deg = wrap_heading(a.heading_deg + turn * t);
    return out;
}

// The stand-in plane's outline, in u: x to the right wing, y forward. Fuselage, wings, tailplane.
struct Quad {
    std::array<GroundPt, 4> corners;
};
constexpr std::array<Quad, 3> kStandIn = {{
    {{{{-1.2f, 9.0f}, {1.2f, 9.0f}, {1.2f, -9.0f}, {-1.2f, -9.0f}}}},
    {{{{-10.0f, 3.0f}, {10.0f, 3.0f}, {10.0f, -1.0f}, {-10.0f, -1.0f}}}},
    {{{{-4.0f, -6.0f}, {4.0f, -6.0f}, {4.0f, -9.0f}, {-4.0f, -9.0f}}}},
}};

// Draws the stand-in at its height, or (shadow) its shadow flat on the ground below it. The
// anchor is snapped to whole art pixels, so the shape doesn't shimmer as it moves.
void draw_stand_in(SDL_Renderer* r, const Body& b, PixelPt origin, bool shadow) {
    const float h = b.heading_deg * (3.14159265f / 180.0f);
    const GroundPt right{std::cos(h), -std::sin(h)};
    const GroundPt fwd{std::sin(h), std::cos(h)};
    const ScreenPt at = to_screen(b.x, b.y, shadow ? 0.0f : b.z);
    const float ax = std::round(at.x) - static_cast<float>(origin.x);
    const float ay = std::round(at.y) - static_cast<float>(origin.y);
    const SDL_FColor body{0.78f, 0.80f, 0.70f, 1.0f};
    const SDL_FColor wing{0.66f, 0.70f, 0.60f, 1.0f};
    const SDL_FColor dark{0.0f, 0.0f, 0.0f, 0.38f};
    std::array<SDL_Vertex, 18> v{};
    std::size_t n = 0;
    for (std::size_t q = 0; q < kStandIn.size(); ++q) {
        std::array<SDL_FPoint, 4> p{};
        for (std::size_t c = 0; c < 4; ++c) {
            const GroundPt l = kStandIn[q].corners[c];
            const ScreenPt s =
                to_screen(right.x * l.x + fwd.x * l.y, right.y * l.x + fwd.y * l.y, 0.0f);
            p[c] = {ax + s.x, ay + s.y};
        }
        const SDL_FColor col = shadow ? dark : (q == 1 ? wing : body);
        for (const std::size_t c : {0u, 1u, 2u, 0u, 2u, 3u}) {
            v[n++] = SDL_Vertex{p[c], col, {0.0f, 0.0f}};
        }
    }
    SDL_RenderGeometry(r, nullptr, v.data(), static_cast<int>(n), nullptr, 0);
}

// The developer overlay (F1), pinned to the view's top-left in the 8x8 debug font.
void draw_overlay(SDL_Renderer* r, const std::vector<std::string>& lines) {
    std::size_t widest = 0;
    for (const std::string& l : lines) {
        widest = l.size() > widest ? l.size() : widest;
    }
    const SDL_FRect back{0.0f, 0.0f, static_cast<float>(widest * 8 + 4),
                         static_cast<float>(lines.size() * 10 + 2)};
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_RenderFillRect(r, &back);
    SDL_SetRenderDrawColor(r, 236, 232, 200, 255);
    float y = 2.0f;
    for (const std::string& l : lines) {
        SDL_RenderDebugText(r, 2.0f, y, l.c_str());
        y += 10.0f;
    }
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

} // namespace

int main(int argc, char** argv) {
    Options opts;
    if (const int parsed = parse_options(argc, argv, opts); parsed != 0) {
        return parsed == 1 ? 0 : 2;
    }
    const bool shot_mode = opts.shot != nullptr;

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
    if (shot_mode) {
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
    if (!shot_mode) {
        SDL_SetRenderVSync(renderer, 1);
    }
    const char* backend = SDL_GetRendererName(renderer);
    SDL_Log("Wings of Freedom %s, SDL %d.%d.%d, renderer: %s", WOF_VERSION,
            SDL_VERSIONNUM_MAJOR(SDL_GetVersion()), SDL_VERSIONNUM_MINOR(SDL_GetVersion()),
            SDL_VERSIONNUM_MICRO(SDL_GetVersion()), backend);

    int exit_code = 0;
    {
        Tuning tuning;
        TuningFile tuning_file;
        const char* base_path = SDL_GetBasePath();
        tuning_file.path = std::string(base_path != nullptr ? base_path : "") + "tuning.txt";
        open_tuning(tuning, tuning_file);

        const std::uint64_t seed = opts.has_seed ? opts.seed : random_seed();
        SDL_Log("ground seed 0x%016llx", static_cast<unsigned long long>(seed));
        game::GroundView ground(renderer);
        ground.reset(tuning.ground, seed);

        // One player for now, flying north from near the south edge; the world takes any number.
        World world;
        Body start;
        start.x =
            opts.has_at ? static_cast<float>(opts.at_x) : static_cast<float>(kMapWidthU) * 0.5f;
        start.y = opts.has_at ? static_cast<float>(opts.at_y) : 480.0f;
        start.z = std::fmin(opts.height, tuning.stand_in.ceiling_u);
        start.heading_deg = wrap_heading(opts.heading);
        start.speed_u_s = tuning.stand_in.speed_u_s;
        world.bodies.push_back(start);
        std::vector<Body> previous = world.bodies;
        Camera camera;
        camera.snap_to(world.bodies[0], tuning.camera);

        const game::Bindings bindings = game::default_bindings();
        bool switch_pressed = false;
        bool overlay = opts.overlay;
        Screen screen;
        if (!refresh_screen(renderer, screen)) {
            exit_code = 1;
        }

        FixedStep step(kTickNs, kMaxTicksPerFrame);
        Uint64 last = SDL_GetTicksNS();
        Uint64 fps_since = last;
        int fps_frames = 0;
        int fps = 0;
        bool running = exit_code == 0;

        while (running) {
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
            // no input), so a screenshot is the same every time.
            const Uint64 now = SDL_GetTicksNS();
            const int ticks = shot_mode ? 1 : step.advance(static_cast<std::int64_t>(now - last));
            last = now;
            const bool* keys = SDL_GetKeyboardState(nullptr);
            for (int t = 0; t < ticks; ++t) {
                previous = world.bodies;
                const std::array<InputCommand, 1> commands = {
                    shot_mode ? InputCommand{}
                              : game::read_command(bindings, keys, switch_pressed)};
                switch_pressed = false;
                wof::step(world, commands, tuning.stand_in);
                camera.follow(world.bodies[0], tuning.camera);
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
            draw_stand_in(renderer, plane, origin, true);
            draw_stand_in(renderer, plane, origin, false);
            if (overlay) {
                const GroundSample under =
                    sample_ground(tuning.ground, seed, static_cast<int>(std::floor(plane.x)),
                                  static_cast<int>(std::floor(plane.y)));
                draw_overlay(
                    renderer,
                    {format("Wings of Freedom %s  %d fps  %s", WOF_VERSION, fps, backend),
                     format("window %dx%d  scale %dx  view %dx%d", screen.out_w, screen.out_h,
                            fit.scale, fit.w, fit.h),
                     format("camera %d,%d  lead %.0f,%.0f u", origin.x, origin.y,
                            static_cast<double>(camera.lead().x),
                            static_cast<double>(camera.lead().y)),
                     format("plane %.0f,%.0f  %.0f u (%.0f ft)  %.0f mph",
                            static_cast<double>(plane.x), static_cast<double>(plane.y),
                            static_cast<double>(plane.z), static_cast<double>(plane.z * kFeetPerU),
                            static_cast<double>(plane.speed_u_s * kMphPerUs)),
                     format("heading %.0f (sprite %d of 32)",
                            static_cast<double>(plane.heading_deg),
                            heading_index(plane.heading_deg)),
                     format("ground %s  tiles %d  tick %lld", ground_name(under.kind),
                            ground.cached(), static_cast<long long>(world.tick))});
            }
            if (shot_mode && world.tick >= opts.ticks) {
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
