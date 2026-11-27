# Changelog

All notable changes to Wings of Freedom. Each pull request adds a line under Unreleased; a release moves
them under its version (see CLAUDE.md, "Releasing").

## [Unreleased]

### Design
- `design/game.md`: the game overview, rules agreed so far, and open questions.
- `design/flight.md`: the v0.1 Fly and fight spec.
- `design/planes.md`: real planes, nations and eras; the v0.1 roster (P-26A, He 51B, B-10B, He 111B-2) with sources and the scale; `game.md` and `flight.md` reworked to match (30,000 ft ceiling with the ground zooming out, guns by calibre, twin engines, choosing your nation).

### Added
- v0.1 real planes: the P-26A, He 51B, B-10B and He 111B-2 fly by their real speed, climb, stall, turn and endurance (scaled), with their real guns (rounds per minute, ammo, damage by calibre) and bomb loads (blasts by weight); twin engines take hits and smoke on their own; the He 51 biplane model; a 30,000 ft ceiling that takes about 2 minutes to climb to (a third of every climb and dive is height; tanks doubled); everything sized by its depth from your plane, with the ground a quarter size from the top and planes baked as crisp pixel art at each size; a nation screen (US or Germany) and `--nation us|de`; readouts in real mph with rounds per gun type. An older `tuning.txt` is kept as `tuning-format<N>.txt` and the new defaults are written.
- Updater: `update-game.cmd` / `update-game.ps1` (Windows) and `update-game.sh` (Linux, Steam Deck) install or update the game to the newest build on GitHub, keeping `tuning.txt`; they ship inside every build.
- v0.1 damage: engine, wings, fuselage, tail and pilot with gradual effects (top speed, turning, fuel leak) and OK/WARN/DNGR/CRIT/GONE stages; hits land by direction; rounds do 2 damage and a pilot wound takes 5; lost wings, tail or pilot send the plane spinning down; plane collisions; engine smoke, fuel spray and missing parts on the sprite; developer keys F5-F7 and --damage.
- v0.1 weapons: two cowl guns (F, every 3rd round a tracer) with the forgiving, strict and auto-aim height rules; bombs (Space, hold to ripple) that fall from your height and speed, with blasts that can catch you if you drop low and scorch marks that fade; practice drones and ground targets that count hits; developer keys F2-F4.
- v0.1 flight: throttle, climb and dive with speed traded for height, stall, fuel and gliding, the edge countdown, flying home, and ground crashes; the placeholder P-26 Peashooter-style fighter baked at 32 headings x 9 pitches x 3 banks, with its shadow; a run-end banner and temporary readout.
- v0.1 window and camera: 480x270 pixel art at whole-number scales (more world on uneven screens), the isometric farmland, and a camera that follows a stand-in plane; live tuning file and screenshot tools.
- Project set up: CI (check, unit tests, test build), tag releases, issue and pull request templates.
- Board and workflow page links.
