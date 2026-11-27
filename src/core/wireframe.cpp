#include "core/wireframe.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace wof {
namespace {

struct Pt {
    float x = 0.0f;
    float y = 0.0f;
};

[[nodiscard]] float cross(Pt o, Pt a, Pt b) noexcept {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// The convex hull (Andrew's monotone chain), counter-clockwise.
[[nodiscard]] std::vector<Pt> hull(std::vector<Pt> p) {
    std::sort(p.begin(), p.end(),
              [](Pt a, Pt b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    if (p.size() < 3) {
        return p;
    }
    std::vector<Pt> h(p.size() * 2);
    std::size_t k = 0;
    for (const Pt& q : p) {
        while (k >= 2 && cross(h[k - 2], h[k - 1], q) <= 0.0f) {
            --k;
        }
        h[k++] = q;
    }
    const std::size_t lower = k + 1;
    for (std::size_t i = p.size() - 1; i-- > 0;) {
        while (k >= lower && cross(h[k - 2], h[k - 1], p[i]) <= 0.0f) {
            --k;
        }
        h[k++] = p[i];
    }
    h.resize(k - 1);
    return h;
}

[[nodiscard]] float area(const std::vector<Pt>& poly) noexcept {
    float twice = 0.0f;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const Pt& a = poly[i];
        const Pt& b = poly[(i + 1) % poly.size()];
        twice += a.x * b.y - b.x * a.y;
    }
    return std::abs(twice) * 0.5f;
}

} // namespace

std::vector<WireLine> top_wireframe(const std::vector<Block>& model, float min_area) {
    std::vector<WireLine> lines;
    for (const Block& b : model) {
        std::vector<Pt> corners;
        for (const Vec3& c : b.corners) {
            corners.push_back({c.x, c.y});
        }
        const std::vector<Pt> outline = hull(std::move(corners));
        if (outline.size() < 3 || area(outline) < min_area) {
            continue;
        }
        for (std::size_t i = 0; i < outline.size(); ++i) {
            const Pt& a = outline[i];
            const Pt& z = outline[(i + 1) % outline.size()];
            lines.push_back({a.x, a.y, z.x, z.y, b.part});
        }
    }
    return lines;
}

} // namespace wof
