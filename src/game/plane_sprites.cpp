#include "game/plane_sprites.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "core/damage.hpp"
#include "core/fighter.hpp"
#include "core/iso.hpp"
#include "core/raster.hpp"

namespace wof::game {
namespace {

constexpr float kBakedBankDeg = 30.0f;

[[nodiscard]] constexpr int cell_px(ModelId id, int step) noexcept {
    return PlaneSprites::kCellPerStep * step * model_cells(id);
}

// Copies a cell's pixels into an atlas `atlas_w` wide.
void blit(const Sprite& s, std::vector<std::uint32_t>& atlas, int atlas_w, int column, int row) {
    for (int y = 0; y < s.h; ++y) {
        for (int x = 0; x < s.w; ++x) {
            const auto ax = static_cast<std::size_t>(column * s.w + x);
            const auto ay = static_cast<std::size_t>(row * s.h + y);
            atlas[ay * static_cast<std::size_t>(atlas_w) + ax] = s.at(x, y);
        }
    }
}

SDL_Texture* upload(SDL_Renderer* r, const std::vector<std::uint32_t>& pixels, int w, int h) {
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
    if (t == nullptr) {
        SDL_Log("plane atlas %dx%d failed: %s", w, h, SDL_GetError());
        return nullptr;
    }
    SDL_UpdateTexture(t, nullptr, pixels.data(), w * 4);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

} // namespace

PlaneSprites::~PlaneSprites() {
    shutdown();
}

void PlaneSprites::shutdown() {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        quit_ = true;
        jobs_.clear();
    }
    wake_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    for (auto& entry : atlases_) {
        if (entry.second.plane != nullptr) {
            SDL_DestroyTexture(entry.second.plane);
        }
        if (entry.second.shadow != nullptr) {
            SDL_DestroyTexture(entry.second.shadow);
        }
    }
    atlases_.clear();
    queued_.clear();
    done_.clear();
}

PlaneSprites::Pixels PlaneSprites::bake_pixels(ModelId id, std::uint32_t parts, int step) {
    const Uint64 start = SDL_GetTicksNS();
    const int cell = cell_px(id, step);
    const int atlas_w = cell * kColumns;
    const std::vector<Block> model =
        scaled_model(plane_model(id), static_cast<float>(step) / kFullSizeStep);
    const auto n = static_cast<std::size_t>(atlas_w) * static_cast<std::size_t>(cell * kRows);
    const bool with_shadow = step <= kFullSizeStep; // shadows lie below you: never bigger
    Pixels out;
    out.plane.assign(n, 0u);
    if (with_shadow) {
        out.shadow.assign(n, 0u);
    }
    const float mid = static_cast<float>(cell) * 0.5f;
    for (int h = 0; h < kColumns; ++h) {
        for (int p = 0; p < kPitches; ++p) {
            for (int b = 0; b < kBanks; ++b) {
                const Pose pose{static_cast<float>(h) * (360.0f / kHeadings),
                                static_cast<float>(p - kPitches / 2) * 10.0f,
                                static_cast<float>(b - 1) * kBakedBankDeg};
                const int row = p * kBanks + b;
                blit(render_model(model, pose, cell, cell, mid, mid, parts), out.plane, atlas_w, h,
                     row);
                if (with_shadow) {
                    blit(render_shadow(model, pose, cell, cell, mid, mid, parts), out.shadow,
                         atlas_w, h, row);
                }
            }
        }
    }
    out.ms = static_cast<double>(SDL_GetTicksNS() - start) / 1e6;
    return out;
}

PlaneSprites::Atlas PlaneSprites::upload_atlas(const Pixels& pixels, ModelId id, int step) const {
    const int w = cell_px(id, step) * kColumns;
    const int h = cell_px(id, step) * kRows;
    Atlas a;
    a.plane = upload(renderer_, pixels.plane, w, h);
    if (!pixels.shadow.empty()) {
        a.shadow = upload(renderer_, pixels.shadow, w, h);
    }
    return a;
}

PlaneSprites::Atlas PlaneSprites::bake_now(const Job& job) {
    const Pixels pixels = bake_pixels(job.id, job.parts, job.step);
    const Atlas a = upload_atlas(pixels, job.id, job.step);
    atlases_[job.key] = a;
    queued_.erase(job.key);
    SDL_Log("plane sprites (model %d, parts 0x%02x, %d/8): baked in %.1f ms",
            static_cast<int>(job.id), job.parts, job.step, pixels.ms);
    if (job.step == kFullSizeStep && job.parts == kAllParts) {
        bake_ms_ = pixels.ms;
    }
    return a;
}

bool PlaneSprites::bake(ModelId id) {
    const Job full{key_of(id, kAllParts, kFullSizeStep), id, kAllParts, kFullSizeStep};
    const auto found = atlases_.find(full.key);
    const Atlas a = found != atlases_.end() ? found->second : bake_now(full);
    if (!in_line_) {
        for (int step = kMinSizeStep; step <= kMaxSizeStep; ++step) {
            request(Job{key_of(id, kAllParts, step), id, kAllParts, step}, false);
        }
    }
    return a.plane != nullptr && a.shadow != nullptr;
}

void PlaneSprites::queue(ModelId id) {
    if (!in_line_) {
        request(Job{key_of(id, kAllParts, kFullSizeStep), id, kAllParts, kFullSizeStep}, false);
    }
}

void PlaneSprites::request(const Job& job, bool urgent) {
    if (atlases_.count(job.key) != 0) {
        return;
    }
    const bool already = !queued_.insert(job.key).second;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        const auto waiting = std::find_if(jobs_.begin(), jobs_.end(),
                                          [&](const Job& j) { return j.key == job.key; });
        if (already) {
            // Needed on screen: move it up. (Not found means it's being baked right now.)
            if (urgent && waiting != jobs_.end()) {
                const Job j = *waiting;
                jobs_.erase(waiting);
                jobs_.push_front(j);
            }
        } else if (urgent) {
            jobs_.push_front(job);
        } else {
            jobs_.push_back(job);
        }
    }
    wake_.notify_one();
    if (!worker_.joinable()) {
        worker_ = std::thread([this] { work(); });
    }
}

