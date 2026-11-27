# Changelog

All notable changes to Wings of Freedom. Each pull request adds a line under Unreleased; a release moves
them under its version (see CLAUDE.md, "Releasing").

## [Unreleased]

### Design
- `design/game.md`: the game overview, rules agreed so far, and open questions.
- `design/flight.md`: the v0.1 Fly and fight spec.

### Added
- v0.1 weapons: two cowl guns (F, every 3rd round a tracer) with the forgiving, strict and auto-aim height rules; bombs (Space, hold to ripple) that fall from your height and speed, with blasts that can catch you if you drop low and scorch marks that fade; practice drones and ground targets that count hits; developer keys F2-F4.
- v0.1 flight: throttle, climb and dive with speed traded for height, stall, fuel and gliding, the edge countdown, flying home, and ground crashes; the placeholder P-26 Peashooter-style fighter baked at 32 headings x 9 pitches x 3 banks, with its shadow; a run-end banner and temporary readout.
- v0.1 window and camera: 480x270 pixel art at whole-number scales (more world on uneven screens), the isometric farmland, and a camera that follows a stand-in plane; live tuning file and screenshot tools.
- Project set up: CI (check, unit tests, test build), tag releases, issue and pull request templates.
- Board and workflow page links.
