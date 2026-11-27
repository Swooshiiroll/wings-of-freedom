# Real planes, nations and eras

Every plane in the game is a real aircraft, with its real stats kept in their real proportions.
Written from the owner's answers of 2026-11-26 (five question rounds). It replaces the single
invented fighter in `flight.md` with a roster.

Numbers marked **(est.)** are estimates where sources give nothing or conflict; the owner checks
them. Every game value is a **(start)** value in `tuning.txt`.

## How real: real readouts, scaled action

- **Each plane's stats come from the real aircraft**: top speed, climb, stall speed, endurance,
  turning, guns, rate of fire, ammunition and bomb load. A faster or tighter-turning plane really
  is faster or tighter-turning in the game, by the same ratio.
- **The HUD shows the real numbers:** mph, feet and rounds per minute.
- **Fixed scale factors keep the action arcade-paced.** Taken literally, the P-26 would need
  about 18 s for a full circle and 5 minutes to climb 12,000 ft.

### The scale

| What | Factor (start) | So that |
|---|---|---|
| Speed | **0.641 u/s per mph** (the readout is 1.56 mph per u/s) | the P-26A's real 234 mph is today's 150 u/s |
| Turning | the real sustained turn rate **× 4.44** | the P-26A's (est.) 16 s full circle takes today's 3.6 s |
| Climb | **0.00636 u/s per ft/min**, then a **third** of every climb and dive becomes height | the P-26A's real 2,360 ft/min is 15 u/s of climbing, 5 u/s of height: deck to 30,000 ft in about 2 minutes (owner, 2026-11-27) |
| Stall | the real stall speed, scaled like speed | |
| Dive limit | **1.6 ×** the plane's top speed | |
| Endurance | the P-26A's tank lasts **8 minutes** (480 s) at full throttle; others in proportion to their real endurance | doubled (owner, 2026-11-27), as climbing takes longer |
| Guns | the **real rounds per minute and ammunition**, in real time | |

- Gravity (90 u/s²) and the pitch rate stay game values. The pitch rate is 60°/s for fighters
  and 30°/s for bombers.
- The flight model's thrust and drag are worked out per plane so that it hits its scaled top
  speed and climb.

## Eras

| Era | Years | |
|---|---|---|
| Pre-war | 1930s | **v0.1** starts here |
| WWII | 1939-45 | |
| Korea | 1950-53 | the first jets |
| Vietnam | 1960s-70s | |
| Cold War | 1980s-90s | |

The campaign moves through them, each unlocking the next.

## Nations and sides

- **Six nations:** the US, the UK, Germany, Italy, Japan and the USSR.
- **You choose your country before a mission**, Axis or Allied, and fly its planes.
- **Each mission is against one nation of the other side.**
- **Sides follow history:**
  - **Pre-war and WWII:** the Allies (US, UK, USSR) against the Axis (Germany, Italy, Japan).
  - **Korea onwards:** the West (US, UK, with Germany, Italy and Japan flying Western types)
    against the East (USSR, and China later).
- **Maps by theatre:** the European farmland now, and a Pacific map later for Japan.
- **No national markings.** Each nation's planes have their own colour schemes, so friend and foe
  read apart.

## v0.1: the US against Germany, pre-war

You fly the US or Germany. The other nation is the enemy. Each nation's fighters form its
**aircraft tree** (`aircraft_tree.md`).

### The planes

