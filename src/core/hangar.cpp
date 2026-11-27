#include "core/hangar.hpp"

#include <algorithm>
#include <cstddef>

namespace wof {

HangarRow hangar_row(int row) noexcept {
    if (row < kTreeSteps) {
        return {HangarKind::Plane, std::max(0, row)};
    }
    if (row < kHangarFly) {
        return {HangarKind::Upgrade, row - kTreeSteps};
    }
    if (row == kHangarFly) {
        return {HangarKind::Fly, 0};
    }
    return {row == kHangarReset ? HangarKind::Reset : HangarKind::Back, 0};
}

Hangar open_hangar(const NationProgress& n) noexcept {
    return {n.flying, n.flying, false};
}

void hangar_point(Hangar& h, int row) noexcept {
    row = std::clamp(row, 0, kHangarRows - 1);
    if (row == h.row) {
        return;
    }
    h.row = row;
    h.confirm = false;
    if (row < kTreeSteps) {
        h.step = row;
    }
}

void hangar_move(Hangar& h, int by) noexcept {
    hangar_point(h, ((h.row + by) % kHangarRows + kHangarRows) % kHangarRows);
}

HangarAction hangar_select(Hangar& h, NationProgress& n, const Prices& p) noexcept {
    const HangarRow row = hangar_row(h.row);
    // A purchase: the first press asks, the second buys.
    const auto confirmed = [&h]() {
        const bool yes = h.confirm;
        h.confirm = !yes;
        return yes;
    };
    switch (row.kind) {
    case HangarKind::Plane:
        h.step = row.index;
        if (plane_state(n, row.index, p) == BuyState::Owned) {
            if (n.flying == row.index) {
                return HangarAction::None;
            }
            n.flying = row.index;
            return HangarAction::Changed;
        }
        if (plane_state(n, row.index, p) == BuyState::Open && confirmed()) {
            buy_plane(n, row.index, p);
            n.flying = row.index;
            return HangarAction::Changed;
        }
        return HangarAction::None;
    case HangarKind::Upgrade: {
        const auto u = static_cast<Upgrade>(row.index);
        if (upgrade_state(n, h.step, u, p) == BuyState::Open && confirmed()) {
            buy_upgrade(n, h.step, u, p);
            return HangarAction::Changed;
        }
        return HangarAction::None;
    }
    case HangarKind::Fly:
        return HangarAction::Fly;
    case HangarKind::Reset:
        if (confirmed()) {
            n = NationProgress{};
            h = Hangar{kHangarReset, 0, false};
            return HangarAction::Changed;
        }
        return HangarAction::None;
    case HangarKind::Back:
        return HangarAction::Back;
    }
    return HangarAction::None;
}

} // namespace wof
