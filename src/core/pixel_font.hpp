#pragma once

#include <cstdint>
#include <string_view>

namespace wof {

// A tiny 3x5 pixel font for dial faces and the message strip: digits, capitals (lower case draws
// as capitals) and the punctuation the dashboard needs. Unknown characters draw blank.

// 15 bits: five rows of three, the top row in the highest bits, the left column highest in each.
using Glyph = std::uint16_t;

inline constexpr int kGlyphW = 3;
inline constexpr int kGlyphH = 5;
inline constexpr int kGlyphAdvance = 4;

[[nodiscard]] Glyph glyph_of(char c) noexcept;
[[nodiscard]] constexpr bool glyph_pixel(Glyph g, int col, int row) noexcept {
    return ((g >> static_cast<unsigned>((kGlyphH - 1 - row) * kGlyphW + (kGlyphW - 1 - col))) &
            1u) != 0u;
}
// The pixels a string covers across (no trailing space).
[[nodiscard]] constexpr int text_width(std::string_view s) noexcept {
    return s.empty() ? 0 : static_cast<int>(s.size()) * kGlyphAdvance - 1;
}

} // namespace wof
