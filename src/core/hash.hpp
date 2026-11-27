#pragma once

#include <cstdint>

namespace wof {

// SplitMix64 finaliser: turns any 64-bit value into a well-mixed one. Used for hash-based
// layouts (the ground) and to derive sub-seeds from a run's one seed, so they never correlate.
constexpr std::uint64_t splitmix64(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30u)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27u)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31u);
}

// A hash of two whole numbers (negative ones too) and a salt under a seed: the same on every
// compiler, since it's integer maths only.
constexpr std::uint64_t hash2(std::uint64_t seed, int a, int b, std::uint64_t salt) noexcept {
    const std::uint64_t packed =
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a)) << 32u) |
        static_cast<std::uint32_t>(b);
    return splitmix64(seed ^ splitmix64(packed ^ splitmix64(salt)));
}

// 64-bit FNV-1a over values fed in a fixed byte order, so the same data hashes the same on every
// platform. The golden tests use it to pin generated ground across compilers.
class Fnv1a {
  public:
    constexpr void add_byte(std::uint8_t b) noexcept {
        hash_ ^= b;
        hash_ *= 0x100000001b3ull;
    }
    constexpr void add_u32(std::uint32_t v) noexcept {
        for (int i = 0; i < 4; ++i) {
            add_byte(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    constexpr void add_int(int v) noexcept { add_u32(static_cast<std::uint32_t>(v)); }

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return hash_; }

  private:
    std::uint64_t hash_ = 0xcbf29ce484222325ull;
};

} // namespace wof
