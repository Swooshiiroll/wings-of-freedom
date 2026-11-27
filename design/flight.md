# Fly and fight (v0.1)

The spec for the first playable build. You fly a real pre-war fighter (the US's Boeing P-26A or
Germany's Heinkel He 51) with full altitude control, burning fuel. You fight the other nation's
fighters, bombers and flak with guns and bombs, and take damage part by part. Written from the
owner's answers of 2026-11-26 (flight rounds 1-5, the first draft's open questions, and the
real-planes rounds).
The whole game is in `game.md`; the planes, their real stats and the scale are in `planes.md`.

**Real planes (2026-11-26):**
- Flight, gun and bomb numbers are now **per plane**: each plane's real stats, scaled as
  `planes.md` sets out.
- Where this doc gives single numbers (speeds, turn rate, climb, guns), they're the **P-26A's**
  game values.
- The sim works in units (u). The HUD shows real mph and feet.

Every number here is a **(start)** value. They all live in `tuning.txt`, which the game reloads
while it runs, and are changed by playtest, not settled here.

## What v0.1 is for

Show that flying, shooting, bombing and getting hurt feel right before anything is built on top.
A run is one flight. It ends when you are shot down, crash, or fly off the south edge, and then
a new run starts.

**In:** the player's fighter, light flight physics, fuel, guns, bombs, the full damage model,
mid-air collisions, enemy fighters, bombers and flak, escalating waves, the HUD, the controls
menu, the gunnery height option, the run summary.

**Out (later milestones):** objectives, base visits, runway, SOS and escort, points, kill chains,
shop, pilots, saving, other planes and eras, trucks and tanks, sound, gamepad, co-op.

## World and view

- **Units:** distances in world units (u), time in seconds.
- **Projection:** 2:1 true isometric at 480x270. North points up-right on screen and south (home)
  down-left. One unit of ground is about one art pixel.
- **Height:** 0 is the ground. It is drawn as the gap between a plane and its shadow, **0.4 px per
  u**, sized by depth (design/planes.md, Height).
  - The ceiling is **30,000 ft (600 u) for every plane** (owner, 2026-11-26).
  - **Everything is sized by its depth** from your plane: full size at your height, down to a
    quarter for the ground from 30,000 ft, up to 1.5× above you. Planes change size in crisp
    eighth steps; the ground is drawn smooth (the one exception to pixel-perfect).
  - **The shadow gap** grows to about 130 px at 30,000 ft.
- **Map:** **2880 u east-west by 4800 u north-south** (about 6 screens across and 10 deep). The
  ground is flat in v0.1, with drawn detail only.
