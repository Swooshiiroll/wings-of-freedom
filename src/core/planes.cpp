#include "core/planes.hpp"

#include <algorithm>
#include <cstddef>

namespace wof {
namespace {

// The v0.1 roster (design/planes.md, with its sources). Figures marked (est.) there are
// estimates.
constexpr std::array<PlaneType, kPlaneTypes> kRoster = {{
    // Boeing P-26A "Peashooter", US fighter, 1934: one .30 and one .50 (owner, 2026-11-26).
    {"p26a",
     "Boeing P-26A",
     Nation::US,
     Role::Fighter,
     1934,
     234.0f,
     2360.0f,
     70.0f,
     16.0f,
     1.8f,
     60.0f,
     1,
     {{{Calibre::Rifle, 1000.0f, 500, -1.0f, Mount::Fixed},
       {Calibre::Heavy, 600.0f, 200, 1.0f, Mount::Fixed}}},
     2,
     100.0f,
     2,
     40.0f,
     40.0f,
     60.0f,
     20.0f,
     ModelId::P26},
    // Heinkel He 51B, German fighter, 1935 (biplane): two 7.92 mm MG 17s.
    {"he51b",
     "Heinkel He 51B",
     Nation::Germany,
     Role::Fighter,
     1935,
     205.0f,
     2340.0f,
     62.0f,
     13.0f,
     2.0f,
     60.0f,
     1,
     {{{Calibre::Rifle, 1000.0f, 500, -1.0f, Mount::Fixed},
       {Calibre::Rifle, 1000.0f, 500, 1.0f, Mount::Fixed}}},
     2,
     22.05f, // 10 kg
     6,
     40.0f,
     40.0f,
     60.0f,
     20.0f,
     ModelId::He51},
    // Martin B-10B, US bomber, 1935: three .30s (nose, dorsal, ventral). Its model comes in PR 6.
    {"b10b",
     "Martin B-10B",
     Nation::US,
     Role::Bomber,
     1935,
     213.0f,
     1380.0f,
     65.0f,
     30.0f,
     6.4f,
     30.0f,
     2,
     {{{Calibre::Rifle, 1000.0f, 500, 0.0f, Mount::Nose},
       {Calibre::Rifle, 1000.0f, 500, 0.0f, Mount::Dorsal},
       {Calibre::Rifle, 1000.0f, 500, 0.0f, Mount::Ventral}}},
     3,
     500.0f,
     4,
     60.0f,
     100.0f,
     160.0f,
     40.0f,
     ModelId::P26},
    // Heinkel He 111B-2, German bomber, 1937: three 7.92 mm MG 15s. Its model comes in PR 6.
    {"he111b",
     "Heinkel He 111B-2",
     Nation::Germany,
     Role::Bomber,
     1937,
     230.0f,
     1000.0f,
     75.0f,
     34.0f,
     3.0f,
     30.0f,
     2,
     {{{Calibre::Rifle, 1000.0f, 750, 0.0f, Mount::Nose},
       {Calibre::Rifle, 1000.0f, 750, 0.0f, Mount::Dorsal},
       {Calibre::Rifle, 1000.0f, 750, 0.0f, Mount::Ventral}}},
     3,
     551.0f, // 250 kg
     6,
     60.0f,
     100.0f,
     160.0f,
     40.0f,
     ModelId::P26},
}};

} // namespace

const char* nation_name(Nation n) noexcept {
    switch (n) {
    case Nation::US:
        return "United States";
    case Nation::UK:
        return "United Kingdom";
    case Nation::Germany:
        return "Germany";
    case Nation::Italy:
        return "Italy";
    case Nation::Japan:
        return "Japan";
    case Nation::USSR:
        return "Soviet Union";
    }
    return "?";
}

const char* calibre_name(Calibre c) noexcept {
    switch (c) {
    case Calibre::Rifle:
        return ".30";
    case Calibre::Heavy:
        return ".50";
    case Calibre::Cannon20:
        return "20mm";
    }
    return "?";
}

const PlaneType& plane_type(PlaneId id) noexcept {
    return kRoster[static_cast<std::size_t>(id)];
}

const PlaneType& plane_type(int index) noexcept {
    return kRoster[static_cast<std::size_t>(std::clamp(index, 0, kPlaneTypes - 1))];
}

bool find_plane(std::string_view id, PlaneId& out) noexcept {
    for (int i = 0; i < kPlaneTypes; ++i) {
        if (kRoster[static_cast<std::size_t>(i)].id == id) {
            out = static_cast<PlaneId>(i);
            return true;
        }
    }
    return false;
}

PlaneId fighter_of(Nation n) noexcept {
    return n == Nation::Germany ? PlaneId::He51B : PlaneId::P26A;
}

} // namespace wof
