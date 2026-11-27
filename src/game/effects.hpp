#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "core/camera.hpp"
#include "core/weapons.hpp"

namespace wof::game {

// Short-lived things drawn from the simulation's events (design/flight.md, Weapons): dust puffs
// where rounds hit the ground, bomb explosions, and the scorch marks they leave, which fade out.
// They age with the simulation's ticks, so screenshots are repeatable.
class Effects {
  public:
    static constexpr float kPuffS = 0.25f;
    static constexpr float kBoomS = 0.5f;

    // Starts the effects for a tick's events. `blast_radius_u` sizes the explosions.
    void add(const std::vector<Event>& events, float blast_radius_u);

    // Ages everything by dt, dropping what's finished (scorch marks after `scorch_fade_s`).
    void tick(float dt, float scorch_fade_s);

    void clear();

    // Scorch marks, drawn on the ground under everything else.
    void draw_ground(SDL_Renderer* r, PixelPt origin, float scorch_fade_s) const;

    // Dust puffs and explosions.
    void draw_air(SDL_Renderer* r, PixelPt origin) const;

  private:
    struct Mark {
        float x = 0.0f;
        float y = 0.0f;
        float age = 0.0f;
        float size = 0.0f;
    };
    std::vector<Mark> puffs_;
    std::vector<Mark> booms_;
    std::vector<Mark> scorches_;
};

// A filled circle of `radius` u on the ground (an ellipse on screen), centred on (x, y) at height
// z.
void fill_ground_disc(SDL_Renderer* r, PixelPt origin, float x, float y, float z, float radius,
                      SDL_FColor colour);

// The outline of the same circle.
void ground_ring(SDL_Renderer* r, PixelPt origin, float x, float y, float radius,
                 SDL_FColor colour);

} // namespace wof::game
