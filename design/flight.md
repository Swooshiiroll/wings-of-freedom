# Fly and fight (v0.1)

The spec for the first playable build. You fly a real pre-war fighter of the US or Germany, from
its aircraft tree (`aircraft_tree.md`), with full altitude control, burning fuel. You fight the other nation's
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
- **View** (owner, 2026-11-27): at 480x270, **north is straight up** the screen and south (home)
  straight down, seen from 30° above. The farmland's field axes run diagonally across it, so the
  fields still look isometric. One unit of ground is about one art pixel.
- **Height:** 0 is the ground. It is drawn as the gap between a plane and its shadow, **0.4 px per
  u**, sized by depth (design/planes.md, Height).
  - The ceiling is **30,000 ft (600 u) for every plane** (owner, 2026-11-26).
  - **Everything is sized by its depth** from your plane: full size at your height, down to a
    quarter for the ground from 30,000 ft, up to 1.5× above you. Planes change size in crisp
    eighth steps; the ground is drawn smooth (the one exception to pixel-perfect).
  - **The shadow gap** grows to **100 px** by about 17,000 ft, then holds (owner, 2026-11-27, so
    your plane can stay near the middle).
- **Map:** **28,800 u by 28,800 u**, about 12.5 miles square: about 2 minutes at cruising speed
  from the middle to any side (owner, 2026-11-27). The ground is flat in v0.1, with drawn detail
  only.
