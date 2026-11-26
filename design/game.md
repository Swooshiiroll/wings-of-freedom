# Wings of Freedom: game overview

The whole game in one place: what it is, the rules agreed so far, and what is still open. Written
from the owner's answers of 2026-11-26. Each system gets its own doc before it is built
(`flight.md` comes next, for v0.1). Numbers marked **(start)** are first guesses, set in the
live-reloaded tuning file and changed by playtest.

## What it is

A pixel-art air-combat game seen from outside the plane, in an isometric view of the ground below.
The style reference is the strike half of *Ultimate Air Combat* (NES, 1992). Nothing is copied
from it: all art, sound and code are original.

- Singleplayer first, co-op later.
- The first game mode is **Strike**: fly behind enemy lines, destroy targets, and get home.
- The player watches ammunition, fuel and damage the whole time.

## View and look

- **True isometric.** The ground grid is turned 45°.
  - North, toward enemy territory, is up-right on screen.
  - South, toward home base, is down-left.
- **Pixel art at 480x270**, scaled by whole numbers to the window (4x on a 1080p screen).
- **Planes are drawn at 32 headings** (one every 11.25°). Flight itself is smooth; only the
  sprite steps.
- **Height** shows as the gap between a plane and its shadow on the ground.

## Flying

- **Free flight** over a large scrolling map, with **full altitude control** (climb and dive
  freely).
- **Keyboard first**, every key rebindable in a controls menu. Gamepad comes later.
- **Default keys:**

  | Key | Action |
  |---|---|
  | A / D | Turn left / right |
  | W / S | Climb / dive |
  | Shift / Ctrl | Throttle up / down |
  | Space | Guns |
  | B | Bomb |

## Weapons

- The first plane carries **guns and bombs**, each with limited ammunition.
- **Hitting other planes at different heights** is a game option, with three rules:
  - **Forgiving band:** guns hit if the target is roughly at your height. The target shows whether
    it is above, level or below.
  - **Strict:** you must match its height closely.
  - **Auto-aim in height:** shots angle up or down toward a target ahead of you.
- **Bombs fall** from your height and speed. The **impact marker** (a crosshair where the bomb
  would land) is an **upgrade**; without it, you aim by your shadow.

## Damage

The plane has four parts that take hits. Each part is first **damaged**, then **destroyed**.

| Part | Damaged | Destroyed |
|---|---|---|
| Engine | Lower top speed and acceleration | You must try to get back to base |
| Wings | Wider turns | The wing breaks off and the plane crashes |
| Fuselage | Leaks fuel | All fuel is lost and you must try to get back to base; more hits take the tail off and the plane crashes |
| Pilot | 3 hits in all | The third hit kills the pilot and the plane crashes |

## Strike mode

### A round

1. The round marks its **objectives**: targets behind enemy lines. Enemies spawn in the air and on
   the ground, and the pressure rises as the campaign goes on.
2. Destroy the objectives.
3. Fly home off the **south edge** of the map. The round ends there.
4. Spend points on upgrades and planes. The next round is harder.

**Points** come from kills, with a bonus for **kill chains**. Other ways to earn them are decided
later.

### Base visits during a round

Flying off the south edge mid-round takes you to base. **The battle goes on** while you are there.

- Choose **one** service per visit:
  - **refuel**;
  - **repair** (the plane and the pilot);
  - **rearm**.
- Upgrades and planes you have bought are **equipped on the same visit**, as well as the chosen
  service.
- After each visit the **runway closes for a grace period**: you can't land again until it
  reopens.
- **SOS key, once per round:** it reopens the runway at once and calls a **fighter escort** to
  cover your way home. The escort takes a moment to arrive.

### Pilots and the campaign

- A campaign starts with **3 pilots**. More can be bought with points.
- When the plane goes down, you lose a pilot, keep your planes, and retry the round.
- Losing the last pilot ends the campaign.
- **The campaign is saved** between rounds: planes, upgrades, points, pilots and the round number.

## Eras and planes

- The planes progress **through eras**, from World War II propeller planes up to jets, bought
  between rounds.
- The first plane, and the whole first era, is World War II.

## Enemies

The first set, for v0.1:
- **Enemy fighters** that chase and shoot you;
- **Enemy bombers**, bigger and tougher, with defensive gunners;
- **Flak guns** on the ground, which threaten you depending on your height. They are also the
  first ground targets to bomb.

Trucks, tanks, depots and other objective targets arrive with Strike's objectives in v0.2.

## Multiplayer

- **Co-op**, with **friendly fire** on.
- Players connect by **direct IP** (LAN or VPN) or through a **hosted server**.
- **Steam** (invites, store page) only if the game is marketed later.
- Later: **one PC, one monitor per player**, each player with a full screen.

How the code is shaped for this from the start:
- The simulation (`src/core`) has no SDL and no I/O, so a host or a headless server can run it.
- Each plane is driven by a small **input command per tick**. Nothing assumes there is only one
  player.

## Platforms

- **Windows and Linux**, including the Steam Deck. CI builds both.
- **Android** is kept possible (no platform-specific code outside SDL), but isn't built yet.
- No Apple platforms. Consoles need licensed developer kits, so they are not planned.

## Milestones

| Milestone | Holds |
|---|---|
| v0.1 Fly and fight | One WWII fighter; flight with altitude, fuel, guns, bombs, the damage model; enemy fighters, bombers and flak; HUD; controls menu; the height-rule option. Crashing or flying home restarts. |
| v0.2 Strike loop | Objectives, base visits and services, runway grace period, SOS and escort, points, kill chains, the shop, pilots, saving |
| v0.3 WWII era and art | More WWII planes and enemies, real art replacing the placeholders, sound |
| v0.4 Co-op | Two or more players, friendly fire, direct IP and a hosted server |
| Backlog | Later eras, one-monitor-per-player, gamepad, Android, Steam |

## Open questions

1. **Screens that 270 doesn't divide.** 1440p is 5.33x and the Steam Deck (1280x800) is 2.96x.
   Proposed: use the largest whole-number scale that fits, and show more of the world in the
   space left over, with the HUD pinned to the edges.
2. **Lost engine or fuel.** Proposed: the plane glides, trading height for distance, so getting
   home depends on how high you were. To settle in `flight.md`.
3. **Default keys** above are my reading of "W/S A/D". Confirm or change.
4. **Runway grace period and service times** (start values), and whether each service takes time
   at base.
5. **The map:** its size, what the east, west and north edges do, terrain, and where the base sits.
6. **Eras and the plane roster:** which eras, how many planes each, and what jets add (missiles,
   radar?).
7. **Points:** values per kill, the kill-chain rule, and prices of upgrades, planes and pilots.
8. **One-monitor-per-player:** how players provide input (two keyboards, gamepads?).
9. **Hosted server:** who runs it, and whether it's the same program in a headless mode.
10. **The name:** other games already use "Wings of Freedom" in their titles. Fine for now; check
    it before any store release.