- **The ground** is a **farmland patchwork** (placeholder look until v0.3's art).
  - Rectangular fields of wheat, barley, ploughed earth, pasture and meadow, plus woods.
  - Hedgerows between fields, dirt roads on a grid, and one river winding north to south.
  - It's made from the seed, the same on every computer.
- **Edges:**
  - **Nothing on the ground marks them.** The farmland carries on past every edge, and only the
    HUD's warning and countdown show where the battle area ends.
  - East, west and north are soft. Within **200 u** of one, a warning shows.
  - Past one of them, a **10 s countdown** starts: "Return to the battle area". Flying back inside
    stops it, and it starts again from 10 s the next time you leave. At 0 the plane
    **self-destructs**, and the run ends as "left the battle area".
  - South is the way home. Leaving the south edge ends the run as "returned to base".
  - Enemy AI keeps its planes inside the map, except bombers, which leave by the south edge.
- **Camera:** it follows the player, centred between the plane and its shadow, and leads a little
  in the direction of flight.
- **Screens that 270 doesn't divide evenly:** the largest whole-number scale is used. The space
  left over shows more of the world, and the HUD stays pinned to the screen edges.
- **The window** opens at the largest whole multiple of 480x270 that fits the screen (1440x810 on
  a 1080p screen). **F11** toggles fullscreen.

## Controls

Keyboard first. Every action can be rebound in the controls menu, and bindings are saved.

| Action | Default key |
|---|---|
| Turn left / right | A / D |
| Dive / climb | W / S |
| Throttle up / down | Shift / Ctrl |
| Fire guns | F |
| Release the selected heavy weapon (bombs; rockets and missiles on later planes) | Space |
| Switch heavy weapon | Tab |
| Pause menu | Esc |

The front end turns keys into one **input command per tick** for the player's plane: turn, pitch,
throttle change, fire guns, release heavy weapon, switch heavy weapon. AI planes produce the same commands, so every plane flies by the
same rules.

## Flight (light physics)

The simulation runs at a fixed **60 ticks per second**.

- **Speed:** thrust from the throttle, minus drag, minus the pull of gravity along the climb.
  - Climbing costs speed and diving gains it. The strength of that pull is **g = 90 u/s²**, which
    also drops the bombs.
  - **Max level speed** is **150 u/s** at full throttle. **Dive limit** is **240 u/s**; the plane
    can't go faster than that.
  - **How it's worked out** (so all of these numbers hold together):
    - Drag is a parasitic part that grows with speed squared, plus an induced part that shrinks
      with it. The best glide below fixes both.
    - The propeller's thrust is full up to **110 u/s**, then falls as 1/speed, scaled by the
      throttle. Its power is set so that full throttle levels off at 150.
- **Pitch:** W/S changes it at **60°/s** (bombers 30°/s), up to **±40°**. **Let go and the nose
  eases back to level** at the same rate (owner, 2026-11-26). Height changes at speed × sin(pitch)
  × **1/3** (`flight.height_share`; owner, 2026-11-27: climbing should take a while), climbing and
  diving alike. Speed trades with the climb angle as before.
  - **Best sustained climb:** each plane's real climb rate, scaled. For the P-26A that's **15 u/s**
    of climbing (2,360 ft/min real), a third of it height, so deck to 30,000 ft takes about
    2 minutes. Its thrust is worked out to hit both its top speed and this climb.
  - Holding S climbs at up to 40° and loses speed fast: from 110 u/s to a stall in about 2 s.
- **Turning:** **100°/s** at the best turn speed of **110 u/s**.
  - Below that speed the rate falls with speed (50°/s at 55); above it, with 1/speed (73°/s at
    150).
  - At best speed, a full circle takes about 3.6 s.
- **Bank:** the plane banks **30°** into a turn and levels its wings after. It's for looks only.
- **Throttle:** 0-100% in steps while Shift/Ctrl is held, at **50% per second**.
- **Stall:** below each plane's scaled real stall speed (the P-26A's **45 u/s**, about 70 mph)
  until speed is 8 u/s above it again. Height is the safety margin.
  - The nose drops on its own toward **-30°**, at **40°/s**.
  - Turn and pitch control work at **30%**.
- **Ceiling:** above **30,000 ft (600 u)** the plane can't climb any further, and the nose is
  pushed back to level.
- **A run starts** in the air near the south edge, heading north at **120 u** and **110 u/s**,
  with **80%** throttle.
- **The ground:** touching it is a crash, and the run ends.

### Fuel

- A full tank lasts **8 minutes** (480 s) at full throttle in the P-26A, others in proportion to
  their real endurance. Burn scales with throttle, down to **30%** at idle. A climb to 30,000 ft
  costs about a quarter of it.
- **Engine off** (fuel empty or engine destroyed): no thrust, so the plane **glides**. Best glide
  is about **6 u forward per 1 u of height**, at **90 u/s**. Whether you get home depends on how
  high you were.

## Weapons

- **Guns** fire with **F**, at any time.
- **Heavy weapons** (bombs, rockets, missiles) are released with **Space**. **Tab** cycles through
  the heavy weapons the plane carries.
- The first fighter carries only bombs, so Tab has nothing to cycle in v0.1.
- The HUD shows the selected heavy weapon and what's left of each weapon.

### Guns

- Fixed, firing forward along the nose, pitch included.
- **Each plane carries its real guns** (owner, 2026-11-26), with their real rate of fire and ammo.
  Damage depends on calibre (`planes.md`): rifle calibre 1, .50 cal 2.5, 20 mm 8.
