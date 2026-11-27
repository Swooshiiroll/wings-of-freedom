#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wof {

// The pixel-art view (design/flight.md, World and view): the art is 480x270, shown at the largest
// whole-number scale that fits the window. Screens that 270 doesn't divide evenly (1440p, Steam
// Deck) show more of the world instead of borders or blurry pixels.

inline constexpr int kBaseViewW = 480;
inline constexpr int kBaseViewH = 270;

// The scale and the art pixels the view covers.
struct ViewFit {
    int scale = 1;
    int w = kBaseViewW;
    int h = kBaseViewH;

    friend constexpr bool operator==(ViewFit, ViewFit) = default;
};

// For a window of out_w x out_h real pixels: the largest whole scale with 480 x scale <= out_w and
// 270 x scale <= out_h (at least 1), and the view is the window divided by it, rounded up (the
// last partial pixel is cropped). 1080p gives 4x at 480x270, 1440p 5x at 512x288, and the
// Steam Deck 2x at 640x400.
[[nodiscard]] ViewFit fit_view(int out_w, int out_h) noexcept;

struct WindowSize {
    int w = 0;
    int h = 0;

    friend constexpr bool operator==(WindowSize, WindowSize) = default;
};

// The window the game opens with: the largest whole multiple of 480x270 that fits a display's
// usable area, keeping `decor_h` spare for the title bar (at least 1x). On a 1080p screen with a
// taskbar that's 3x, 1440x810.
[[nodiscard]] WindowSize start_window(int usable_w, int usable_h, int decor_h = 48) noexcept;

// Where to draw the arrow toward something off screen (design/flight.md, Enemies): on the view's
// edge, `margin` inside it, on the line from the centre to the thing's view position, pointing
// along that line. `on_screen` when it's inside the margins and needs no arrow.
struct EdgeArrow {
    float x = 0.0f;
    float y = 0.0f;
    float angle_rad = 0.0f; // 0 points right, a quarter turn down
    bool on_screen = true;
};
[[nodiscard]] EdgeArrow edge_arrow(float px, float py, int view_w, int view_h,
                                   float margin) noexcept;

// One arrow stands for the enemies in about the same direction (design/flight.md, Enemies): the
// nearest first, each joining a group within `spread_rad` of its angle, and the nearest of a group
// giving its height mark.
struct Sighting {
    float angle_rad = 0.0f; // on screen, from the view's centre
    float distance = 0.0f;  // across the ground, u
    float dz = 0.0f;        // its height less yours, u
    bool bomber = false;
};
enum class HeightMark : std::uint8_t { Lower, Level, Higher };
struct ArrowGroup {
    float angle_rad = 0.0f;
    HeightMark mark = HeightMark::Level;
    std::size_t nearest = 0; // the sighting the arrow points at
    bool bomber = false;     // any of them a bomber
};
[[nodiscard]] std::vector<ArrowGroup> group_arrows(const std::vector<Sighting>& seen,
                                                   float spread_rad, float level_u);

} // namespace wof
