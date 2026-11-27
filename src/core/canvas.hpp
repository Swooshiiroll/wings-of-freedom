#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace wof {

// A small software canvas for pixel art drawn at run time (the dashboard): 0xAARRGGBB pixels,
// with alpha blended over what's there. No SDL; the front end uploads the pixels.
using Colour = std::uint32_t;

[[nodiscard]] constexpr Colour rgb(int r, int g, int b, int a = 255) noexcept {
    return (static_cast<Colour>(a) << 24u) | (static_cast<Colour>(r) << 16u) |
           (static_cast<Colour>(g) << 8u) | static_cast<Colour>(b);
}

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    friend constexpr bool operator==(const Rect&, const Rect&) = default;
};

class Canvas {
  public:
    Canvas(int w, int h);

    [[nodiscard]] int width() const noexcept { return w_; }
    [[nodiscard]] int height() const noexcept { return h_; }
    [[nodiscard]] const std::vector<Colour>& pixels() const noexcept { return pixels_; }
    [[nodiscard]] Colour at(int x, int y) const noexcept; // 0 outside

    void clear(Colour c) noexcept;
    void set(int x, int y, Colour c) noexcept; // blends by c's alpha; clipped
    void fill(Rect r, Colour c) noexcept;
    void frame(Rect r, Colour c) noexcept; // a 1 px outline
    void line(float x0, float y0, float x1, float y1, Colour c) noexcept;
    // A filled circle, and a 1 px ring.
    void disc(float cx, float cy, float radius, Colour c) noexcept;
    void ring(float cx, float cy, float radius, Colour c) noexcept;
    // Text in the 3x5 pixel font (core/pixel_font.hpp), from its top-left or centred on x.
    void text(int x, int y, std::string_view s, Colour c) noexcept;
    void text_centred(float cx, int y, std::string_view s, Colour c) noexcept;

  private:
    int w_;
    int h_;
    std::vector<Colour> pixels_;
};

} // namespace wof