void PlaneSprites::work() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return quit_ || !jobs_.empty(); });
            if (quit_) {
                return;
            }
            job = jobs_.front();
            jobs_.pop_front();
        }
        Done done{job, bake_pixels(job.id, job.parts, job.step)};
        const std::lock_guard<std::mutex> lock(mutex_);
        done_.push_back(std::move(done));
    }
}

void PlaneSprites::pump() {
    Done done;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (done_.empty()) {
            return;
        }
        done = std::move(done_.front());
        done_.pop_front();
    }
    queued_.erase(done.job.key);
    if (atlases_.count(done.job.key) != 0) {
        return; // baked on this thread meanwhile
    }
    atlases_[done.job.key] = upload_atlas(done.pixels, done.job.id, done.job.step);
    SDL_Log("plane sprites (model %d, parts 0x%02x, %d/8): baked in %.1f ms in the background",
            static_cast<int>(done.job.id), done.job.parts, done.job.step, done.pixels.ms);
}

PlaneSprites::Found PlaneSprites::nearest(ModelId id, std::uint32_t parts, int step,
                                          bool shadow) const {
    for (const std::uint32_t p : {parts, static_cast<std::uint32_t>(kAllParts)}) {
        for (int d = 0; d <= kMaxSizeStep; ++d) {
            for (const int s : {step - d, step + d}) {
                if (s < kMinSizeStep || s > kMaxSizeStep) {
                    continue;
                }
                const auto it = atlases_.find(key_of(id, p, s));
                if (it == atlases_.end()) {
                    continue;
                }
                SDL_Texture* t = shadow ? it->second.shadow : it->second.plane;
                if (t != nullptr) {
                    return {t, s};
                }
            }
        }
    }
    return {};
}

