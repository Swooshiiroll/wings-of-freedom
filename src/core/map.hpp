#pragma once

namespace wof {

// The battle area (design/flight.md, World and view), in world units (u). x runs east and y
// north; (0, 0) is the south-west corner. South (y < 0) is the way home. The ground itself
// carries on past every edge with no marking.
// About 2 minutes at cruising speed (~120 u/s) from the centre to any side (owner, 2026-11-27):
// about 12.5 miles square.
inline constexpr int kMapWidthU = 28800; // east-west
inline constexpr int kMapDepthU = 28800; // north-south
// The enemy's front line, halfway up (owner, 2026-11-27): about 2 minutes' flying from your base.
inline constexpr int kFrontU = kMapDepthU / 2;

} // namespace wof
