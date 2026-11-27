#include "game/effects.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "core/iso.hpp"

namespace wof::game {
namespace {

constexpr int kSides = 20;

[[nodiscard]] SDL_FPoint at(PixelPt origin, float x, float y, float z) noexcept {
    const ScreenPt s = to_screen(x, y, z);
    return {s.x - static_cast<float>(origin.x), s.y - static_cast<float>(origin.y)};
}

[[nodiscard]] std::array<SDL_FPoint, kSides + 1> circle(PixelPt origin, float x, float y, float z,
                                                        float radius) noexcept {
    std::array<SDL_FPoint, kSides + 1> pts{};
    for (int i = 0; i <= kSides; ++i) {
        const float a = static_cast<float>(i) * 2.0f * std::numbers::pi_v<float> / kSides;
        pts[static_cast<std::size_t>(i)] =
            at(origin, x + std::cos(a) * radius, y + std::sin(a) * radius, z);
    }
    return pts;
}

template <typename Marks> void age(Marks& marks, float dt, float life) {
    for (auto& m : marks) {
        m.age += dt;
    }
    std::erase_if(marks, [life](const auto& m) { return m.age >= life; });
}

} // namespace

void fill_ground_disc(SDL_Renderer* r, PixelPt origin, float x, float y, float z, float radius,
                      SDL_FColor colour) {
    const auto pts = circle(origin, x, y, z, radius);
    const SDL_FPoint centre = at(origin, x, y, z);
    std::array<SDL_Vertex, kSides * 3> v{};
    for (int i = 0; i < kSides; ++i) {
        const auto k = static_cast<std::size_t>(i);
        v[k * 3] = SDL_Vertex{centre, colour, {0.0f, 0.0f}};
        v[k * 3 + 1] = SDL_Vertex{pts[k], colour, {0.0f, 0.0f}};
        v[k * 3 + 2] = SDL_Vertex{pts[k + 1], colour, {0.0f, 0.0f}};
    }
    SDL_RenderGeometry(r, nullptr, v.data(), static_cast<int>(v.size()), nullptr, 0);
}

void ground_ring(SDL_Renderer* r, PixelPt origin, float x, float y, float radius,
                 SDL_FColor colour) {
    const auto pts = circle(origin, x, y, 0.0f, radius);
    SDL_SetRenderDrawColorFloat(r, colour.r, colour.g, colour.b, colour.a);
    SDL_RenderLines(r, pts.data(), static_cast<int>(pts.size()));
}

void Effects::add(const std::vector<Event>& events, float blast_radius_u) {
    for (const Event& e : events) {
        if (e.kind == EventKind::RoundGround) {
            puffs_.push_back(Mark{e.x, e.y, 0.0f, 1.5f});
        } else if (e.kind == EventKind::Blast) {
            booms_.push_back(Mark{e.x, e.y, 0.0f, blast_radius_u});
            scorches_.push_back(Mark{e.x, e.y, 0.0f, std::max(4.0f, blast_radius_u * 0.4f)});
        }
    }
}

void Effects::tick(float dt, float scorch_fade_s) {
    age(puffs_, dt, kPuffS);
    age(booms_, dt, kBoomS);
    age(scorches_, dt, scorch_fade_s);
}

void Effects::clear() {
    puffs_.clear();
    booms_.clear();
    scorches_.clear();
}

void Effects::draw_ground(SDL_Renderer* r, PixelPt origin, float scorch_fade_s) const {
    for (const Mark& m : scorches_) {
        const float left = 1.0f - m.age / std::max(0.1f, scorch_fade_s);
        fill_ground_disc(r, origin, m.x, m.y, 0.0f, m.size,
                         SDL_FColor{0.10f, 0.08f, 0.06f, 0.55f * left});
        fill_ground_disc(r, origin, m.x, m.y, 0.0f, m.size * 0.55f,
                         SDL_FColor{0.05f, 0.04f, 0.03f, 0.6f * left});
    }
}

void Effects::draw_air(SDL_Renderer* r, PixelPt origin) const {
    for (const Mark& m : puffs_) {
        const float left = 1.0f - m.age / kPuffS;
        fill_ground_disc(r, origin, m.x, m.y, m.age * 6.0f, m.size + m.age * 4.0f,
                         SDL_FColor{0.80f, 0.70f, 0.52f, 0.85f * left});
    }
    for (const Mark& m : booms_) {
        const float t = m.age / kBoomS; // 0..1
        if (t < 0.25f) {
            // The flash.
            fill_ground_disc(r, origin, m.x, m.y, 2.0f, m.size * (0.35f + t * 1.4f),
                             SDL_FColor{1.0f, 0.93f, 0.62f, 0.95f});
        }
        // Smoke rising, and the blast's edge running out to its full radius.
        fill_ground_disc(r, origin, m.x, m.y, t * 14.0f, m.size * (0.25f + 0.35f * t),
                         SDL_FColor{0.20f, 0.18f, 0.16f, 0.7f * (1.0f - t)});
        ground_ring(r, origin, m.x, m.y, m.size * t, SDL_FColor{1.0f, 0.62f, 0.25f, 1.0f - t});
    }
}

} // namespace wof::game
