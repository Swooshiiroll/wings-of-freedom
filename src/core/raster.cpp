#include "core/raster.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>

namespace wof {
namespace {

constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

// The six faces of a block, as corner indices going round each one.
constexpr std::array<std::array<int, 4>, 6> kFaces = {{
    {{0, 1, 2, 3}},
    {{4, 5, 6, 7}},
    {{0, 1, 5, 4}},
    {{1, 2, 6, 5}},
    {{2, 3, 7, 6}},
    {{3, 0, 4, 7}},
}};

// Toward the sun: above, and to the upper left as seen on screen.
constexpr Vec3 kSun{-0.35f, -0.46f, 0.82f};
constexpr float kAmbient = 0.52f;

// A projected corner: pixel position, and depth (bigger is nearer the viewer, who looks from the
// south-east and above).
struct Projected {
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
};

[[nodiscard]] Vec3 sub(Vec3 a, Vec3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
[[nodiscard]] Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
[[nodiscard]] float dot(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] Projected project(Vec3 w, float ox, float oy, bool flat) noexcept {
    const float z = flat ? 0.0f : w.z;
    return {ox + w.x + w.y, oy + (w.x - w.y) * 0.5f - kModelHeightPxPerU * z,
            w.x - w.y + 0.8164966f * z};
}

// Fills a triangle into the sprite where it's nearer than what's there.
void fill(const Projected& a, const Projected& b, const Projected& c, std::uint32_t colour,
          std::int16_t id, Sprite& s, std::vector<float>& depth) {
    const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (std::abs(area) < 1e-6f) {
        return; // edge-on
    }
    const int x0 = std::max(0, static_cast<int>(std::floor(std::min({a.x, b.x, c.x}))));
    const int x1 = std::min(s.w - 1, static_cast<int>(std::ceil(std::max({a.x, b.x, c.x}))));
    const int y0 = std::max(0, static_cast<int>(std::floor(std::min({a.y, b.y, c.y}))));
    const int y1 = std::min(s.h - 1, static_cast<int>(std::ceil(std::max({a.y, b.y, c.y}))));
    for (int py = y0; py <= y1; ++py) {
        for (int px = x0; px <= x1; ++px) {
            const float x = static_cast<float>(px) + 0.5f;
            const float y = static_cast<float>(py) + 0.5f;
            // Barycentric weights from the edge functions; inside when all share area's sign.
            const float wa = ((b.x - x) * (c.y - y) - (b.y - y) * (c.x - x)) / area;
            const float wb = ((c.x - x) * (a.y - y) - (c.y - y) * (a.x - x)) / area;
            const float wc = 1.0f - wa - wb;
            if (wa < 0.0f || wb < 0.0f || wc < 0.0f) {
                continue;
            }
            const float d = wa * a.depth + wb * b.depth + wc * c.depth;
            const std::size_t i = static_cast<std::size_t>(py) * static_cast<std::size_t>(s.w) +
                                  static_cast<std::size_t>(px);
            if (d > depth[i]) {
                depth[i] = d;
                s.pixels[i] = colour;
                s.block[i] = id;
            }
        }
    }
}

[[nodiscard]] std::uint32_t shaded(Rgb8 c, float light) noexcept {
    const auto ch = [light](std::uint8_t v) {
        return static_cast<std::uint32_t>(
            std::clamp(static_cast<int>(std::lround(static_cast<float>(v) * light)), 0, 255));
    };
    return 0xff000000u | (ch(c.r) << 16u) | (ch(c.g) << 8u) | ch(c.b);
}

// Draws every face of the selected blocks. `flat`: the shadow (everything at height 0, one colour).
Sprite render(const std::vector<Block>& model, const Pose& pose, int w, int h, float ox, float oy,
              std::uint32_t parts, bool flat) {
    Sprite s;
    s.w = w;
    s.h = h;
    const auto n = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    s.pixels.assign(n, 0u);
    s.block.assign(n, static_cast<std::int16_t>(-1));
    std::vector<float> depth(n, -std::numeric_limits<float>::infinity());
    const float sun_len = std::sqrt(dot(kSun, kSun));
    for (std::size_t bi = 0; bi < model.size(); ++bi) {
        const Block& blk = model[bi];
        if ((part_bit(blk.part) & parts) == 0) {
            continue;
        }
        std::array<Vec3, 8> world{};
        Vec3 centre{};
        for (std::size_t k = 0; k < 8; ++k) {
            world[k] = pose_point(blk.corners[k], pose);
            centre = {centre.x + world[k].x / 8.0f, centre.y + world[k].y / 8.0f,
                      centre.z + world[k].z / 8.0f};
        }
        std::array<Projected, 8> pr{};
        for (std::size_t k = 0; k < 8; ++k) {
            pr[k] = project(world[k], ox, oy, flat);
        }
        for (const auto& f : kFaces) {
            const auto c0 = static_cast<std::size_t>(f[0]);
            const auto c1 = static_cast<std::size_t>(f[1]);
            const auto c2 = static_cast<std::size_t>(f[2]);
            const auto c3 = static_cast<std::size_t>(f[3]);
            std::uint32_t colour = kShadow;
            if (!flat) {
                // The face's normal, turned to point out of the block, against the sun.
                Vec3 normal = cross(sub(world[c1], world[c0]), sub(world[c3], world[c0]));
                const Vec3 face_mid{(world[c0].x + world[c2].x) * 0.5f,
                                    (world[c0].y + world[c2].y) * 0.5f,
                                    (world[c0].z + world[c2].z) * 0.5f};
                if (dot(normal, sub(face_mid, centre)) < 0.0f) {
                    normal = {-normal.x, -normal.y, -normal.z};
                }
                const float len = std::sqrt(dot(normal, normal));
                const float lit =
                    len > 0.0f ? std::max(0.0f, dot(normal, kSun) / (len * sun_len)) : 0.0f;
                colour = shaded(blk.colour, kAmbient + (1.0f - kAmbient) * lit);
            }
            const auto id = static_cast<std::int16_t>(bi);
            fill(pr[c0], pr[c1], pr[c2], colour, id, s, depth);
            fill(pr[c0], pr[c2], pr[c3], colour, id, s, depth);
        }
    }
    return s;
}

// A 1 px dark line around everything drawn, in the empty pixels next to it.
void outline(Sprite& s) {
    const std::vector<std::int16_t> drawn = s.block;
    for (int y = 0; y < s.h; ++y) {
        for (int x = 0; x < s.w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * static_cast<std::size_t>(s.w) +
                                  static_cast<std::size_t>(x);
            if (drawn[i] >= 0) {
                continue;
            }
            const auto filled = [&](int nx, int ny) {
                return nx >= 0 && ny >= 0 && nx < s.w && ny < s.h &&
                       drawn[static_cast<std::size_t>(ny) * static_cast<std::size_t>(s.w) +
                             static_cast<std::size_t>(nx)] >= 0;
            };
            if (filled(x - 1, y) || filled(x + 1, y) || filled(x, y - 1) || filled(x, y + 1)) {
                s.pixels[i] = kOutline;
                s.block[i] = -2;
            }
        }
    }
}

} // namespace

Vec3 pose_point(Vec3 p, const Pose& pose) noexcept {
    // Bank about the nose (y): positive puts the right wing (x > 0) down.
    const float b = pose.bank_deg * kDegToRad;
    const Vec3 banked{p.x * std::cos(b) + p.z * std::sin(b), p.y,
                      p.z * std::cos(b) - p.x * std::sin(b)};
    // Pitch about the wings (x): positive puts the nose (y > 0) up.
    const float t = pose.pitch_deg * kDegToRad;
    const Vec3 pitched{banked.x, banked.y * std::cos(t) - banked.z * std::sin(t),
                       banked.y * std::sin(t) + banked.z * std::cos(t)};
    // Heading about the vertical: forward goes to (sin h, cos h), right to (cos h, -sin h).
    const float hd = pose.heading_deg * kDegToRad;
    return {pitched.x * std::cos(hd) + pitched.y * std::sin(hd),
            -pitched.x * std::sin(hd) + pitched.y * std::cos(hd), pitched.z};
}

Sprite render_model(const std::vector<Block>& model, const Pose& pose, int w, int h, float ox,
                    float oy, std::uint32_t parts) {
    Sprite s = render(model, pose, w, h, ox, oy, parts, false);
    outline(s);
    return s;
}

Sprite render_shadow(const std::vector<Block>& model, const Pose& pose, int w, int h, float ox,
                     float oy, std::uint32_t parts) {
    return render(model, pose, w, h, ox, oy, parts, true);
}

} // namespace wof
