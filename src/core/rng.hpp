#pragma once

#include <cstdint>

namespace wof {

// PCG32 (M. E. O'Neill, pcg-random.org): small, fast and fully deterministic, so a run replays
// exactly from its seed. Same algorithm and seeding as the reference pcg32_srandom_r.
class Rng {
  public:
    constexpr explicit Rng(std::uint64_t seed, std::uint64_t stream = 0x5eedu) noexcept
        : inc_((stream << 1u) | 1u) {
        next_u32();
        state_ += seed;
        next_u32();
    }

    constexpr std::uint32_t next_u32() noexcept {
        const std::uint64_t old = state_;
        state_ = old * 6364136223846793005ull + inc_;
        const auto xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
        const auto rot = static_cast<std::uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    }

    // Uniform in [0, bound) without modulo bias; bound must be > 0.
    constexpr std::uint32_t next_below(std::uint32_t bound) noexcept {
        const std::uint32_t threshold = (~bound + 1u) % bound;
        for (;;) {
            const std::uint32_t r = next_u32();
            if (r >= threshold) {
                return r % bound;
            }
        }
    }

    // Uniform in [0, 1): the top 24 bits, so every value is exactly representable as a float.
    constexpr float next_float() noexcept {
        return static_cast<float>(next_u32() >> 8u) * (1.0f / 16777216.0f);
    }

  private:
    std::uint64_t state_ = 0;
    std::uint64_t inc_;
};

} // namespace wof
