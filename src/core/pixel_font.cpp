#include "core/pixel_font.hpp"

namespace wof {
namespace {

// Five rows, top to bottom, each three bits (4 = left, 1 = right).
[[nodiscard]] constexpr Glyph rows(unsigned a, unsigned b, unsigned c, unsigned d,
                                   unsigned e) noexcept {
    return static_cast<Glyph>((a << 12u) | (b << 9u) | (c << 6u) | (d << 3u) | e);
}

} // namespace

Glyph glyph_of(char c) noexcept {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }
    switch (c) {
    case '0':
        return rows(7, 5, 5, 5, 7);
    case '1':
        return rows(2, 6, 2, 2, 7);
    case '2':
        return rows(7, 1, 7, 4, 7);
    case '3':
        return rows(7, 1, 3, 1, 7);
    case '4':
        return rows(5, 5, 7, 1, 1);
    case '5':
        return rows(7, 4, 7, 1, 7);
    case '6':
        return rows(7, 4, 7, 5, 7);
    case '7':
        return rows(7, 1, 2, 2, 2);
    case '8':
        return rows(7, 5, 7, 5, 7);
    case '9':
        return rows(7, 5, 7, 1, 7);
    case 'A':
        return rows(2, 5, 7, 5, 5);
    case 'B':
        return rows(6, 5, 6, 5, 6);
    case 'C':
        return rows(3, 4, 4, 4, 3);
    case 'D':
        return rows(6, 5, 5, 5, 6);
    case 'E':
        return rows(7, 4, 6, 4, 7);
    case 'F':
        return rows(7, 4, 6, 4, 4);
    case 'G':
        return rows(3, 4, 5, 5, 3);
    case 'H':
        return rows(5, 5, 7, 5, 5);
    case 'I':
        return rows(7, 2, 2, 2, 7);
    case 'J':
        return rows(1, 1, 1, 5, 2);
    case 'K':
        return rows(5, 5, 6, 5, 5);
    case 'L':
        return rows(4, 4, 4, 4, 7);
    case 'M':
        return rows(5, 7, 7, 5, 5);
    case 'N':
        return rows(6, 5, 5, 5, 5);
    case 'O':
        return rows(2, 5, 5, 5, 2);
    case 'P':
        return rows(6, 5, 6, 4, 4);
    case 'Q':
        return rows(2, 5, 5, 7, 3);
    case 'R':
        return rows(6, 5, 6, 5, 5);
    case 'S':
        return rows(3, 4, 2, 1, 6);
    case 'T':
        return rows(7, 2, 2, 2, 2);
    case 'U':
        return rows(5, 5, 5, 5, 7);
    case 'V':
        return rows(5, 5, 5, 5, 2);
    case 'W':
        return rows(5, 5, 7, 7, 5);
    case 'X':
        return rows(5, 5, 2, 5, 5);
    case 'Y':
        return rows(5, 5, 2, 2, 2);
    case 'Z':
        return rows(7, 1, 2, 4, 7);
    case '.':
        return rows(0, 0, 0, 0, 2);
    case ',':
        return rows(0, 0, 0, 2, 4);
    case ':':
        return rows(0, 2, 0, 2, 0);
    case '-':
        return rows(0, 0, 7, 0, 0);
    case '+':
        return rows(0, 2, 7, 2, 0);
    case '/':
        return rows(1, 1, 2, 4, 4);
    case '%':
        return rows(5, 1, 2, 4, 5);
    case '(':
        return rows(1, 2, 2, 2, 1);
    case ')':
        return rows(4, 2, 2, 2, 4);
    case '\'':
        return rows(2, 2, 0, 0, 0);
    case '!':
        return rows(2, 2, 2, 0, 2);
    default:
        return 0;
    }
}

} // namespace wof
