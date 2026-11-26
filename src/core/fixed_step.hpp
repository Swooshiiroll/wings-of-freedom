#pragma once

#include <cstdint>

namespace wof {

// Turns variable frame times into a whole number of fixed simulation ticks, so the simulation
// runs the same at any frame rate. Time is kept in integer nanoseconds: no floating-point drift.
class FixedStep {
  public:
    // tick_ns: length of one simulation tick. max_ticks: cap per frame, so a long stall (window
    // dragged, breakpoint) skips time instead of freezing while it catches up.
    FixedStep(std::int64_t tick_ns, int max_ticks) noexcept;

    // Adds a frame's elapsed time and returns how many ticks to simulate now.
    int advance(std::int64_t frame_ns) noexcept;

    // How far the leftover time is into the next tick, in [0, 1): for interpolating rendering
    // between the previous and current simulation states.
    [[nodiscard]] float alpha() const noexcept;

    [[nodiscard]] std::int64_t tick_ns() const noexcept { return tick_ns_; }
    [[nodiscard]] std::uint64_t total_ticks() const noexcept { return total_ticks_; }

  private:
    std::int64_t tick_ns_;
    int max_ticks_;
    std::int64_t accumulated_ns_ = 0;
    std::uint64_t total_ticks_ = 0;
};

} // namespace wof
