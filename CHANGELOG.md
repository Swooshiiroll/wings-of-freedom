# Changelog

All notable changes to Wings of Freedom. Each pull request adds a line under Unreleased; a release moves
them under its version (see CLAUDE.md, "Releasing").

## [Unreleased]

### Design
- `design/game.md`: the game overview, rules agreed so far, and open questions.
- `design/flight.md`: the v0.1 Fly and fight spec.
- `design/planes.md`: real planes, nations and eras; the v0.1 roster (P-26A, He 51B, B-10B, He 111B-2) with sources and the scale; `game.md` and `flight.md` reworked to match (30,000 ft ceiling with the ground zooming out, guns by calibre, twin engines, choosing your nation).

### Added
- Enemy flak: a front line of gun groups with trenches and shell holes halfway up the map and sites beyond; machine guns, then light cannon from wave 5 and heavy guns from wave 9, with reinforcements; destroyed by strafing or bombs for 50 or 150 points.
- Enemy bombers: the Keystone B-6A and Dornier Do 23G in the first raids, the B-10B and He 111B-2 from wave 9; formations every 3rd wave flying to your base, warned as they enter; nose, dorsal and ventral gunners in bursts that can be knocked out; escorts from wave 6; a counter for bombers that reach your base; larger bomber arrows.
- The aircraft tree: points from kills with chains, banked only when you get home (with condition and wave bonuses); a run summary; a hangar after the nation screen to buy the next fighter and six upgrades per plane; the bomb sight as an upgrade; progress saved in `progress.txt`; points and chain on the dashboard.
- The aircraft tree's planes: the P-12E, Ar 65E, P-36A and Bf 109B with their real stats and models; the P-26A starts with two .30s; enemy fighters grow up their tree with the waves; a plane screen with all six (and `--plane`).
- A bigger, north-up map: north is straight up the screen (fields still look diagonal) over 28,800 x 28,800 u, with a river every 6,000 u; fighter waves fly in from the north, east or west edges (the first fight about 2 minutes in); one edge arrow per direction, marked higher, level or lower; practice targets by the new start.
- Cockpit dashboard (design/dashboard.md): a bottom instrument panel (96, 80 or 64 px) with pre-war US and German styles and units, airspeed, altimeter, compass, turn-and-bank, climb, pitch, tachometer, fuel, oil pressure, clock, ammo drums, bomb and warning lamps, a message strip and a damage wireframe; a Units setting for arrows and banners; replaces the text readout and pitch ladder.
- Pause menu (Esc, focus loss, run-end screen): resume, restart, options, quit, by keyboard or mouse; rebinding with two keys per action, saved in `settings.txt`; auto-leveling On/Assisted/Off; separate aim strictness for your guns and the enemy's; a pitch ladder on the HUD; the nation screen clickable.
- v0.1 enemy fighters and waves: the other nation's fighter in three styles (turn-fighter, dive-and-climb, head-on) that sharpen with each wave and never run out; waves from 10 s then every 40 s (1 + n/2, at most 6 in the air) arriving near your height with a banner; kills in the readout; arrows toward off-screen enemies with their height against yours; F1 labels each pilot; practice targets now off at the start (F2 or `--targets`), and `--wave N`.
- v0.1 real planes: the P-26A, He 51B, B-10B and He 111B-2 fly by their real speed, climb, stall, turn and endurance (scaled), with their real guns (rounds per minute, ammo, damage by calibre) and bomb loads (blasts by weight); twin engines take hits and smoke on their own; the He 51 biplane model; a 30,000 ft ceiling that takes about 2 minutes to climb to (a third of every climb and dive is height; tanks doubled); everything sized by its depth from your plane, with the ground a quarter size from the top and planes baked as crisp pixel art at each size; a nation screen (US or Germany) and `--nation us|de`; readouts in real mph with rounds per gun type. An older `tuning.txt` is kept as `tuning-format<N>.txt` and the new defaults are written.
- Updater: `update-game.cmd` / `update-game.ps1` (Windows) and `update-game.sh` (Linux, Steam Deck) install or update the game to the newest build on GitHub, keeping `tuning.txt`; they ship inside every build.
- v0.1 damage: engine, wings, fuselage, tail and pilot with gradual effects (top speed, turning, fuel leak) and OK/WARN/DNGR/CRIT/GONE stages; hits land by direction; rounds do 2 damage and a pilot wound takes 5; lost wings, tail or pilot send the plane spinning down; plane collisions; engine smoke, fuel spray and missing parts on the sprite; developer keys F5-F7 and --damage.
- v0.1 weapons: two cowl guns (F, every 3rd round a tracer) with the forgiving, strict and auto-aim height rules; bombs (Space, hold to ripple) that fall from your height and speed, with blasts that can catch you if you drop low and scorch marks that fade; practice drones and ground targets that count hits; developer keys F2-F4.
- v0.1 flight: throttle, climb and dive with speed traded for height, stall, fuel and gliding, the edge countdown, flying home, and ground crashes; the placeholder P-26 Peashooter-style fighter baked at 32 headings x 9 pitches x 3 banks, with its shadow; a run-end banner and temporary readout.
- v0.1 window and camera: 480x270 pixel art at whole-number scales (more world on uneven screens), the isometric farmland, and a camera that follows a stand-in plane; live tuning file and screenshot tools.
- Project set up: CI (check, unit tests, test build), tag releases, issue and pull request templates.
- Board and workflow page links.
