#include "core/view.hpp"

#include <algorithm>

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

} // namespace wof
