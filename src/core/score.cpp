#include "core/score.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "core/world.hpp"

namespace wof {

float chain_multiplier(int chain, const ScoreParams& p) noexcept {
    if (chain <= 1) {
        return 1.0f;
    }
    return std::min(p.chain_max, 1.0f + p.chain_step * static_cast<float>(chain - 1));
}

void score_kill(RunScore& s, Role role, const ScoreParams& p) noexcept {
    s.chain = s.chain_s > 0.0f ? s.chain + 1 : 1;
    s.chain_s = p.chain_window_s;
    const int base = role == Role::Bomber ? p.bomber_points : p.fighter_points;
    s.points +=
        static_cast<int>(std::lround(static_cast<float>(base) * chain_multiplier(s.chain, p)));
    ++(role == Role::Bomber ? s.bombers : s.fighters);
}

void score_tick(RunScore& s, const World& w, const ScoreParams& p, float dt) noexcept {
    if (s.chain_s > 0.0f) {
        s.chain_s -= dt;
        if (s.chain_s <= 0.0f) {
            s.chain_s = 0.0f;
            s.chain = 0;
        }
    }
    for (const Event& e : w.events) {
        if (e.kind == EventKind::WaveArrived) {
            ++s.waves;
        } else if (e.kind == EventKind::EnemyDown && e.by == 0) {
            const Body& b = w.bodies[static_cast<std::size_t>(e.who)];
            score_kill(s, plane_type(b.type).role, p);
        }
    }
}

float condition_of(const Body& b, const DamageParams& d) noexcept {
    const float full = static_cast<float>(b.damage.engines) * d.engine_hp + 2.0f * d.wing_hp +
                       d.fuselage_hp + d.tail_hp;
    const float parts = full > 0.0f ? std::clamp(total_hp(b) / full, 0.0f, 1.0f) : 0.0f;
    const float pilot = d.pilot_wounds > 0
                            ? std::clamp(1.0f - static_cast<float>(b.damage.wounds) /
                                                    static_cast<float>(d.pilot_wounds),
                                         0.0f, 1.0f)
                            : 1.0f;
    return parts * pilot;
}

bool got_home(const Body& b) noexcept {
    return b.outcome == Outcome::ReturnedHome && !b.damage.going_down;
}

Payout payout(const RunScore& s, const Body& b, const DamageParams& d,
              const ScoreParams& p) noexcept {
    Payout r;
    r.banked = got_home(b);
    r.points = s.points;
    r.condition_share = p.condition_bonus * condition_of(b, d);
    r.wave_share = p.wave_bonus * static_cast<float>(s.waves);
    if (r.banked) {
        r.total = static_cast<int>(
            std::lround(static_cast<float>(s.points) * (1.0f + r.condition_share + r.wave_share)));
    }
    return r;
}

} // namespace wof
