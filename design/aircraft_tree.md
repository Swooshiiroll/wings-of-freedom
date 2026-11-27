# The aircraft tree

You start in an early fighter and earn better ones. Written from the owner's answers of
2026-11-27 (six question rounds), after the owner called the P-26A a testing stand-in.

Built in two pull requests: **the planes**, then **points, the hangar and upgrades**.
The bombers (`flight.md`, Bombers) come after both. Numbers marked **(start)** are tuning values.

## The tree

| Step | US | Germany |
|---|---|---|
| 1 (you start here) | Boeing P-12E (1931) | Arado Ar 65E (1933) |
| 2 | Boeing P-26A (1934) | Heinkel He 51B (1935) |
| 3 | Curtiss P-36A (1938) | Messerschmitt Bf 109B (1937) |

Their real stats, looks and sources are in `planes.md`.

- **The P-26A starts with two .30s.** Its .50 becomes an upgrade.
- A developer can fly any of them at once with `--plane`, without the hangar.

## Enemy fighters grow with the waves

The enemy's fighters come from its own tree, whatever you fly (`wave.tier_every`, 4 (start)):

| Waves | Enemy fighters |
|---|---|
| 1-4 | step 1 |
| 5-8 | steps 1 and 2, mixed |
| 9-12 | steps 2 and 3, mixed |
| 13 on | step 3 |

A starter is much slower than the types it meets from wave 5.

## Points

- **Kills:** fighters **100**, bombers **250**. Light and heavy types get their own values later.
- **Chains:** a kill within **20 s** of the last raises the multiplier by **0.5** (x1, x1.5, x2 …
  up to **x4**). 20 s without a kill resets it.
- **Banking:** points count **only if you get home** off the south edge. Otherwise that run's points
  are lost. Quitting or restarting loses them too. A plane going down isn't "home".
- **Home bonus**, added together:
  - up to **+50%** for your plane's condition;
  - **+10%** for each wave you were still flying in.
- **Separate** US and German points.
- **Condition** is the share of the airframe's HP left, times the share of the pilot unhurt.
- **Waves flown** are the waves that arrived while you were flying.
- The dashboard shows this run's points and the chain.
- **Prices (start):** a step 2 fighter **3,000** (2-3 good runs), step 3 **8,000**. Upgrades: guns
  and engine 800, fuel and bombs 500, armour 400, bomb sight 300.

## Upgrades

Per plane, bought once. **R** is a real fit of that type or a close variant, **F** a field mod
(pilots did refit their own planes).

| US | P-12E | P-26A | P-36A |
|---|---|---|---|
| Guns | .50 replaces a .30 (R) | .50 replaces a .30 (R) | two wing .30s, P-36C (R) |
| Engine | 600 hp, P-12F (R) | fuel-injected, P-26B, tuned (R) | 1,200 hp, P-36C (R) |
| Fuel | belly tank (F) | belly tank (F) | auxiliary tank (F) |
| Armour | seat plate (F) | seat plate (F) | plate and sealed tank, P-36G (R, est.) |
| Bombs | 4 × 116 lb (F) | 4 × 100 lb (F) | 2 × 100 lb wing racks (R, est.) |

| Germany | Ar 65E | He 51B | Bf 109B |
|---|---|---|---|
| Guns | third MG 17 (F) | third MG 17 (F) | two wing MG 17s, C-1 (R) |
| Engine | boosted BMW VI (F) | boosted BMW VI (F) | injected Jumo 210Ga, C-1 (R) |
| Fuel | belly tank (F) | 170 L tank, B-1 (R) | drop tank (F) |
| Armour | seat plate (F) | seat plate (F) | seat plate (F) |
| Bombs | 6 × 10 kg magazine, Ar 65a (R) | 4 × 50 kg, C-1 (R) | 4 × 50 kg belly rack (F) |

Every plane also has a **bomb sight**: the impact marker. Without it there is no marker (F4 stays
a developer switch).

**Effects (start):** engine about +6% speed and +10% climb; fuel +40-50% endurance; armour halves
damage to your pilot (the P-36's also halves fuel leaks). Tanks can't be dropped.

## Screens

- **Hangar**, after the nation: points, your tree (owned, or its price), the chosen plane's
  upgrades, **Fly**, **Reset progress** and **Back**. Keyboard and mouse; Esc goes back.
  - Planes are bought **in order**: the third needs the second.
  - Buying and resetting take **two presses**; moving away cancels.
  - **Reset progress** erases the nation shown, not the other.
  - Choosing a plane you own makes it the one you fly.
- **Run summary:** how it ended, time, kills, points and bonus, then **banked** or **lost**. Enter
  goes to the hangar; R flies again.
- The **dashboard strip** shows the wave, kills, this run's points and the chain. (The gun aim and
  auto-leveling words left it to make room; F3 shows the new aim for a moment.)
- Progress is saved in `progress.txt` next to the game, which the updater keeps. It's saved after
  every purchase and banked run. The developer options below never save.

## Developer options

`--plane ID` and `--upgrades LIST` fly at once; `--points N`, `--unlock-all` set up the hangar;
`--menu hangar` with `--shot` saves its screen.

## Not in this

Flying the bombers (the enemy raids are in `flight.md`), pilots and the campaign (v0.2), and later eras.
