#pragma once

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

} // namespace wof
