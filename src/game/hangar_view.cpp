#include "game/hangar_view.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "core/plane_rules.hpp"
#include "core/planes.hpp"
#include "core/upgrades.hpp"

namespace wof::game {
namespace {

constexpr float kGlyph = 8.0f; // the debug font's character size
constexpr float kPanelW = 464.0f;
constexpr float kPanelH = 222.0f;
constexpr float kRowH = 14.0f;
constexpr float kLeftW = 188.0f; // the planes' and buttons' column
constexpr float kRightX = 204.0f;
constexpr float kRightW = 252.0f; // the upgrades' column

constexpr SDL_Color kTitle{255, 236, 170, 255};
constexpr SDL_Color kText{236, 232, 200, 255};
constexpr SDL_Color kDim{140, 140, 128, 255};
constexpr SDL_Color kGood{160, 220, 140, 255};
constexpr SDL_Color kWarn{240, 170, 90, 255};

void colour(SDL_Renderer* r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

void text(SDL_Renderer* r, float x, float y, const std::string& s, SDL_Color c) {
    colour(r, c);
    SDL_RenderDebugText(r, std::floor(x), std::floor(y), s.c_str());
}

void right(SDL_Renderer* r, float x_end, float y, const std::string& s, SDL_Color c) {
    text(r, x_end - static_cast<float>(s.size()) * kGlyph, y, s, c);
}

void centred(SDL_Renderer* r, int view_w, float y, const std::string& s, SDL_Color c) {
    text(r, (static_cast<float>(view_w) - static_cast<float>(s.size()) * kGlyph) * 0.5f, y, s, c);
}

[[nodiscard]] SDL_FPoint panel_origin(int view_w, int view_h) noexcept {
    return {std::floor((static_cast<float>(view_w) - kPanelW) * 0.5f),
            std::floor((static_cast<float>(view_h) - kPanelH) * 0.5f)};
}

[[nodiscard]] SDL_FRect row_rect(int view_w, int view_h, int row) noexcept {
    const SDL_FPoint o = panel_origin(view_w, view_h);
    const HangarRow h = hangar_row(row);
    switch (h.kind) {
    case HangarKind::Plane:
        return {o.x + 8.0f, o.y + 34.0f + static_cast<float>(h.index) * kRowH, kLeftW, kRowH};
    case HangarKind::Upgrade:
        return {o.x + kRightX, o.y + 66.0f + static_cast<float>(h.index) * kRowH, kRightW, kRowH};
    case HangarKind::Fly:
    case HangarKind::Reset:
    case HangarKind::Back:
        break;
    }
    return {o.x + 8.0f, o.y + 92.0f + static_cast<float>(row - kHangarFly) * kRowH, kLeftW, kRowH};
}

[[nodiscard]] std::string upper(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
    }
    return out;
}

// The plane's name without its maker: "P-12E", "HE 51B".
[[nodiscard]] std::string short_name(PlaneId id) {
    const std::string_view name = plane_type(id).name;
    const auto space = name.find(' ');
    return upper(space == std::string_view::npos ? name : name.substr(space + 1));
}

// "2 x .30 + 1 x .50" (7.9 for the German rifle calibre).
[[nodiscard]] std::string guns_text(const PlaneRules& pr, UnitSystem units) {
    std::array<int, kCalibres> count{};
    for (int g = 0; g < pr.gun_count; ++g) {
        const GunSpec& spec = pr.guns[static_cast<std::size_t>(g)];
        if (spec.mount == Mount::Fixed) {
            ++count[static_cast<std::size_t>(spec.calibre)];
        }
    }
    std::string out;
    for (int c = 0; c < kCalibres; ++c) {
        const int n = count[static_cast<std::size_t>(c)];
        if (n == 0) {
            continue;
        }
        const auto cal = static_cast<Calibre>(c);
        const char* name =
            cal == Calibre::Rifle && units == UnitSystem::Metric ? "7.9" : calibre_name(cal);
        out += (out.empty() ? "" : " + ") + std::to_string(n) + " x " + name;
    }
    return out;
}

[[nodiscard]] std::string bombs_text(const PlaneRules& pr, UnitSystem units) {
    if (pr.bombs <= 0) {
        return "NONE";
    }
    const bool metric = units == UnitSystem::Metric;
    const float weight = metric ? pr.bomb_lb * 0.45359237f : pr.bomb_lb;
    return std::to_string(pr.bombs) + " x " + grouped(weight, 1) + (metric ? " KG" : " LB");
}

[[nodiscard]] std::string speed_text(const PlaneRules& pr, const PlaneScale& s, UnitSystem units) {
    const float mph = mph_of(pr.flight.max_level_u_s, s);
    return units == UnitSystem::Imperial ? grouped(mph, 1) + " MPH"
                                         : grouped(mph * kKmhPerMph, 1) + " KM/H";
}

[[nodiscard]] std::string climb_text(const PlaneRules& pr, const PlaneScale& s, UnitSystem units) {
    const float fpm = best_climb(pr.flight) / s.u_per_fpm;
    return units == UnitSystem::Imperial
               ? grouped(fpm, 10) + " FT/MIN"
               : std::to_string(static_cast<int>(std::lround(fpm * kMetresPerFoot / 60.0f))) +
                     " M/S";
}

// The status after a plane or upgrade: what it is or costs.
[[nodiscard]] std::string status(BuyState state, int price, SDL_Color& c) {
    switch (state) {
    case BuyState::Owned:
        c = kGood;
        return "OWNED";
    case BuyState::Locked:
        c = kDim;
        return grouped(static_cast<float>(price), 1);
    case BuyState::TooDear:
        c = kDim;
        return grouped(static_cast<float>(price), 1);
    case BuyState::Open:
        c = kTitle;
        return grouped(static_cast<float>(price), 1);
    }
    return "";
}

// What the highlighted row does.
[[nodiscard]] std::string hint(const Hangar& h, const NationProgress& n, Nation nation,
                               const Prices& p) {
    const HangarRow row = hangar_row(h.row);
    const FighterTree tree = fighter_tree(nation);
    const auto need = [&](int price) {
        return "NEED " + grouped(static_cast<float>(price - n.points), 1) + " MORE POINTS";
    };
    switch (row.kind) {
    case HangarKind::Plane: {
        const int price = plane_price(row.index, p);
        switch (plane_state(n, row.index, p)) {
        case BuyState::Owned:
            return n.flying == row.index ? "YOUR PLANE" : "ENTER: FLY THIS PLANE";
        case BuyState::Locked:
            return "BUY THE " + short_name(tree[static_cast<std::size_t>(row.index - 1)]) +
                   " FIRST";
        case BuyState::TooDear:
            return need(price);
        case BuyState::Open:
            return h.confirm ? "PRESS ENTER AGAIN TO BUY"
                             : "ENTER: BUY FOR " + grouped(static_cast<float>(price), 1);
        }
        return "";
    }
    case HangarKind::Upgrade: {
        const auto u = static_cast<Upgrade>(row.index);
        const UpgradeInfo info = upgrade_info(tree[static_cast<std::size_t>(h.step)], u);
        const char* kind = u == Upgrade::Sight ? "SHOWS WHERE BOMBS LAND. "
                           : info.real         ? "A REAL FIT. "
                                               : "A FIELD MOD. ";
        const int price = upgrade_price(u, p);
        switch (upgrade_state(n, h.step, u, p)) {
        case BuyState::Owned:
            return std::string(kind) + "FITTED";
        case BuyState::Locked:
            return "BUY THE PLANE FIRST";
        case BuyState::TooDear:
            return kind + need(price);
        case BuyState::Open:
            return h.confirm ? "PRESS ENTER AGAIN TO BUY" : kind + std::string("ENTER: BUY");
        }
        return "";
    }
    case HangarKind::Fly:
        return "FLY THE " + short_name(tree[static_cast<std::size_t>(n.flying)]);
    case HangarKind::Reset:
        return h.confirm ? "PRESS ENTER AGAIN TO ERASE THIS NATION"
                         : "ERASE THIS NATION'S POINTS AND PLANES";
    case HangarKind::Back:
        return "CHOOSE ANOTHER NATION";
    }
    return "";
}

} // namespace

int hangar_row_at(int view_w, int view_h, float x, float y) noexcept {
    for (int row = 0; row < kHangarRows; ++row) {
        const SDL_FRect rc = row_rect(view_w, view_h, row);
        if (x >= rc.x && x < rc.x + rc.w && y >= rc.y && y < rc.y + rc.h) {
            return row;
        }
    }
    return -1;
}

void draw_hangar(SDL_Renderer* r, const Hangar& h, const NationProgress& n, Nation nation,
                 const Rules& rules, UnitSystem units, int view_w, int view_h) {
    SDL_SetRenderDrawColor(r, 22, 30, 40, 255);
    SDL_RenderClear(r);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    const SDL_FPoint o = panel_origin(view_w, view_h);
    const SDL_FRect panel{o.x, o.y, kPanelW, kPanelH};
    SDL_SetRenderDrawColor(r, 10, 14, 18, 255);
    SDL_RenderFillRect(r, &panel);
    text(r, o.x + 8.0f, o.y + 8.0f,
         std::string("HANGAR  ") + (nation == Nation::Germany ? "GERMANY" : "UNITED STATES"),
         kTitle);
    right(r, o.x + kPanelW - 8.0f, o.y + 8.0f, "POINTS " + grouped(static_cast<float>(n.points), 1),
          kTitle);

    // The cursor.
    const SDL_FRect at = row_rect(view_w, view_h, h.row);
    SDL_SetRenderDrawColor(r, 255, 220, 120, 60);
    SDL_RenderFillRect(r, &at);

    const FighterTree tree = fighter_tree(nation);
    for (int step = 0; step < kTreeSteps; ++step) {
        const SDL_FRect rc = row_rect(view_w, view_h, step);
        const PlaneId id = tree[static_cast<std::size_t>(step)];
        const bool owned = n.planes[static_cast<std::size_t>(step)].owned;
        // The plane whose upgrades show, marked.
        if (step == h.step) {
            text(r, rc.x, rc.y + 3.0f, ">", kTitle);
        }
        text(r, rc.x + 10.0f, rc.y + 3.0f, short_name(id), owned ? kText : kDim);
        SDL_Color c = kText;
        std::string s =
            status(plane_state(n, step, rules.price), plane_price(step, rules.price), c);
        if (owned && n.flying == step) {
            s = "FLYING";
        }
        right(r, rc.x + rc.w - 4.0f, rc.y + 3.0f, s, c);
    }
    constexpr std::array<const char*, 3> kButtons{"FLY", "RESET PROGRESS", "BACK"};
    for (int i = 0; i < 3; ++i) {
        const SDL_FRect rc = row_rect(view_w, view_h, kHangarFly + i);
        text(r, rc.x + 10.0f, rc.y + 3.0f, kButtons[static_cast<std::size_t>(i)],
             i == 0 ? kTitle : kText);
    }

    // The shown plane as fitted, and its upgrades.
    const PlaneId shown = tree[static_cast<std::size_t>(h.step)];
    const UpgradeSet fitted = n.planes[static_cast<std::size_t>(h.step)].upgrades;
    const PlaneRules pr =
        fit_plane(shown, fitted, rules.flight, rules.damage, rules.scale, rules.upgrade);
    const float rx = o.x + kRightX;
    text(r, rx, o.y + 26.0f, upper(plane_type(shown).name), kTitle);
    text(r, rx, o.y + 38.0f,
         speed_text(pr, rules.scale, units) + "  " + climb_text(pr, rules.scale, units), kText);
    text(r, rx, o.y + 50.0f, guns_text(pr, units) + "  BOMBS " + bombs_text(pr, units), kText);
    for (int i = 0; i < kUpgrades; ++i) {
        const auto u = static_cast<Upgrade>(i);
        const SDL_FRect rc = row_rect(view_w, view_h, kTreeSteps + i);
        const BuyState state = upgrade_state(n, h.step, u, rules.price);
        text(r, rc.x + 2.0f, rc.y + 3.0f, upgrade_info(shown, u).label,
             state == BuyState::Locked ? kDim : kText);
        SDL_Color c = kText;
        std::string s = status(state, upgrade_price(u, rules.price), c);
        if (state == BuyState::Owned) {
            s = "FITTED";
        }
        right(r, rc.x + rc.w - 2.0f, rc.y + 3.0f, s, c);
    }
    centred(r, view_w, o.y + kPanelH - 40.0f, hint(h, n, nation, rules.price),
            h.confirm ? kWarn : kText);
    centred(r, view_w, o.y + kPanelH - 18.0f, "ARROWS OR MOUSE   ENTER: CHOOSE   ESC: BACK", kDim);
}

void draw_summary(SDL_Renderer* r, const Summary& s, int view_w, int view_h) {
    const char* how = "";
    switch (s.outcome) {
    case Outcome::Crashed:
        how = "CRASHED";
        break;
    case Outcome::LeftArea:
        how = "LEFT THE BATTLE AREA";
        break;
    case Outcome::ReturnedHome:
        how = "RETURNED TO BASE";
        break;
    case Outcome::ShotDown:
        how = "SHOT DOWN";
        break;
    case Outcome::PilotKilled:
        how = "PILOT KILLED";
        break;
    case Outcome::Flying:
        return;
    }
    const auto number = [](int v) { return grouped(static_cast<float>(v), 1); };
    const auto percent = [](float share) {
        return "+" + std::to_string(static_cast<int>(std::lround(share * 100.0f))) + "%";
    };
    // Rows of label and value; an empty value centres the label.
    std::vector<std::pair<std::string, std::string>> rows;
    rows.emplace_back(how, "");
    rows.emplace_back("TIME " + std::to_string(s.seconds / 60) + ":" +
                          (s.seconds % 60 < 10 ? "0" : "") + std::to_string(s.seconds % 60) +
                          "   WAVE " + std::to_string(s.wave),
                      "");
    rows.emplace_back("SHOT DOWN: " + std::to_string(s.score.fighters) + " FIGHTERS, " +
                          std::to_string(s.score.bombers) + " BOMBERS",
                      "");
    rows.emplace_back("", "");
    rows.emplace_back("KILL POINTS", number(s.payout.points));
    if (s.payout.banked) {
        rows.emplace_back("PLANE CONDITION", percent(s.payout.condition_share));
        rows.emplace_back("WAVES FLOWN " + std::to_string(s.score.waves),
                          percent(s.payout.wave_share));
        rows.emplace_back(s.kept ? "BANKED" : "WOULD BANK", number(s.payout.total));
    } else {
        rows.emplace_back("NOT HOME: LOST", number(s.payout.points));
    }
    if (s.kept) {
        rows.emplace_back(s.nation == Nation::Germany ? "GERMAN POINTS" : "US POINTS",
                          number(s.points_now));
    }
    rows.emplace_back("", "");
    rows.emplace_back(s.kept ? "ENTER: HANGAR   R: FLY AGAIN   ESC: MENU"
                             : "ENTER OR R: NEW RUN   ESC: MENU",
                      "");

    constexpr float kW = 340.0f;
    const float height = static_cast<float>(rows.size()) * 10.0f + 8.0f;
    const float x = std::floor((static_cast<float>(view_w) - kW) * 0.5f);
    const float y = std::max(2.0f, std::floor((static_cast<float>(view_h) - height) * 0.5f));
    const SDL_FRect back{x, y, kW, height};
    SDL_SetRenderDrawColor(r, 0, 0, 0, 190);
    SDL_RenderFillRect(r, &back);
    float line = y + 5.0f;
    for (const auto& [label, value] : rows) {
        const bool total = label == "BANKED" || label == "WOULD BANK";
        const bool lost = label == "NOT HOME: LOST";
        const SDL_Color c = total ? kGood : lost ? kWarn : kTitle;
        if (value.empty()) {
            centred(r, view_w, line, label, c);
        } else {
            text(r, x + 60.0f, line, label, c);
            right(r, x + kW - 60.0f, line, value, c);
        }
        line += 10.0f;
    }
}

} // namespace wof::game
