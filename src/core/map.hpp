#pragma once

namespace wof {

// The battle area (design/flight.md, World and view), in world units (u). x runs east and y
// north; (0, 0) is the south-west corner. South (y < 0) is the way home. The ground itself
// carries on past every edge with no marking.
inline constexpr int kMapWidthU = 2880; // east-west, about 6 screens across
inline constexpr int kMapDepthU = 4800; // north-south, about 10 screens deep

} // namespace wof