PlaneSprites::Found PlaneSprites::find(ModelId id, std::uint32_t parts, int step, bool shadow) {
    const Job job{key_of(id, parts, step), id, parts, step};
    if (atlases_.count(job.key) == 0) {
        if (in_line_) {
            bake_now(job);
        } else {
            request(job, true);
        }
    }
    Found f = nearest(id, parts, step, shadow);
    if (f.texture == nullptr && !in_line_) {
        // Nothing of this model yet: bake its full size now (once).
        bake_now(Job{key_of(id, kAllParts, kFullSizeStep), id, kAllParts, kFullSizeStep});
        f = nearest(id, parts, step, shadow);
    }
    return f;
}

PlaneSprites::Cell PlaneSprites::cell_of(const Body& b) noexcept {
    const auto pitch = static_cast<int>(std::lround(b.pitch_deg / 10.0f)) + kPitches / 2;
    const int bank = b.bank_deg < -15.0f ? 0 : b.bank_deg > 15.0f ? 2 : 1;
    return {heading_index(b.heading_deg), std::clamp(pitch, 0, kPitches - 1) * kBanks + bank};
}

void PlaneSprites::draw(const Body& b, const ViewXf& xf, bool shadow, SDL_Color tint, bool flash) {
    // The plane is sized by its depth; its shadow lies on the ground, so it's sized with that.
    const float z = shadow ? 0.0f : b.z;
    int step = size_step(xf.scale_at(z));
    if (shadow) {
        step = std::min(step, kFullSizeStep);
    }
    const std::uint32_t parts = kAllParts & ~missing_parts(b);
    const ModelId model = plane_type(b.type).model;
    const Found f = find(model, parts, step, shadow);
    if (f.texture == nullptr) {
        return;
    }
    const Cell c = cell_of(b);
    const auto src_cell = static_cast<float>(cell_px(model, f.step));
    const auto size = static_cast<float>(cell_px(model, step));
    const ScreenPt at = xf.at(b.x, b.y, z);
    const SDL_FRect src{static_cast<float>(c.column) * src_cell,
                        static_cast<float>(c.row) * src_cell, src_cell, src_cell};
    const SDL_FRect dst{std::round(at.x - size * 0.5f), std::round(at.y - size * 0.5f), size, size};
    SDL_SetTextureColorMod(f.texture, tint.r, tint.g, tint.b);
    SDL_RenderTexture(renderer_, f.texture, &src, &dst);
    if (flash && !shadow) {
        // A hit: the same sprite again, added on top, so it glows.
        SDL_SetTextureBlendMode(f.texture, SDL_BLENDMODE_ADD);
        SDL_SetTextureColorMod(f.texture, 255, 255, 255);
        SDL_RenderTexture(renderer_, f.texture, &src, &dst);
        SDL_SetTextureBlendMode(f.texture, SDL_BLENDMODE_BLEND);
    }
    SDL_SetTextureColorMod(f.texture, 255, 255, 255);
}

bool PlaneSprites::save_sheet(const char* path, ModelId id, int step) {
    const int s_step = std::clamp(step, kMinSizeStep, kMaxSizeStep);
    std::vector<std::uint32_t> sheet = bake_pixels(id, kAllParts, s_step).plane;
    for (std::uint32_t& px : sheet) {
        if ((px >> 24u) == 0u) {
            px = 0xff628c46u; // pasture green, to judge how the plane reads on the ground
        }
    }
    const int w = cell_px(id, s_step) * kColumns;
    const int h = cell_px(id, s_step) * kRows;
    SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ARGB8888, sheet.data(), w * 4);
    if (s == nullptr) {
        SDL_Log("sprite sheet surface failed: %s", SDL_GetError());
        return false;
    }
    const bool ok = SDL_SavePNG(s, path);
    if (ok) {
        SDL_Log("saved %s (%dx%d)", path, w, h);
    } else {
        SDL_Log("saving %s failed: %s", path, SDL_GetError());
    }
    SDL_DestroySurface(s);
    return ok;
}

} // namespace wof::game
