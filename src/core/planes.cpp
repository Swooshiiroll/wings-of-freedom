#include "core/planes.hpp"

#include <algorithm>
#include <cstddef>

namespace wof {
namespace {

// The v0.1 roster (design/planes.md, with its sources). Figures marked (est.) there are
// estimates.
constexpr std::array<PlaneType, kPlaneTypes> kRoster = {{
    // Boeing P-26A "Peashooter", US fighter, 1934: two .30s (owner, 2026-11-27: the .50 is an
    // upgrade).
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
       {Calibre::Rifle, 1000.0f, 500, 1.0f, Mount::Fixed}}},
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
    // Boeing P-12E, US fighter, 1931 (biplane): two .30s. 244 lb of bombs.
    {"p12e",
     "Boeing P-12E",
     Nation::US,
     Role::Fighter,
     1931,
     189.0f,
     1900.0f,
     55.0f,
     12.0f,
     3.5f,
     60.0f,
     1,
     {{{Calibre::Rifle, 1000.0f, 600, -1.0f, Mount::Fixed},
       {Calibre::Rifle, 1000.0f, 600, 1.0f, Mount::Fixed}}},
     2,
     116.0f,
     2,
     40.0f,
     40.0f,
     60.0f,
     20.0f,
     ModelId::P12},
    // Arado Ar 65E, German fighter, 1933 (biplane): two 7.92 mm MG 17s, no bombs.
    {"ar65",
     "Arado Ar 65E",
     Nation::Germany,
     Role::Fighter,
     1933,
     186.0f,
     2086.0f,
     60.0f,
     13.0f,
     2.3f,
     60.0f,
     1,
     {{{Calibre::Rifle, 1000.0f, 500, -1.0f, Mount::Fixed},
       {Calibre::Rifle, 1000.0f, 500, 1.0f, Mount::Fixed}}},
     2,
     0.0f,
     0,
     40.0f,
     40.0f,
     60.0f,
     20.0f,
     ModelId::Ar65},
    // Curtiss P-36A Hawk, US fighter, 1938: one .30 and one .50, no bombs.
    {"p36a",
     "Curtiss P-36A",
     Nation::US,
     Role::Fighter,
     1938,
     313.0f,
     3400.0f,
     74.0f,
     16.0f,
     2.3f,
     60.0f,
     1,
     {{{Calibre::Rifle, 1000.0f, 500, -1.0f, Mount::Fixed},
       {Calibre::Heavy, 600.0f, 200, 1.0f, Mount::Fixed}}},
     2,
     0.0f,
     0,
     40.0f,
     40.0f,
     60.0f,
     20.0f,
     ModelId::P36},
    // Messerschmitt Bf 109B-1, German fighter, 1937: two 7.92 mm MG 17s, no bombs.
    {"bf109b",
     "Messerschmitt Bf 109B",
     Nation::Germany,
     Role::Fighter,
     1937,
     292.0f,
     2200.0f,
     75.0f,
     18.0f,
     1.5f,
     60.0f,
     1,
     {{{Calibre::Rifle, 1000.0f, 500, -1.0f, Mount::Fixed},
       {Calibre::Rifle, 1000.0f, 500, 1.0f, Mount::Fixed}}},
     2,
     0.0f,
     0,
     40.0f,
     40.0f,
     60.0f,
     20.0f,
     ModelId::Bf109},
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

FighterTree fighter_tree(Nation n) noexcept {
    return n == Nation::Germany ? FighterTree{PlaneId::Ar65E, PlaneId::He51B, PlaneId::Bf109B}
                                : FighterTree{PlaneId::P12E, PlaneId::P26A, PlaneId::P36A};
}

} // namespace wof