- **The P-26A:** one **.30 cal** and one **.50 cal** in the cowling (owner, 2026-11-26), 1 u
  either side of the nose.
  - The .30 fires about 1,000 rounds/min with 500 rounds: about 30 s of fire.
  - The .50 fires about 600 rounds/min with 200 rounds: about 20 s of fire, at 2.5 damage a hit.
  - Together that's about 27 rounds and 42 damage a second.
- **The He 51:** two 7.92 mm MG 17s, the same way.
- Rifle-calibre rounds leave at **360 u/s** plus the plane's speed and last **0.8 s**. That's a
  game value, not the real muzzle velocity.
- *(PR 3-4 had 12 rounds/s doing 2 damage each. Real guns replace that, with damage by calibre in
  its place.)*
- **Every 3rd round is a tracer** (owner, 2026-11-26), drawn as a short streak. The others are
  invisible but hit all the same.
- **Hitting a plane:** a round's path passes within **8 u** of it on the ground, and the round is
  within the height rule's band there. Hits are tested along the whole path each tick, so fast
  rounds can't skip through.
- **Strafing:** rounds that reach the ground hit ground targets within **6 u** of their edge.

### The height rule for gunnery (options menu)

| Rule | A round hits a plane when | Default |
|---|---|---|
| Forgiving band | Within **±10 u** of its height | Yes |
| Strict | Within **±3 u** of its height | |
| Auto-aim in height | The target is within **±12°** of the nose and **±40 u** of height; rounds angle toward it | |

You judge other planes' heights from their **shadows only**. There are no height markers.

Under auto-aim, a round's climb is set when it's fired, toward the nearest plane or drone in the
cone. Its hit is then tested with the forgiving band. Until PR 7's options menu, **F3** cycles the
rule (or `--height-rule forgiving|strict|auto`).

### Bombs

- **Each plane carries its real bombs** (`planes.md`): the P-26A **2 × 100 lb**, the He 51
  **6 × 10 kg**. The blast scales with the bomb's weight (a 100 lb bomb is the 20 u / 40 damage
  below).
- Each bomb falls from the plane's height and carries its speed, under g. From 120 u at 150 u/s,
  a bomb lands about 245 u ahead.
- **Hold Space to ripple** (owner, 2026-11-26). One bomb drops at once, then one every **0.3 s**
  until you let go or run out. Tapping faster than that doesn't drop two at once.
- Each burst leaves a **scorch mark that fades over 30 s** (owner).
- **Blast radius 20 u**, **40 damage** at the centre, falling to 0 at the edge. The blast reaches
  up as well as across: it hurts any plane within 20 u of the burst, yours included.
- **Bombing low is dangerous.** A bomb keeps the plane's speed, so a plane flying straight on is
  right above it when it lands. Drop from below 20 u and your own blast hits you: at 10 u, for
  20 damage. Those hits land as a flak burst's do.
