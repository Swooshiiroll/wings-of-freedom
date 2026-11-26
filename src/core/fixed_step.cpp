#include "core/fixed_step.hpp"

namespace wof {

FixedStep::FixedStep(std::int64_t tick_ns, int max_ticks) noexcept
    : tick_ns_(tick_ns > 0 ? tick_ns : 1), max_ticks_(max_ticks > 0 ? max_ticks : 1) {}

int FixedStep::advance(std::int64_t frame_ns) noexcept {
    if (frame_ns > 0) {
        accumulated_ns_ += frame_ns;
    }
    std::int64_t ticks = accumulated_ns_ / tick_ns_;
    if (ticks > max_ticks_) {
        // Drop the backlog beyond the cap; keep the fraction of the current tick.
        ticks = max_ticks_;
        accumulated_ns_ %= tick_ns_;
    } else {
        accumulated_ns_ -= ticks * tick_ns_;
    }
    total_ticks_ += static_cast<std::uint64_t>(ticks);
    return static_cast<int>(ticks);
}

float FixedStep::alpha() const noexcept {
    return static_cast<float>(static_cast<double>(accumulated_ns_) / static_cast<double>(tick_ns_));
}

} // namespace wof
