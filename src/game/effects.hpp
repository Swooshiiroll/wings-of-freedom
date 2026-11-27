#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

#include "core/camera.hpp"
#include "game/view_xf.hpp"
#include "core/weapons.hpp"

namespace wof::game {

// Short-lived things drawn from the simulation's events (design/flight.md, Weapons and Damage):
// dust puffs where rounds hit the ground, bomb and crash explosions, the scorch marks they leave
// (which fade out), sparks where rounds strike a plane, and the smoke and fuel spray trailing a
// damaged one. They age with the simulation's ticks, so screenshots are repeatable.
class Effects {
  public:
    static constexpr float kPuffS = 0.25f;
    static constexpr float kBoomS = 0.5f;
    static constexpr float kSmokeS = 1.2f;
    static constexpr float kSprayS = 0.5f;
    static constexpr float kSparkS = 0.12f;

    enum class Trail : std::uint8_t { Grey, Black, Spray };

    // Starts the effects for a tick's events: puffs, blasts and scorches, and sparks on planes hit.
    // `blast_radius_u` sizes the bomb explosions.
    void add(const std::vector<Event>& events, float blast_radius_u);

    // One puff of smoke (or fuel spray) left behind at (x, y, z).
    void trail(float x, float y, float z, Trail kind);

    // A fireball at (x, y, z): a plane hitting the ground or destroyed in the air. It leaves a
    // scorch mark if it's on the ground.
    void explode(float x, float y, float z, float size);

    // Ages everything by dt, dropping what's finished (scorch marks after `scorch_fade_s`).
    void tick(float dt, float scorch_fade_s);

    void clear();

    // Scorch marks, drawn on the ground under everything else.
    void draw_ground(SDL_Renderer* r, const ViewXf& xf, float scorch_fade_s) const;

    // Dust puffs, smoke, sparks and explosions.
    void draw_air(SDL_Renderer* r, const ViewXf& xf) const;

  private:
    struct Mark {
        float x = 0.0f;
        float y = 0.0f;
        float age = 0.0f;
        float size = 0.0f;
        float z = 0.0f;
        Trail kind = Trail::Grey;
    };
    std::vector<Mark> puffs_;
    std::vector<Mark> booms_;
    std::vector<Mark> scorches_;
    std::vector<Mark> smoke_;
    std::vector<Mark> sparks_;
};

// A filled circle of `radius` u on the ground (an ellipse on screen), centred on (x, y) at height
// z.
void fill_ground_disc(SDL_Renderer* r, const ViewXf& xf, float x, float y, float z, float radius,
                      SDL_FColor colour);

// The outline of the same circle.
void ground_ring(SDL_Renderer* r, const ViewXf& xf, float x, float y, float z, float radius,
                 SDL_FColor colour);

} // namespace wof::game