- **The ground** is a **farmland patchwork** (placeholder look until v0.3's art).
  - Rectangular fields of wheat, barley, ploughed earth, pasture and meadow, plus woods.
  - Hedgerows between fields, dirt roads on a grid, and a winding river every 6000 u, as
    landmarks.
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
- **Camera** (owner, 2026-11-27): your plane **stays near the middle** of the view above the
  dashboard. As you climb it rides up, at most **30 px** above the middle, while the shadow gap
  widens. The view looks a little ahead along your heading (**10 u**, at most about 15 px).
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
| Next wave (once it's offered; see Waves) | N |
| Pause menu | Esc |

The front end turns keys into one **input command per tick** for the player's plane: turn, pitch,
throttle change, fire guns, release heavy weapon, switch heavy weapon, call the next wave. AI
planes produce the same commands (but never call a wave), so every plane flies by the same rules.

If your settings already give a new action's default key to something else, the new action starts
unbound.

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
- **Pitch:** W/S changes it at **60°/s** (bombers 30°/s), up to **±40°**. What happens when you let
  go is your **auto-leveling** setting (owner, 2026-11-27):
  - **On** (default): the nose eases back to level at the same rate.
  - **Assisted:** only if you let go within **5°** of level (`flight.assist_zone_deg`); otherwise it
    stays. A held key passes straight through.
  - **Off:** the nose stays where you leave it. A stall still drops it.
  Height changes at speed × sin(pitch)
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
- **A run starts** in the air near the middle of the south edge, heading north at **160 u**
  (8,000 ft; owner, 2026-11-27) and **110 u/s**, with **80%** throttle.
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
- **The P-26A:** two **.30 cal** in the cowling (owner, 2026-11-27: the .50 is an upgrade), 1 u
  either side of the nose.
  - Each fires about 1,000 rounds/min with 500 rounds: about 30 s of fire.
  - With the .50 upgrade, one .30 becomes a .50 (about 600 rounds/min, 200 rounds, 2.5 damage a
    hit).
- **The He 51:** two 7.92 mm MG 17s, the same way. The other fighters' guns are in `planes.md`.
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

Your guns and the enemy's have **separate settings** (owner, 2026-11-27), both Forgiving by default.

| Rule | A round hits a plane when | Default |
|---|---|---|
| Forgiving band | Within **±10 u** of its height | Yes |
| Strict | Within **±3 u** of its height | |
| Auto-aim in height | The target is within **±12°** of the nose and **±40 u** of height; rounds angle toward it | |

You judge other planes' heights from their **shadows only**. There are no height markers.

Under auto-aim, a round's climb is set when it's fired, toward the nearest plane or drone in the
cone. Its hit is then tested with the forgiving band. **F3** cycles your rule (`--height-rule
forgiving|strict|auto` sets it for one run).

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
- **4 drones** circle at **40, 90, 140 and 200 u** at **70 u/s**, just north of the start. They
  use the fighter model, tinted orange-red.
- **8 ground targets** (white and red bullseyes, 10 u across) sit along the way north.
- Hits don't hurt them. They glow, and their hit count shows for 2 s. The dashboard's message strip shows the total.
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

  The dashboard's wireframe shows each part's stage in colour (dashboard.md).
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
- **Mid-air collisions option** (owner, 2026-11-27; options menu, On by default): switched Off,
  nothing collides with your plane. Enemy planes can still collide with each other, and the ground
  is unchanged.
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
| Plane | From the enemy's aircraft tree, growing with the waves (`aircraft_tree.md`) |
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

Four, with their real stats (`planes.md`). The early ones fly the first raids so the early
fighters can catch them (owner, 2026-11-27).

| | Waves 3 and 6 | Wave 9 on | Toughness (engine / wing / fuselage / tail) |
|---|---|---|---|
| US | Keystone B-6A (cruise 103 mph) | Martin B-10B (193 mph) | 45 / 75 / 120 / 30, then 60 / 100 / 160 / 40 |
| Germany | Dornier Do 23G (130 mph) | Heinkel He 111B-2 (229 mph) | the same |

- **Every 3rd wave** brings a formation: **a pair** at wave 3, **a V of three** at wave 6, **five**
  from wave 9, spaced **80 u** across and **50 u** back (`wave.*`, start values).
- From wave 6 they have **escort fighters**: one for each pair (three bring 1, five bring 2), of the
  enemy types for that wave. Escorts **guard** their bombers: they break off to fight you within
  **600 u** of the formation and return once you're beyond **1,200 u** (start values). Neither
  bombers nor escorts count toward the 6 fighters in the air at most.
- They come in at the **north edge** within **3,000 u** of your base's line and fly **straight to
  your base**, **holding course** under fire, at their real cruising speed: the early ones at
  **8,000-12,000 ft** (their real ceiling), the later ones at **8,000-15,000 ft**. Escorted
  formations slow to **90%** of their slowest escort's top speed.
- They're **announced as they enter**: how many, their height, their direction from you, and any
  escorts.
- **Two engines** (losing one halves the power). They're **bigger marks**: rounds and rams reach
  twice a fighter's distance.
- **Three gunners** (nose, dorsal, ventral), with the bomber's real guns and rate, fire at any
  player within **160 u** in their arcs, in **short bursts** (0.6 s on, 0.6 s off). They follow
  your enemy aim setting and sharpen with later waves. They hold fire rather than hit their own
  side.
  - **Nose:** forward, ±60° of the nose.
  - **Dorsal:** behind and level or above, ±120° of the tail.
  - **Ventral:** behind and level or below, ±60° of the tail.
- **Each gunner can be knocked out on its own.** It takes **3 wounds** (5 damage each), like a
  pilot, and then stops firing; the bomber flies on.
  - The nose gunner takes **10%** of hits from ahead, out of the engines' share.
  - The dorsal and ventral gunners each take **8%** of hits from behind, out of the fuselage's
    share. A knocked-out gunner's share goes back.
- A bomber reaching the south edge shows **"Bomber reached your base"**, counted in the run
  summary. It costs nothing until v0.2. A falling bomber crossing the edge doesn't count.
- Shooting one down is worth **250 points** (`aircraft_tree.md`). Their off-screen arrows are drawn
  larger.

### Flak guns

The enemy nation's real guns (`planes.md`; owner, 2026-11-27).

- **Where:** a **front line halfway up** the map (about 2 minutes' flying from your base), **14
  groups of 3** spread across it with gaps to slip through, four of them **strongpoints**; and **6
  sites** of 3 guns farther north, where ground targets will go (`flak.*`, start values).
- **The front line shows:** a 400 u band of mud, zigzag trenches and shell holes.
- **They step up with the waves:**
  - waves 1-4: **machine guns**;
  - from wave 5, surviving machine guns are **swapped in place** for **light cannon**;
  - from wave 9, **heavy guns** at the sites and the strongpoints.
- **Reinforcements:** a gun added to a group or site every 2 waves, up to 8. Destroyed guns stay
  destroyed, as wrecks.
- **Aim:** like the enemy pilots, following your enemy aim setting and sharpening with the waves.
  - Machine guns and cannon fire aimed streams at their real rate, every round a tracer.
  - Heavy guns fire timed shells to where you're heading. Bursts scatter **4 u** plus **0.15 u**
    per u of your height, and damage like a bomb's blast.
- Guns fire only at players and hold fire rather than shoot through their own planes. A burst can
  still catch an enemy that flies into it.
- **Warning:** only what you see: flashes, tracers and black bursts.
- **Destroying them:** strafe (rounds reaching the ground on the gun) or bomb. A machine gun has
  **20 HP**, a cannon **30**, a heavy **45**. Light guns are worth **50 points**, heavy ones
  **150**, in chains like kills.

### Waves

- The first wave sets out at **10 s**.
- **The next waits until everything is down** (owner, 2026-11-27): fighters, bombers and escorts,
  shot down or gone (a bomber that reaches your base is gone). Fighters still waiting for room
  count as up. Then a "wave cleared" notice and a **10 s** break (`wave.break_s`).
- **Calling the next wave:** if a wave is still up **3 minutes** after it set out
  (`wave.call_after_s`), a popup offers the next: press **N**. Nothing is forced. The old planes
  stay, and the following wave needs them all down too (or another 3 minutes).
- Fighters appear **6,000 u** from you (`wave.out_u`, about a minute of their flying), from the
  **north, east or west**, never the south: those arriving together from one side with room on
  the map, abreast of you or north of you. Flying to meet them, the fight starts in about half a
  minute.
- Bomber raids still come in at the north edge, so an un-met raid takes several minutes to reach
  your base.
- **Wave n** brings **1 + n/2 fighters** (rounded down). Every 3rd wave adds a bomber formation
  (above), outside the 6 at most.
- The fighters **grow up the enemy's aircraft tree**: step 1 in waves 1-4, mixed with step 2 in
  5-8, steps 2 and 3 in 9-12, step 3 from wave 13 (`aircraft_tree.md`).
- Fighters arrive **within about 3,000 ft of your height** (owner, 2026-11-27). At most **6** are in
  the air; the rest of a wave waits for a place.
- **Every wave is announced** with a notice on the dashboard (its size and height), which also counts waves
  and kills (owner).
- **Off-screen enemies** get an arrow at the screen edge (owner, 2026-11-27). Enemies in about the
  same direction share one. A mark beside it says **higher**, **level** (within about 1,000 ft)
  or **lower**, for the nearest of them. No numbers.

## HUD

- **The cockpit dashboard** (owner, 2026-11-27): an instrument panel across the bottom, with every
  gauge and warning. See `dashboard.md`.
- **Height and speed** read in the nation's units on the gauges:
  - Height at **50 ft per u**; the ceiling reads 30,000 ft. The simulation itself works in u.
  - Speed as **real mph**, at **1.56 mph per u/s** (`planes.md`'s scale): the P-26A's top level
    speed reads its real 234 mph, the He 51's 205 mph.
- **Radar / minimap:** nearby enemy planes, flak guns, and an arrow toward the base (south), for
  eras that had radar (not the pre-war one). It shows positions only, never heights.

## Run end and menus

- **End of a run:** shot down, crashed, pilot killed, left the battle area, or returned to base.
  A summary shows the cause, time flown, kills by type, and the points: banked (with the bonuses)
  or lost (`aircraft_tree.md`). Enter goes to the hangar; R flies again. Bombers that reached base
  join it with the bombers.
- **Pause menu (Esc;** owner, 2026-11-27): also opens when the window loses focus, and from the
  run-end screen. Resume, restart run, options, quit.
  - **Options:** controls, auto-leveling, your gun aim, enemy gun aim, mid-air collisions,
    dashboard size, units.
  - **Controls:** two keys per action. Binding a key already in use moves it here; Delete clears
    one; reset to defaults. Esc and F1-F12 are reserved.
  - Keyboard (arrows or W/S, Enter, Esc) and mouse. The nation screen works the same way.
  - Choices are saved in `settings.txt` next to the game, which the updater keeps.
- **Before a run, you choose your nation**, then your plane in the **hangar**
  (`aircraft_tree.md`). The other nation is the enemy (owner, 2026-11-26).

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
  - **Boeing P-12E:** a short, stubby biplane with a radial engine; olive drab and chrome yellow.
  - **Arado Ar 65E:** a big biplane, long inline engine, deep chin radiator, two struts a side;
    grey-green.
  - **Curtiss P-36A:** low-wing monoplane with a radial and an enclosed canopy; bare metal with
    yellow wings and tail.
  - **Messerschmitt Bf 109B:** slim low-wing monoplane, pointed nose, square wingtips; dark
    grey-green over pale blue.
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
  - `--menu pause|options|controls` shows a menu page over the shot (shots use the default
    settings); `--menu hangar` saves the hangar.
- `--sprite-sheet FILE`: saves every baked plane pose to a PNG and exits (your plane's);
  `--sprite-scale PCT` picks the size (25-150, in eighth steps).
- `--plane ID` (p12e, p26a, p36a, ar65, he51b, bf109b) flies it at once, without the hangar;
  `--nation us|de` skips the nation screen.
- `--upgrades LIST`, `--points N`, `--unlock-all`: see `aircraft_tree.md`. Nothing is saved.
- **`tuning.txt` format:** the file starts with `format = 4`. A file of an older format is renamed
  `tuning-format<N>.txt` (a file with no format line is format 1) and the defaults are written, so
  its old ceiling, tank and zoom numbers don't quietly win. Each plane's speeds, climb, stall, turn, pitch rate and tank
  come from its real stats through the `scale.*` keys, so they're no longer in the file.
- **F2** practice targets on/off (`--targets` starts with them on), **F3** your gun aim
  (`--height-rule`), **F4** the bomb marker.
- **Damage testing** (owner, 2026-11-26, for before enemies; still handy):
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
7. Pause and options menus (rebinding, auto-leveling, aim; pulled forward, owner 2026-11-27), then
   the cockpit dashboard (design/dashboard.md), the nation screen and the run summary.

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
  - wave sizes follow the formula, and waves come only from the north, east and west.
- **View:** north draws straight up and east to the right; planes' noses follow their headings.
- **Enemies:** pilots turn to their target and only fire close and nose on, pull out of dives, stay
  in the area, get onto a tail, climb before diving, aim better each wave, and never run out; kills
  are credited; edge arrows point the right way and group by direction.
- **Determinism:** the same seed and inputs give the same result.

**Playtest by the owner:** the CI Windows build. Numbers change in `tuning.txt` from the notes.

## Open questions

The first draft's eight questions are answered above, and so are the two that came out of them:
speed shows in mph, and bomber gunners are knocked out as described under Bombers.

The gunners' hit shares (as written under Bombers) and their cruising heights of 8,000-15,000 ft
are confirmed (owner, 2026-11-27). Still open, from the real-planes rework (and `planes.md`'s own
open questions):
1. **Flak** reaches, groups and timing: start values to check in play.