| | Boeing P-26A | Heinkel He 51B | Martin B-10B | Heinkel He 111B-2 |
|---|---|---|---|---|
| Nation, role | US fighter | German fighter | US bomber | German bomber |
| In service | 1934 | 1935 | 1935 | 1937 |
| Top speed | 234 mph [1][2] | 205 mph at sea level [3] | 213 mph [5] | 230 mph at 13,000 ft [7] |
| Climb | 2,360 ft/min [4] | 2,340 ft/min (1,000 m in 1 min 24 s) [3] | 1,380 ft/min [5] | 1,000 ft/min (est.) |
| Stall | 70 mph (est.) | 62 mph (est.) | 65 mph (est.) | 75 mph (est.) |
| Full circle (real) | 16 s (est.) | 13 s (est., biplane) | 30 s (est.) | 34 s (est.) |
| Endurance | 1.8 h (360 mi) [2] | 2.0 h (est.) | 6.4 h (1,240 mi) [6] | 3 h (est.) |
| Engines | 1 | 1 | 2 | 2 |
| Guns | 2 × .30 cal, synchronised (owner, 2026-11-27; the .50 is an upgrade) | 2 × 7.92 mm MG 17, synchronised, 500 rounds each [3] | 3 × .30 cal: nose, dorsal, ventral [5][6] | 3 × 7.92 mm MG 15: nose, dorsal, ventral [7] |
| Rate and ammo | ~1,000 rounds/min each, 500 rounds each (est.) | ~1,000 rounds/min each (est.) | ~1,000 rounds/min each (est.); 500 rounds each (est.) | ~1,000 rounds/min each (est.); 750 rounds each (ten 75-round drums, est.) |
| Bombs | 2 × 100 lb (200 lb max [2]) | 6 × 10 kg [3] | 4 × 500 lb (2,260 lb max [5]) | 6 × 250 kg (1,500 kg max [7]) |
| Crew | pilot | pilot | pilot, 3 gunners (the B-10B's crew of 4) | pilot, 3 gunners |
| Colours | olive drab, chrome yellow wings and tail | light grey | true blue fuselage, chrome yellow wings | dark green and brown splinter, pale blue underneath |

Notes:
- **P-26A climb:** some sources give "719 ft/min", which is really **719 m/min (= 2,360 ft/min)**.
- **He 111B-2 speed:** sources range from 230 to 242 mph. The 242 mph figure is the later
  E-series [7].
- **P-26A guns:** two .30s (owner, 2026-11-27), as P-26As carried; the other real fit, one .30 and
  one .50 (600 rounds/min and 200 rounds [13], estimates), is its gun upgrade.

### The tree's other fighters

| | Boeing P-12E | Arado Ar 65E | Curtiss P-36A | Messerschmitt Bf 109B-1 |
|---|---|---|---|---|
| Nation, tree step | US, 1 | Germany, 1 | US, 3 | Germany, 3 |
| In service | 1931 | 1933 | 1938 | 1937 |
| Top speed | 189 mph at 7,000 ft [14][15] | 186 mph [16][17] | 313 mph at 10,000 ft [18] | 292 mph at 13,000 ft [19][20] |
| Climb | 1,900 ft/min (est. from 10,000 ft in 5.8 min [15]) | 2,086 ft/min [16][17] | 3,400 ft/min (est.) [21] | 2,200 ft/min [20] |
| Stall | 55 mph (est.) | 60 mph (est.) | 74 mph (est.) | 75 mph (est.) |
| Full circle (real) | 12 s (est., biplane) | 13 s (est., biplane) | 16 s (est.) | 18 s (est.) |
| Endurance | 3.5 h (570 mi) [14] | 2.3 h (350 mi) [16] | 2.3 h (est.) | 1.5 h (est.) |
| Guns | 2 × .30 cal, 600 rounds each [14] | 2 × 7.92 mm MG 17, 500 each [16][17] | 1 × .50 (200, est.) + 1 × .30 (500, est.) [18] | 2 × 7.92 mm MG 17, 500 each (est.) [19][22] |
| Bombs | 2 × 116 lb (244 lb [14]) | none (its 10 kg magazine was dropped from the 65E [16]) | none [18] | none |
| Colours | olive drab, chrome yellow wings and tail | grey-green | bare metal, yellow wings and tail | dark grey-green over pale blue |

Their game values:

| | P-12E | Ar 65E | P-36A | Bf 109B |
|---|---|---|---|---|
| Top speed (u/s) | 121 | 119 | 201 | 187 |
| Best climb (u/s) | 12.1 | 13.3 | 21.6 | 14.0 |
| Stall (u/s) | 35 | 38 | 47 | 48 |
| Turn rate at best turn speed | 133°/s | 123°/s | 100°/s | 89°/s |
| Tank at full throttle | 933 s | 613 s | 613 s | 400 s |

### Their game values (from the scale)

| | P-26A | He 51B | B-10B | He 111B-2 |
|---|---|---|---|---|
| Top speed (u/s) | 150 | 131 | 137 | 147 |
| Best climb (u/s) | 15.0 | 14.9 | 8.8 | 6.4 |
| Stall (u/s) | 45 | 40 | 42 | 48 |
| Turn rate at best turn speed | 100°/s | 123°/s | 53°/s | 47°/s |
| Best turn speed (0.73 × top) | 110 | 96 | 100 | 108 |
| Dive limit (u/s) | 240 | 210 | 218 | 236 |
| Tank at full throttle | 240 s | 267 s | 853 s | 400 s |
| Pitch rate | 60°/s | 60°/s | 30°/s | 30°/s |

### Damage (`flight.md`, Damage, per plane)

- **Fighters:** engine 40, each wing 40, fuselage 60, tail 20 HP. The pilot has 3 wounds of 5
  damage each, as today.
- **Bombers:**
  - **two engines, 60 HP each.** Losing one roughly halves the power (the twin-engine rule). Each
    smokes on its own.
  - each wing 100, fuselage 160 and tail 40 HP;
  - each of the 3 gunners has 3 wounds, like a pilot.
- **Hit split:** the engine share of the hit table splits between a twin's two engines.

### Guns by calibre

| Calibre | Damage per hit | Round speed (game) | Life |
|---|---|---|---|
| Rifle calibre (.30 cal, 7.62, 7.7, 7.92 mm) | 1 | 360 u/s + the plane's | 0.8 s |
| Heavy machine gun (.50 cal, 12.7 mm) | 2.5 | 380 u/s + the plane's | 0.9 s |
| 20 mm cannon | 8 | 340 u/s + the plane's | 0.8 s |

- **Round speed and range stay game values,** because real muzzle velocities would make rounds
  almost instant.
- **Each gun keeps its real rate and ammo.**
- **Tracers:** every 3rd round per gun is a tracer.
- **The readout** shows rounds left per gun type.
- **How fast that kills:**
  - **The He 51's** two rifle-calibre guns fire about 33 rounds and 33 damage a second. From
    ahead, a fighter's engine fails after about 90 hits (about 3 s on target).
  - **A .30 and a .50** (the P-36A, or an upgraded P-26A) fire about 27 rounds but 42 damage a
    second. That's about 2 s on target from ahead.

### Bombs by weight

Blast radius and damage scale with the cube root of the bomb's weight. A 100 lb bomb is today's
20 u blast with 40 damage at the centre.

| Bomb | Blast radius | Damage |
|---|---|---|
| 10 kg (22 lb) | 12 u | 24 |
| 100 lb | 20 u | 40 |
| 500 lb | 34 u | 68 |
| 250 kg (551 lb) | 35 u | 71 |

### Flak: real light and heavy guns (built in PR 6)

| | Rate | Reaches | Hit |
|---|---|---|---|
| **German light:** 2 cm Flak 30 | 120 rounds/min practical [8] | ~3,500 ft effective [9] | 20 mm shells: 8 per hit, aimed tracer streams |
| **German heavy:** 8.8 cm Flak 18 | 15-20 rounds/min [10] | 26,000 ft effective [10] | timed bursts: 16 u, 14 damage, scatter growing with height |
| **US light:** 37 mm M1 | 120 rounds/min max, ~90 practical [11] | ~10,500 ft (est.; sources conflict) | 37 mm shells: 14 per hit, aimed streams |
| **US heavy:** 3-inch M3 | ~25 rounds/min (est.) | ~28,000 ft [12] | timed bursts: 15 u, 13 damage |

- **Light flak** fires aimed streams like guns: deadly low, and useless above its reach.
- **Heavy flak** fires timed bursts that scatter more the higher you are (`flight.md`'s rule).

## Height: 30,000 ft for every plane

- **Ceiling 30,000 ft** for every plane (the owner chose one fixed ceiling). That's 600 u at
  50 ft per u.
- **The sky is tall to climb** (owner, 2026-11-27: "too quick to climb to that altitude"): a third
  of every climb and dive becomes height (`flight.height_share`). The P-26A and He 51 take about
  2 minutes from the deck to the top, the bombers 3.5 to 4.5. A full-speed pull-up gains about
  2,000 ft, and a full dive from the top takes about 12 s. Speeds, turning, aim bands and the
  altimeter are unchanged; bombs leave with the plane's real up or down speed, and guns still fire
  where the nose points.
- **Depth** (owner, 2026-11-27): everything in the air is sized by how far below or above your
  plane it is, like perspective. Your height is full size; below you things shrink, down to the
  ground's size; above you they grow, up to **1.5×** at a full ceiling above.
  - **From 30,000 ft the ground is drawn a quarter size** (`look.zoom_at_ceiling`), so you see
    much more of the map.
  - **Planes shrink and grow in crisp steps** of an eighth (a quarter to 1.5×), each baked as
    pixel art. The zoomed ground is drawn smooth, the one exception to pixel-perfect.
  - **Speed is the same at every height** (owner): the slower-looking ground up high is
    perspective.
- **The shadow gap** grows with height to about 130 px at 30,000 ft. The camera aims 0.6 of the way
  up from the shadow to the plane, so the plane rides higher on screen as you climb. Up high the
  shadow also sits a little toward the screen's centre (perspective, as the camera looks ahead).

## The later roster (names only; an open list)

| Era | US | UK | Germany | Italy | Japan | USSR |
|---|---|---|---|---|---|---|
| Pre-war | P-12E, P-26A, P-36A, B-10B, B-17B | Gladiator, Hurricane I, Blenheim I | Ar 65, He 51, Bf 109B/D, He 111B, Do 17 | CR.32, CR.42, SM.79 | Ki-27, A5M, G3M, Ki-21 | I-15, I-16, SB |
| WWII | P-40, P-38, P-47, P-51, F4F, F6F, F4U, B-25, B-17, B-24 | Spitfire, Typhoon, Mosquito, Lancaster | Bf 109E-K, Fw 190, Bf 110, Ju 87, Ju 88, Me 262 | C.202, C.205 | A6M, Ki-43, Ki-84, N1K, G4M | Yak-9, La-5, Il-2, Pe-2 |
| Korea | F-86, F-80, F9F, B-29 | Meteor | (West) | (West) | (West) | MiG-15, Il-28 |
| Vietnam | F-4, F-105, F-8, A-4 | | | | | MiG-17, MiG-21 |
| Cold War | F-15, F-16, F/A-18, A-10 | Tornado, Harrier | Tornado | Tornado | F-15J | MiG-23, MiG-29, Su-27 |

## Open questions

1. **The (est.) figures** in the v0.1 table: check them, or point to better sources.
2. ~~P-26A armament~~: two .30s, the .50 an upgrade (owner, 2026-11-27).
3. **The scale factors** (speed 0.641, turn 4.44, climb 0.00636) are start values for
   playtesting.
4. **The Pacific map** for Japan: in v0.3 with the other four nations, or later?
5. **Jets** (Korea onwards) are 2-3 times faster than these planes. The map, the scale or both will
   need to grow for those eras.

## Sources

1. [National Museum of the USAF: Boeing P-26A](https://www.nationalmuseum.af.mil/Visit/Museum-Exhibits/Fact-Sheets/Display/Article/198089/boeing-p-26a/)
2. [Smithsonian NASM: Boeing P-26A Peashooter](https://si.edu/object/nasm_A19730273000)
3. [Wikipedia: Heinkel He 51](https://en.wikipedia.org/wiki/Heinkel_He_51)
4. [HistoryOfWar: Boeing P-26](https://www.historyofwar.org/articles/weapons_boeing_P-26.html); [Planes of Fame: P-26A](https://planesoffame.org/aircraft/plane-P-26A) (as m/min)
5. [Wikipedia: Martin B-10](https://en.wikipedia.org/wiki/Martin_B-10)
6. [National Museum of the USAF: Martin B-10](https://www.nationalmuseum.af.mil/Visit/MuseumExhibits/FactSheets/Display/tabid/509/Article/197393/martin-b-10.aspx); [HistoryOfWar: Martin B-10](https://www.historyofwar.org/articles/weapons_martin_B-10.html)
7. [HistoryOfWar: Heinkel He 111B](https://www.historyofwar.org/articles/weapons_he111B.html)
8. [Wikipedia: 2 cm Flak 30, Flak 38 and Flakvierling 38](https://en.wikipedia.org/wiki/2_cm_Flak_30,_Flak_38_and_Flakvierling_38)
9. [Lone Sentry: TM-E 30-451, German antiaircraft guns](https://lonesentry.com/manuals/german_aa/gaa7_german_antiaircraft_aa_guns.html)
10. [Wikipedia: 8.8 cm Flak 18/36/37/41](https://en.wikipedia.org/wiki/8.8_cm_Flak_18/36/37/41)
11. [Wikipedia: 37 mm gun M1](https://en.wikipedia.org/wiki/37_mm_gun_M1)
12. [Wikipedia: 3-inch anti-aircraft gun M3](https://en.wikipedia.org/wiki/3-inch_anti-aircraft_gun_M3); [WW2DB](https://ww2db.com/weapon.php?q=a161)
13. [War Thunder wiki: P-26A-34 M2](https://wiki.warthunder.com/P-26A-34_M2) (the .50's 200 rounds; a game reference, so treated as an estimate)
14. [Wikipedia: Boeing P-12](https://en.wikipedia.org/wiki/Boeing_P-12)
15. [HistoryOfWar: Boeing P-12](https://www.historyofwar.org/articles/weapons_boeing_P-12.html)
16. [Wikipedia: Arado Ar 65](https://en.wikipedia.org/wiki/Arado_Ar_65)
17. [Warbirds Resource Group: Ar 65](https://warbirdsresourcegroup.org/LRG/ar65.html)
18. [HistoryOfWar: Curtiss P-36A](https://www.historyofwar.org/articles/weapons_curtiss_P-36A.html)
19. [HistoryOfWar: Bf 109B](https://www.historyofwar.org/articles/weapons_bf_109B.html)
20. [Warbirds Resource Group: Bf 109B](https://warbirdsresourcegroup.org/LRG/luftwaffe_messerschmitt_bf109b_specifications.html)
21. [Wikipedia: Curtiss P-36](https://en.wikipedia.org/wiki/Curtiss_P-36_Hawk)
22. [HistoryOfWar: Bf 109C](https://www.historyofwar.org/articles/weapons_bf_109C.html)
