#include "core/progress.hpp"

#include <algorithm>
#include <cstddef>

#include "core/text.hpp"

namespace wof {
namespace {

constexpr std::array<Nation, 2> kNations{Nation::US, Nation::Germany};

[[nodiscard]] const char* nation_key(Nation n) noexcept {
    return n == Nation::Germany ? "de" : "us";
}

[[nodiscard]] bool valid_step(int step) noexcept {
    return step >= 0 && step < kTreeSteps;
}

} // namespace

int plane_price(int step, const Prices& p) noexcept {
    return step <= 0 ? 0 : step == 1 ? p.step2 : p.step3;
}

int upgrade_price(Upgrade u, const Prices& p) noexcept {
    switch (u) {
    case Upgrade::Guns:
        return p.guns;
    case Upgrade::Engine:
        return p.engine;
    case Upgrade::Fuel:
        return p.fuel;
    case Upgrade::Armour:
        return p.armour;
    case Upgrade::Bombs:
        return p.bombs;
    case Upgrade::Sight:
        return p.sight;
    }
    return 0;
}

BuyState plane_state(const NationProgress& n, int step, const Prices& p) noexcept {
    if (!valid_step(step)) {
        return BuyState::Locked;
    }
    const auto i = static_cast<std::size_t>(step);
    if (n.planes[i].owned) {
        return BuyState::Owned;
    }
    if (step > 0 && !n.planes[i - 1].owned) {
        return BuyState::Locked;
    }
    return n.points >= plane_price(step, p) ? BuyState::Open : BuyState::TooDear;
}

BuyState upgrade_state(const NationProgress& n, int step, Upgrade u, const Prices& p) noexcept {
    if (!valid_step(step) || !n.planes[static_cast<std::size_t>(step)].owned) {
        return BuyState::Locked;
    }
    if (has_upgrade(n.planes[static_cast<std::size_t>(step)].upgrades, u)) {
        return BuyState::Owned;
    }
    return n.points >= upgrade_price(u, p) ? BuyState::Open : BuyState::TooDear;
}

bool buy_plane(NationProgress& n, int step, const Prices& p) noexcept {
    if (plane_state(n, step, p) != BuyState::Open) {
        return false;
    }
    n.points -= plane_price(step, p);
    n.planes[static_cast<std::size_t>(step)].owned = true;
    return true;
}

bool buy_upgrade(NationProgress& n, int step, Upgrade u, const Prices& p) noexcept {
    if (upgrade_state(n, step, u, p) != BuyState::Open) {
        return false;
    }
    n.points -= upgrade_price(u, p);
    PlaneProgress& plane = n.planes[static_cast<std::size_t>(step)];
    plane.upgrades = static_cast<UpgradeSet>(plane.upgrades | upgrade_bit(u));
    return true;
}

std::string to_progress(const Progress& g) {
    std::string out = "# Wings of Freedom progress: points, planes and upgrades, per nation.\n";
    for (const Nation nation : kNations) {
        const NationProgress& n = g.nations[static_cast<std::size_t>(nation_slot(nation))];
        const FighterTree tree = fighter_tree(nation);
        const std::string who = nation_key(nation);
        out += who + ".points = " + std::to_string(n.points) + "\n";
        out += who + ".flying = ";
        out.append(plane_type(tree[static_cast<std::size_t>(n.flying)]).id).append("\n");
        for (std::size_t step = 0; step < tree.size(); ++step) {
            const std::string id(plane_type(tree[step]).id);
            out += id + ".owned = " + (n.planes[step].owned ? "1" : "0") + "\n";
            out += id + ".upgrades =";
            for (int u = 0; u < kUpgrades; ++u) {
                if (has_upgrade(n.planes[step].upgrades, static_cast<Upgrade>(u))) {
                    out.append(" ").append(upgrade_key(static_cast<Upgrade>(u)));
                }
            }
            out += "\n";
        }
    }
    return out;
}

bool from_progress(std::string_view text, Progress& g, std::string& errors) {
    const std::size_t errors_before = errors.size();
    KeyValueLines lines(text);
    while (lines.next()) {
        if (lines.bad()) {
            report_line(errors, lines.line(), "expected key = value");
            continue;
        }
        const std::string_view key = lines.key();
        const std::string_view value = lines.value();
        const auto dot = key.find('.');
        const std::string_view who = key.substr(0, dot);
        const std::string_view what =
            dot == std::string_view::npos ? std::string_view{} : key.substr(dot + 1);
        bool known = false;
        for (const Nation nation : kNations) {
            NationProgress& n = g.nations[static_cast<std::size_t>(nation_slot(nation))];
            const FighterTree tree = fighter_tree(nation);
            if (who == nation_key(nation) && what == "points") {
                int v = 0;
                known = parse_int(value, v) && v >= 0;
                n.points = known ? v : n.points;
            }
            for (std::size_t step = 0; step < tree.size(); ++step) {
                const std::string_view id = plane_type(tree[step]).id;
                if (who == nation_key(nation) && what == "flying" && value == id) {
                    n.flying = static_cast<int>(step);
                    known = true;
                }
                if (who != id) {
                    continue;
                }
                if (what == "owned") {
                    known = value == "0" || value == "1";
                    if (known) {
                        n.planes[step].owned = step == 0 || value == "1";
                    }
                } else if (what == "upgrades") {
                    known = true;
                    UpgradeSet set = 0;
                    std::string_view rest = value;
                    while (!(rest = trim(rest)).empty()) {
                        const auto space = rest.find(' ');
                        Upgrade u{};
                        if (parse_upgrade(rest.substr(0, space), u)) {
                            set = static_cast<UpgradeSet>(set | upgrade_bit(u));
                        } else {
                            known = false;
                        }
                        rest = space == std::string_view::npos ? std::string_view{}
                                                               : rest.substr(space);
                    }
                    n.planes[step].upgrades = set;
                }
            }
        }
        if (!known) {
            report_line(errors, lines.line(), "not understood: " + std::string(key));
        }
    }
    // The plane chosen to fly has to be one you own.
    for (NationProgress& n : g.nations) {
        n.flying = std::clamp(n.flying, 0, kTreeSteps - 1);
        if (!n.planes[static_cast<std::size_t>(n.flying)].owned) {
            n.flying = 0;
        }
    }
    return errors.size() == errors_before;
}

} // namespace wof
