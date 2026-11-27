#pragma once

#include <vector>

#include "core/fighter.hpp"

namespace wof {

// A plane seen from above as lines (the dashboard's damage wireframe, design/dashboard.md): each
// block of its model gives the outline of its footprint, tagged with its part. In the model's
// frame, u: x toward the right wingtip, y toward the nose.
struct WireLine {
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    Part part = Part::Fuselage;
};

// Blocks with footprints smaller than `min_area` u² (struts and the like) are left out.
[[nodiscard]] std::vector<WireLine> top_wireframe(const std::vector<Block>& model,
                                                  float min_area = 0.5f);

} // namespace wof
