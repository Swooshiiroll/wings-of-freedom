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

- **Isometric look, north up** (owner, 2026-11-27). The ground grid is turned 45°.
  - North, toward enemy territory, is straight up the screen.
  - South, toward home base, is straight down.
- **Pixel art at 480x270**, scaled by whole numbers to the window (4x on a 1080p screen). On
  screens that 270 doesn't divide evenly (1440p, Steam Deck), the largest whole-number scale is used
  and the space left over shows more of the world. The HUD stays pinned to the edges.
- **Planes are drawn at 32 headings** (one every 11.25°). Flight itself is smooth; only the
  sprite steps.
- **Height** shows as the gap between a plane and its shadow on the ground. Every plane can climb
  to **30,000 ft** (about 2 minutes of climbing for a fighter). Everything is sized by how far
  below or above you it is, down to a **quarter size** for the ground from the top (owner,
  2026-11-27).

## Flying

- **Free flight** over a large scrolling map, with **full altitude control** (climb and dive
  freely).
- **Light physics:** climbing costs speed and diving gains it, and a plane that gets too slow stalls
  and drops its nose.
- **No power** (engine destroyed or fuel gone): the plane glides, trading height for distance.
- **Collisions:**
  - touching the ground is a crash, except on a runway (later);
  - when two planes collide, the one with the lower total HP is destroyed and the other is damaged.
- **Keyboard first**, every key rebindable in a controls menu. Gamepad comes later.
- **Default keys:**

  | Key | Action |
  |---|---|
  | A / D | Turn left / right |
  | W / S | Dive / climb |
  | Shift / Ctrl | Throttle up / down |
  | F | Guns |
  | Space | Release the selected heavy weapon (bombs, rockets, missiles) |
  | Tab | Switch heavy weapon |

The details for v0.1 are in `flight.md`.

## Weapons

- Each plane carries **its real guns and bombs**, with their real rate of fire and ammunition.
  Damage depends on calibre: rifle-calibre machine guns are light, .50 cal heavier, and cannon
  much heavier (`planes.md`).
- **Hitting other planes at different heights** is a game option, with three rules:
  - **Forgiving band** (the default): guns hit if the target is roughly at your height.
  - **Strict:** you must match its height closely.
  - **Auto-aim in height:** shots angle up or down toward a target ahead of you.
- **You judge other planes' heights by their shadows only.** There are no height markers.
- **Bombs fall** from your height and speed. The **impact marker** (a crosshair where the bomb
  would land) is an **upgrade**; without it, you aim by your shadow.

## Damage

The plane has four parts that take hits: engine, wings, fuselage and pilot.
- A hit lands on the part facing the fire: from ahead, mostly the engine and pilot; from behind,
  the fuselage; from a side, that wing.
- **The effect grows gradually** as a part's HP falls.
- **The HUD shows stages:** OK, WARNING, DANGER, CRITICAL, then destroyed.

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

## Real planes, nations and eras

Every plane is a **real aircraft** (owner, 2026-11-26), with its real stats kept in proportion. The
full design, with sources, is in `planes.md`.
- **Real readouts, scaled action:** the HUD shows the real mph, feet and rounds per minute, and
  fixed scale factors keep the dogfights arcade-paced.
- **Eras, 1930s to 1990s:** pre-war, WWII, Korea, Vietnam and the Cold War. The campaign moves
  through them.
- **Six nations:** the US, the UK, Germany, Italy, Japan and the USSR.
  - **You choose your country (Axis or Allied) before a mission** and fly its planes.
  - Each mission is against **one nation of the other side**.
  - Sides follow history: Allies against the Axis through WWII, then West against East.
- **Maps by theatre:** the European farmland first, and a Pacific map for Japan.
- **v0.1 is the pre-war US against Germany:**
  - US: the P-12E, P-26A and P-36A fighters and the Martin B-10B bomber;
  - Germany: the Ar 65, He 51 and Bf 109B fighters and the He 111B bomber.
- **The aircraft tree** (owner, 2026-11-27): you start in the first fighter and earn the next with
  points you bring home (`aircraft_tree.md`).

## Enemies

The first set, for v0.1, all real types of the enemy nation:
- **Enemy fighters** that chase and shoot you;
- **Enemy bombers**, bigger and tougher, with nose, dorsal and ventral gunners;
- **Flak guns** on the ground, real light and heavy types. Light flak is deadly low and can't reach
  high. Heavy flak reaches most of the way up and is less accurate the higher you fly. They are
  also the first ground targets to bomb.

Enemy bombers head south to bomb your base, so you have to intercept them.

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
| v0.1 Fly and fight | The pre-war US and Germany with real stats; the aircraft tree (three fighters each) with points, hangar and upgrades; B-10B and He 111B bombers; flight to 30,000 ft, fuel, guns by calibre, bombs, the damage model; enemy fighters, bombers and real flak; HUD; controls menu; the height-rule option. Crashing or flying home restarts. |
| v0.2 Strike loop | Objectives, base visits and services, runway grace period, SOS and escort, pilots and the campaign |
| v0.3 Pre-war nations and art | The UK, Italy, Japan and the USSR's pre-war planes, the Pacific map, real art replacing the placeholders, sound |
| v0.4 Co-op | Two or more players, friendly fire, direct IP and a hosted server |
| Backlog | WWII, Korea, Vietnam and Cold War eras; one-monitor-per-player, gamepad, Android, Steam |

## Open questions

1. **Runway grace period and service times** (start values), and whether each service takes time
   at base.
2. **The Strike map:** terrain, objectives, and where the base sits (v0.1's test field is in
   `flight.md`).
3. **The plane roster beyond v0.1** (an open list in `planes.md`), and what jets add (missiles,
   radar?). Jets are 2-3 times faster, so the maps or the scale will need to grow for them.
4. **Points:** values per kill, the kill-chain rule, and prices of upgrades, planes and pilots.
5. **What a bomber reaching base costs** in v0.2.
6. **One-monitor-per-player:** how players provide input (two keyboards, gamepads?).
7. **Hosted server:** who runs it, and whether it's the same program in a headless mode.
8. **The name:** other games already use "Wings of Freedom" in their titles. Fine for now; check
   it before any store release.
