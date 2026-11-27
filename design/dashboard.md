# The cockpit dashboard

Gauges and indicators on an instrument panel across the bottom of the screen. Agreed with the owner
on 2026-11-27; it replaces v0.1's temporary text readout and pitch ladder.

## Styles follow the era

Every era of aircraft gets its own panel style and its own set of instruments (owner). v0.1 has the
**pre-war** panels, one per nation:

| Nation | Panel | Units |
|---|---|---|
| US | olive drab, brass-rimmed black dials | mph, feet, thousands of ft/min |
| Germany | RLM grey, dark-rimmed black dials; the rifle guns labelled 7.9 | km/h, metres, m/s |

A plane's gauges always read its nation's units. The **Units** setting only changes the enemy
arrows, wave banners and other text: **Nation** (default), **Imperial** or **Metric**.

## Layout

The world is drawn above the panel. **Dashboard size** (a setting): **Large 96 px** (default),
**Medium 80**, **Small 64**, of the 270-row screen. A wider screen gets the panel centred, its
colour running out to the sides.

```
| (AIRSPEED) (ALTIMETER) (COMPASS) (TURN-BANK) (CLIMB) (PITCH) |  damage    |
| (TACH) (FUEL) (OIL) (CLOCK)  [.30 0500][.50 0200] o o  lamps |  wireframe |
| message strip                                                |  top view  |
```

**Needles only:** the dials have no number windows. Smaller panels show fewer numbers on the faces.

## Instruments (pre-war)

| Instrument | Shows |
|---|---|
| Airspeed | 0-300 mph, or 0-500 km/h |
| Altimeter | long needle: hundreds (once round per 1,000); short needle: thousands (once round at 30,000 ft or 10,000 m) |
| Compass | a card turning under the top mark: north (the enemy), south (home) |
| Turn-and-bank | needle: turn rate (full at 120°/s); ball: centred, thrown off in a stall or a spin |
| Climb | what the altimeter is doing (owner). Zero at 9 o'clock; tight near zero, wide at the ends, where it stops (160,000 ft/min or 800 m/s), as dives are very fast |
| Pitch | -40° to +40°, level at 3 o'clock, the Assisted auto-leveling zone marked |
| Tachometer | 600-2,400 rpm with the throttle, 0 with the engine out; a second needle on twins |
| Fuel | empty to full |
| Oil pressure | falls with engine damage, flutters when critical; a needle per engine |
| Clock | main hands: the computer's time of day; inner dial: time since the run started (owner) |
| Ammunition | drum counters per gun type (owner) |
| Bombs | a lamp per bomb still on the rack |
| Lamps | STALL, ENGINE, FUEL (below 15%), EDGE, DOWN, BLAST (your own bomb caught you) |
| Damage | a top-view wireframe of your plane, from its model: parts coloured by stage (green, yellow, orange, red), destroyed parts gone, the pilot marked |

**No radar:** 1930s fighters had none (owner).

## The message strip

Loudest first: the edge countdown, your own blast, going down, the area edge, a new wave or "enemy
down" while it lasts, and otherwise the wave, kills, your aim and auto-leveling.

## How it's built

`src/core/dashboard.*` lays out and paints the panel from a plain state, with `canvas`,
`pixel_font` (3x5) and `wireframe`; the front end (`game/dashboard_view`) uploads it. The camera
keeps your plane on screen in the shorter view. `--panel 96|80|64` and `--units` set a screenshot
up.

## Open questions

1. Later eras (WWII, Korea, Vietnam, Cold War): their panel styles and instruments (gyro horizon,
   radar, gunsight?).
2. Bombers, when they can be flown: a second set of engine gauges is ready; crew positions are not.
