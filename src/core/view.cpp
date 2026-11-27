#include "core/view.hpp"

#include <algorithm>
#include <cmath>

namespace wof {

ViewFit fit_view(int out_w, int out_h) noexcept {
    const int w = std::max(1, out_w);
    const int h = std::max(1, out_h);
    const int scale = std::max(1, std::min(w / kBaseViewW, h / kBaseViewH));
    return {scale, (w + scale - 1) / scale, (h + scale - 1) / scale};
}

WindowSize start_window(int usable_w, int usable_h, int decor_h) noexcept {
    const int k = std::max(1, std::min(usable_w / kBaseViewW, (usable_h - decor_h) / kBaseViewH));
    return {kBaseViewW * k, kBaseViewH * k};
}

EdgeArrow edge_arrow(float px, float py, int view_w, int view_h, float margin) noexcept {
    const float cx = static_cast<float>(view_w) * 0.5f;
    const float cy = static_cast<float>(view_h) * 0.5f;
    const float dx = px - cx;
    const float dy = py - cy;
    const float hw = std::max(1.0f, cx - margin);
    const float hh = std::max(1.0f, cy - margin);
    EdgeArrow a{px, py, std::atan2(dy, dx), true};
    if (std::abs(dx) <= hw && std::abs(dy) <= hh) {
        return a;
    }
    // Pull the point in along its line from the centre until it meets the inner rectangle.
    const float k = std::min(std::abs(dx) > 0.0f ? hw / std::abs(dx) : 1e9f,
                             std::abs(dy) > 0.0f ? hh / std::abs(dy) : 1e9f);
    a.x = cx + dx * k;
    a.y = cy + dy * k;
    a.on_screen = false;
    return a;
}

} // namespace wof