- **No impact marker in v0.1** (it's a v0.2 upgrade). You aim by your shadow. A developer switch
  (**F4**) turns the marker on for testing. It works the fall out with the same steps as the
  simulation, so it's exact.

### Practice targets

Damage (PR 4) and enemies (PR 6) come after weapons, so there are practice targets to shoot at
(owner, 2026-11-26). They stay afterwards as a developer tool.
- **4 drones** circle at **40, 90, 140 and 200 u** at **70 u/s**, over the middle of the map. They
  use the fighter model, tinted orange-red.
- **8 ground targets** (white and red bullseyes, 10 u across) sit along the way north.
- Hits don't hurt them. They glow, and their hit count shows for 2 s. The readout shows the total.
- Off at the start since enemies arrived. **F2** or `--targets` turns them on.

## Damage

### Parts and HP (player's fighter and enemy fighters)

| Part | HP | Damaged (gradual) | Destroyed |
|---|---|---|---|
| Engine | 40 | Top speed and acceleration fall, to **60%** at 1 HP | No thrust: glide home |
| Left wing, right wing | 40 each | Turn rate falls, to **60%** at 1 HP; the weaker wing sets it. The plane doesn't pull to one side | The wing breaks off and the plane crashes |
| Fuselage | 60 | Fuel leaks, growing to a full tank in **60 s** at 1 HP | All fuel is lost (glide home); then a **tail** of **20 HP** takes the hits, and losing it is a crash |
| Pilot | 3 wounds | | The third wound kills the pilot and the plane crashes |

- **A pilot wound is 5 damage reaching the pilot** (owner, 2026-11-26, "parts matter").
  - With 2-damage rounds, from ahead the engine fails or the pilot dies after about 40 rounds
    (about 3.5 s on target).
  - From behind it's about 75 rounds (the fuselage, then the tail).
- **The effects are gradual:** they grow smoothly as HP falls.
  - **Top speed and acceleration:** 60% + 40% × the engine's share of its HP.
  - **Turn rate:** 60% + 40% × the weaker wing's share.
  - **Leak:** (1 − the fuselage's share) of a full tank per 60 s.
- **Going down** (owner, 2026-11-26): a lost wing, a lost tail or a killed pilot sends the plane
  down out of control.
  - It spins at **180°/s** toward a lost wing, nose at **-35°**, with the engine off and the
    controls doing nothing.
  - It trails heavy black smoke and explodes on the ground. The run ends then, as **SHOT DOWN** or
    **PILOT KILLED**.
- **How damage shows on the plane** (owner, 2026-11-26):
  - a DANGER engine trails grey smoke, and a CRITICAL or destroyed one black;
  - a leaking fuselage trails a thin fuel spray;
  - a lost wing or tail is missing from the sprite. Those sprites are baked the first time they're
    needed.

  Until PR 7's damage diagram, the readout shows each part's stage in colour.
- **The display is stepped.** The HUD and the damage diagram show each part in one of four stages,
  plus destroyed:

  | Stage | Part HP | Pilot |
  |---|---|---|
  | OK | above 75% | no hits |
  | WARNING | 50-75% | 1 hit |
  | DANGER | 25-50% | |
  | CRITICAL | below 25% | 2 hits |

### Where hits land

The part that takes a hit depends on where it comes from, relative to the plane's nose:

| Fire from | Engine | Pilot | Wings | Fuselage |
|---|---|---|---|---|
| Ahead (±45° of the nose) | 45% | 20% | 25% | 10% |
| Behind (±45° of the tail) | 10% | 10% | 30% | 50% |
| A side | 10% | 15% | 50% (that side's wing) | 25% |
| Flak burst (below or nearby) | 15% | 10% | 45% | 30% |

Flying so the enemy is behind you protects the engine and pilot, at the cost of the fuselage.

- **How a direction is chosen:** a round's direction is back along its path. Bomb blasts land like
  flak bursts.
- **The wings' share:** it goes to a random wing, or to the side's own wing for side hits.
- **A destroyed fuselage:** hits that land on it go to the tail.
- **Tuning:** the table is in tuning.txt as `hit.ahead.engine` and so on.

### Collisions

- **The ground:** touching it is a crash. Landing on runways comes later.
- **Two planes:** they touch when their outlines overlap within **6 u** of height. Each outline
  is taken as a **7 u** circle, so their centres are within 14 u across.
  - The one with the **lower total HP** (engine + both wings + fuselage + tail) is destroyed
    there and then, in a fireball. The pilot isn't counted, since pilot hits aren't HP.
  - The other takes damage equal to the destroyed plane's remaining total, spread evenly over its
    five parts.
  - If the totals are equal, both are destroyed.
  - Practice drones don't collide.
- **Friendly fire** is always on. In v0.1 that only matters for bombers' gunners and the flak,
  whose shots can hit their own side.

## Enemies

Enemy planes use the same flight and damage model as the player, driven by AI input commands.

The enemy is the **other nation** (`planes.md`): fly the US and you fight Germany's He 51s and He
111Bs over German flak; fly Germany and you fight US P-26As and B-10Bs over US flak.

**Built in three steps** (owner, 2026-11-27): fighters and waves, then bombers and their gunners,
then flak.

### Fighters

| | |
|---|---|
| Plane | The enemy nation's fighter (He 51 or P-26A), with its real stats |
| Style | Each fighter gets one of three, at random |
| Turn-fighter | Gets on your tail and stays there |
| Dive-and-climb | Climbs above you, dives through, climbs away |
| Head-on | Lines up for passes from the front |

- **Rookies that sharpen** (owner, 2026-11-27): a wave-1 pilot's aim is off by up to **8°** and it
  looks at you every **0.5 s**; each wave sharpens that, down to **2°** and **0.15 s**.
- **Unlimited ammunition and fuel** (owner).
- Every pilot leads its shots, fires within **220 u** with its nose within **5°** of its aim, pulls
  up before **40 u** (2,000 ft), turns back **250 u** inside the edges, breaks off rather than ram,
  and keeps clear of the other enemies.
- **Dive-and-climb** gets **80 u** (4,000 ft) above you before it attacks; **head-on** gets **400 u**
  in front first.
- **A kill** is an enemy that goes down after you have damaged or rammed it.

### Bombers

- The enemy nation's bomber (He 111B-2 or B-10B), with its real stats.
- They fly **south in groups of 2**, at **8,000-15,000 ft** (160-300 u) near cruising speed, to
  bomb your base.
- They're tough: **two engines of 60 HP each** (losing one halves the power), each wing **100**,
  fuselage **160**, tail **40**.
- **Three gunners** (nose, dorsal, ventral), with the bomber's real guns and rate, fire at any
  plane within **160 u** in their arcs, using the forgiving band.
  - **Nose:** forward, ±60° of the nose.
  - **Dorsal:** behind and above, ±120° of the tail.
  - **Ventral:** behind and below, ±60° of the tail.
- **Each gunner can be knocked out on its own.** It takes **3 wounds** (5 damage each), like a
  pilot, and then stops firing; the bomber flies on.
  - The nose gunner takes **10%** of hits from ahead, out of the engines' share.
  - The dorsal and ventral gunners each take **8%** of hits from behind, out of the fuselage's
    share.
- In v0.1 a bomber reaching the south edge shows "Bomber reached base" and adds to a counter in
  the summary. It costs nothing until v0.2.

### Flak guns

- On the ground, **8 at the start**, in clusters across the middle and north of the map. They're
  the enemy nation's **real light and heavy guns** (`planes.md`), half of each.
- **Light flak** (2 cm Flak 30, or the US 37 mm M1) fires aimed streams of shells at its real
  rate, like guns leading the target.
  - Deadly low; out of reach above about 3,500 ft (German) or 10,500 ft (US).
  - Range across the ground is **240 u**.
- **Heavy flak** (8.8 cm Flak 18, or the US 3-inch M3) fires timed bursts at its real rate (one
  every 3-4 s) at where a plane is heading.
  - It reaches about 26,000-28,000 ft, with a range of **400 u** across the ground.
  - **Less accurate the higher you are:** burst scatter is **4 u** plus **0.15 u** per unit of
    the target's height.
  - Each burst has a blast radius of about **15 u** and does about **13 damage** at the centre.
- **30 HP** (heavy guns 45). Bombs and strafing destroy them.

### Waves

- The first wave arrives at **10 s**, then one every **40 s**, from the north edge.
- **Wave n** brings **1 + n/2 fighters** (rounded down). Every 3rd wave adds **2 bombers**.
- Fighters arrive **within about 3,000 ft of your height** (owner, 2026-11-27). At most **6** are in
  the air; the rest of a wave waits for a place.
- **Every wave is announced** with a banner (its size and height), and the readout counts waves
  and kills (owner).
- **Off-screen enemies** get an arrow at the screen edge, with their height against yours (owner).
- **Bombers** (owner, 2026-11-27) stay at 8,000-15,000 ft, fly at their real cruising speed, and
  are announced with their height about **45 s** before they arrive, so you can climb to them.
- **1 more flak gun** is placed every 2 waves, up to **16**.

## HUD

- **Fuel** gauge.
- **Weapons:** rounds left, bombs left, and the selected heavy weapon.
- **Damage diagram:** a small outline of the plane. Each part is coloured by stage (OK green,
  WARNING yellow, DANGER orange, CRITICAL red), and a destroyed part is drawn missing. The pilot
  has a marker.
- **Height and speed**, with a **STALL** warning.
  - Height is shown in **feet**, at **50 ft per u**; the ceiling reads 30,000 ft. The simulation
    itself works in u.
  - Speed is shown in **real mph**, at **1.56 mph per u/s** (`planes.md`'s scale): the P-26A's
    top level speed reads its real 234 mph, the He 51's 205 mph.
- **Radar / minimap:** nearby enemy planes, flak guns, and an arrow toward the base (south). It
  shows positions only, never heights.
- **Edge warning** when near a soft edge, and the **countdown** once past it.

## Run end and menus

- **End of a run:** shot down, crashed, pilot killed, left the battle area, or returned to base.
  A summary shows the
  cause, time flown, kills by type, and bombers that reached base. A key starts a new run.
  - **Until the summary arrives (PR 7):** the view freezes with a banner giving the cause and the
    time flown. Enter or R starts a new run (owner, 2026-11-26).
- **Pause menu (Esc):** resume, restart, controls (rebinding), options (gunnery height rule),
  quit.
- **Before a run, you choose your nation:** the US or Germany in v0.1 (owner, 2026-11-26). You fly
  its fighter, and the other nation is the enemy. A simple screen and `--nation us|de` come in
  PR 5; PR 7's menus make it a proper screen.

## Look (placeholder art)

- Planes are **simple blocky 3D shapes**, baked when the game starts into sprites at:
  - **32 headings**;
  - **9 pitches**, one every 10° from -40° to +40° (owner, 2026-11-26), so the nose visibly
    rises and drops;
  - **3 banks** (left, level, right).
- That's 864 poses, in 48x48 cells, baked in about 0.1 s.
- **Every plane is a blocky original model of the real aircraft,** in its nation's pre-war colours
  and with no national markings (`planes.md`).
  - **Boeing P-26A** (owner, 2026-11-26): low wing, big round radial cowling, wheel spats, open
    cockpit with headrest. Olive drab fuselage, yellow wings and tail, red spinner.
  - **Heinkel He 51:** biplane (upper and lower wings with struts), inline engine, fixed gear.
    Light grey.
  - **Martin B-10B:** twin-engine, glazed nose turret, enclosed cockpits. True blue fuselage,
    yellow wings.
  - **Heinkel He 111B:** twin-engine, rounded wings, stepped cockpit, dorsal and ventral guns.
    Green and brown splinter over pale blue.
  - Twin-engined planes are 7 parts: each engine is its own part, so a lost engine can smoke on
    its own.
- The models are drawn in the true 2:1 view, flat-shaded by a sun from the upper left. A 1 px
  dark outline makes them read on any field.
- Lost wings and the tail are left off the model, so damage shows at every angle without
  drawing each one.
- Shadows are the same shapes flattened onto the ground.
- Flak, bursts, tracers and bombs are simple drawn shapes. Real art replaces all of it in v0.3.

## Developer tools

- `--seed N`, `--wave N` (start at a wave), `--window WxH`.
- `--shot FILE`: fly `--ticks N` ticks (default 120) with no input, save the view as a PNG, and
  exit, for checking the drawing.
  - `--heading DEG`, `--height U`, `--pitch DEG`, `--throttle PCT` and `--at X,Y` set how the
    plane starts.
  - `--overlay` starts with the F1 overlay shown.
  - `--fire` and `--bomb` hold the guns or the bomb release throughout.
- `--sprite-sheet FILE`: saves every baked plane pose to a PNG and exits (the `--nation`'s
  fighter); `--sprite-scale PCT` picks the size (25-150, in eighth steps).
- `--nation us|de`: fly that nation's fighter and skip the nation screen (design/planes.md).
- **`tuning.txt` format:** the file starts with `format = 3`. A file of an older format is renamed
  `tuning-format<N>.txt` (a file with no format line is format 1) and the defaults are written, so
  its old ceiling, tank and zoom numbers don't quietly win. Each plane's speeds, climb, stall, turn, pitch rate and tank
  come from its real stats through the `scale.*` keys, so they're no longer in the file.
- **F2** practice targets on/off (`--targets` starts with them on), **F3** the height rule
  (`--height-rule`), **F4**
  the bomb marker.
- **Damage testing until enemies arrive** (owner, 2026-11-26):
  - **F5** takes a 5-round gun burst from a random side;
  - **F6** takes a flak-sized hit (12 damage);
  - **F7** repairs everything;
  - `--damage engine=10,lwing=0,rwing=,fuselage=,tail=,pilot=` starts with damage, for
    screenshots.
- **F1:** an overlay of hitboxes, part HP numbers, and AI styles.
- **Switches** for the bomb marker and for invulnerability.

## How it's built (pull requests, one issue each)

1. Window, 480x270 scaling with more world on uneven screens, the isometric ground and the camera.
2. Flight: physics, fuel, glide, stall, edges, shadow, the baked plane sprites.
3. Weapons: guns, bombs, the three height rules, heavy-weapon switching.
4. Damage: parts, hit location, effects, stages, breakage, crashes, collisions.
5. **Real planes** (owner, 2026-11-26):
   - the roster and scale from `planes.md`, so each plane flies, climbs, turns and stalls by its
     real stats;
   - guns by calibre with real rates and ammo, and bombs by weight;
   - twin engines;
   - the 30,000 ft ceiling with the ground zoom, and real mph and feet;
   - the He 51 model;
   - choosing the US or Germany.
6. Enemies, in three builds (owner, 2026-11-27): the other nation's fighters (three styles) and
   waves; then bombers with three gunners and the B-10B and He 111B models; then light and heavy
   flak.
7. HUD, pause and controls menus, rebinding, the nation screen, the run summary.

The simulation (`src/core`) has no SDL in it. It's deterministic for a given seed and list of
input commands, so tests can replay scripted flights.

## Acceptance

**Unit tests** (core only):
- **Real planes:** each plane's worked-out flight hits its scaled top speed, best climb and stall
  (within 3%), and its turn rate; guns fire at their real rates with their real ammo; damage
  follows calibre; a twin loses half its power with one engine; the ceiling is 30,000 ft.
- **Flight:**
  - climbing loses speed and diving gains it, up to the dive limit;
  - stall below 55 u/s drops the nose;
  - the ceiling holds;
  - a fighter takes about 2 minutes from the deck to 30,000 ft, a bomber longer;
  - sizes by depth: a quarter for the ground from the top, up to 1.5x above.
- **Fuel:**
  - burn follows the throttle;
  - an empty tank glides at about 6:1.
- **Damage effects:** each part's effect scales with its HP, and the stage thresholds are right.
- **Breakage:**
  - a destroyed wing crashes the plane;
  - a destroyed engine or fuselage leaves it gliding;
  - the tail breaks after the fuselage;
  - the third pilot hit crashes it;
  - a bomber gunner stops firing after 3 hits, and the bomber flies on.
- **Hit location:** the weights hold for each direction, including the bomber gunners' shares.
- **Height rules:** each one hits and misses at its own limits.
- **Bombs:**
  - impact point and blast falloff;
  - your own blast hits you when you drop from below 20 u, and not from above.
- **Collisions:** the lower total HP is destroyed and the survivor takes the remainder; ground
  contact crashes.
- **Edges and waves:**
  - past a soft edge the countdown runs, stops when the plane flies back in, and destroys it at 0;
  - leaving south ends the run;
  - wave sizes follow the formula.
- **Enemies:** pilots turn to their target and only fire close and nose on, pull out of dives, stay
  in the area, get onto a tail, climb before diving, aim better each wave, and never run out; kills
  are credited; edge arrows point the right way.
- **Determinism:** the same seed and inputs give the same result.

**Playtest by the owner:** the CI Windows build. Numbers change in `tuning.txt` from the notes.

## Open questions

The first draft's eight questions are answered above, and so are the two that came out of them:
speed shows in mph, and bomber gunners are knocked out as described under Bombers.

From the real-planes rework (and `planes.md`'s own open questions):
1. **The three gunners' hit shares:** the earlier two-gunner rule, re-cut for three. The nose takes
   10% from ahead; the dorsal and ventral take 8% each from behind. Right?
2. **Bomber cruising heights** of 8,000-15,000 ft: right for these pre-war bombers?
3. **Flak ranges across the ground** (light 240 u, heavy 400 u) and heavy-gun HP (45): start
   values to check in play.
