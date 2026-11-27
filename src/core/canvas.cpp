#include "core/canvas.hpp"

#include <algorithm>
#include <cmath>

#include "core/pixel_font.hpp"

namespace wof {

Canvas::Canvas(int w, int h)
    : w_(std::max(0, w)), h_(std::max(0, h)),
      pixels_(static_cast<std::size_t>(w_) * static_cast<std::size_t>(h_), 0u) {}

Colour Canvas::at(int x, int y) const noexcept {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) {
        return 0u;
    }
    return pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) +
                   static_cast<std::size_t>(x)];
}

void Canvas::clear(Colour c) noexcept {
    std::fill(pixels_.begin(), pixels_.end(), c);
}

void Canvas::set(int x, int y, Colour c) noexcept {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) {
        return;
    }
    Colour& under = pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) +
                            static_cast<std::size_t>(x)];
    const Colour a = c >> 24u;
    if (a == 255u) {
        under = c;
        return;
    }
    const auto mix = [a](Colour top, Colour bottom) {
        return (top * a + bottom * (255u - a)) / 255u;
    };
    under = 0xff000000u | (mix((c >> 16u) & 0xffu, (under >> 16u) & 0xffu) << 16u) |
            (mix((c >> 8u) & 0xffu, (under >> 8u) & 0xffu) << 8u) | mix(c & 0xffu, under & 0xffu);
}

void Canvas::fill(Rect r, Colour c) noexcept {
    for (int y = r.y; y < r.y + r.h; ++y) {
        for (int x = r.x; x < r.x + r.w; ++x) {
            set(x, y, c);
        }
    }
}

void Canvas::frame(Rect r, Colour c) noexcept {
    fill({r.x, r.y, r.w, 1}, c);
    fill({r.x, r.y + r.h - 1, r.w, 1}, c);
    fill({r.x, r.y + 1, 1, r.h - 2}, c);
    fill({r.x + r.w - 1, r.y + 1, 1, r.h - 2}, c);
}

void Canvas::line(float x0, float y0, float x1, float y1, Colour c) noexcept {
    const float steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    const int n = std::max(1, static_cast<int>(std::ceil(steps)));
    int last_x = -100000;
    int last_y = -100000;
    for (int i = 0; i <= n; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(n);
        const auto x = static_cast<int>(std::floor(x0 + (x1 - x0) * t));
        const auto y = static_cast<int>(std::floor(y0 + (y1 - y0) * t));
        if (x != last_x || y != last_y) {
            set(x, y, c);
            last_x = x;
            last_y = y;
        }
    }
}

void Canvas::disc(float cx, float cy, float radius, Colour c) noexcept {
    const auto y0 = static_cast<int>(std::floor(cy - radius));
    const auto y1 = static_cast<int>(std::ceil(cy + radius));
    const auto x0 = static_cast<int>(std::floor(cx - radius));
    const auto x1 = static_cast<int>(std::ceil(cx + radius));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - cx;
            const float dy = static_cast<float>(y) + 0.5f - cy;
            if (dx * dx + dy * dy <= radius * radius) {
                set(x, y, c);
            }
        }
    }
}

void Canvas::ring(float cx, float cy, float radius, Colour c) noexcept {
    const auto y0 = static_cast<int>(std::floor(cy - radius));
    const auto y1 = static_cast<int>(std::ceil(cy + radius));
    const auto x0 = static_cast<int>(std::floor(cx - radius));
    const auto x1 = static_cast<int>(std::ceil(cx + radius));
    const float inner = radius - 1.0f;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - cx;
            const float dy = static_cast<float>(y) + 0.5f - cy;
            const float d2 = dx * dx + dy * dy;
            if (d2 <= radius * radius && d2 > inner * inner) {
                set(x, y, c);
            }
        }
    }
}

void Canvas::text(int x, int y, std::string_view s, Colour c) noexcept {
    for (const char ch : s) {
        const Glyph g = glyph_of(ch);
        for (int row = 0; row < kGlyphH; ++row) {
            for (int col = 0; col < kGlyphW; ++col) {
                if (glyph_pixel(g, col, row)) {
                    set(x + col, y + row, c);
                }
            }
        }
        x += kGlyphAdvance;
    }
}

void Canvas::text_centred(float cx, int y, std::string_view s, Colour c) noexcept {
    const auto w = static_cast<float>(text_width(s));
    text(static_cast<int>(std::floor(cx - w * 0.5f + 0.5f)), y, s, c);
}

} // namespace wof
