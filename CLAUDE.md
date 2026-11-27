# Wings of Freedom: working in this repo

A pixel-art isometric air-combat game: fly strike missions behind enemy lines, solo or in co-op. Built with C++20, SDL3 3.4.18 and CMake (tests: doctest). Two people work on it: the owner, who decides and tests, and
Claude, who implements.

- Repo: https://github.com/Swooshiiroll/wings-of-freedom
- Board: https://github.com/users/Swooshiiroll/projects/5
- Workflow page: https://claude.ai/artifact/TvBohXytmXAzz5gBharuo3

## Models

- **Planning** (plan mode: design rounds, bug triage, the plan itself): Opus 5.5, xhigh effort.
- **Carrying out an approved plan:** Sonnet 5.5, high effort.

This is automatic: the owner's Claude Code user settings use `opusplan`, which switches from Opus to
Sonnet when a plan is approved, with the effort set per model. So plan in plan mode. If a session
is on the wrong model for the phase, say so; the owner switches with `/model`.

## Designing a feature first

For a **new feature or a rework** of an existing one (not bug fixes or small tweaks), design it
with the owner before writing any code.

1. **Ask questions, lots of them, in rounds.** Each round builds on the answers to the last. Keep
   going until nothing important is unclear.
   - Use multiple choice (AskUserQuestion) where there are clear options.
   - Ask open questions for creative matters: look, feel, names.
   - Never fill a gap with an assumption.

   Cover:
   - **Purpose:** what it's for, and the experience it should create.
   - **Rules and numbers:** limits, timings, defaults, how it scales.
   - **Interactions:** with the existing features, saved data and formats (a migration?), settings.
   - **UI and controls:** where it lives, feedback, what the user sees.
   - **Edge cases:** old data, empty and huge inputs, failure and offline cases.
   - **Scope:** what's out, and whether it ships in phases.
   - **Acceptance:** how we'll know it's right, and what the tests must check.
2. **Write the answers down.**
   - **Big features** (a new system, many files, a data-format change) go in `design/<feature>.md`,
     with an Open questions section.
   - **Small ones** get a spec in their issue.
   - Link the doc from the issue.
3. **Get the owner's confirmation** of the spec before creating the branch. Questions that come up
   while building go back to the owner too.
4. **Map the pull request to the spec.** Its description says which points it covers and any it
   leaves for later.

## Every change goes through a pull request

`main` is protected: it only takes pull requests whose CI checks pass. Never push to `main`.


1. **Start from an issue.** Find or create it (`gh issue list`, `gh issue create`).
   - Give it the templates' labels, and a milestone if it belongs to one.
   - Add it to the board with `--project "Wings of Freedom"`.
   - Columns: Backlog → Next → In progress → In review → Done. Move the issue as work moves
     (`gh project item-edit`, or drag it on the board).
2. **Branch:** `feat/<issue>-slug`, `fix/…`, `perf/…`, `design/…`, `docs/…` or `chore/…`.
3. **Work in small commits.**
   - **Unit tests:** every behaviour change adds or updates unit tests.
   - **Before pushing:** run `tools/dev.sh check` and `tools/dev.sh test`.
4. **Open the pull request:** `gh pr create`, using the template.
   - Put `Closes #<issue>` in it.
   - Add a line to `CHANGELOG.md` under Unreleased.
5. **Wait for CI:** `gh pr checks --watch`. Then give the owner the test-build link: the run's
   Artifacts, `wings-of-freedom-<branch>-<sha>`.
6. **Merge only when the owner says so:** `gh pr merge --squash --delete-branch`.

**Releasing:**
1. Move the Unreleased notes under a new `## [x.y.z] - date` heading.
2. Run `tools/dev.sh version x.y.z`.
3. Merge that pull request, then tag: `git tag vx.y.z && git push origin vx.y.z`.
4. The release workflow checks, tests, builds and publishes it, with that changelog section as the
   notes.

## Commands (`tools/dev.sh`, or `tools/dev.ps1` from PowerShell)

| Command | What it does |
|---|---|
| `setup` | First run on a fresh checkout (CI runs it too) |
| `check` | Format, version consistency, warnings-as-errors build (CI check `check`) |
| `test` | Unit tests (CI runs them inside `check`, see below) |
| `build` | Builds into `build/` (CI check `build`; uploaded as the test build) |
| `version x.y.z` | Sets the version everywhere (`VERSION` is the source) |

Logs go to `out/`.

## Rules

- **Never push to `main`; merge only on the owner's go-ahead.**
- **Design before code** for features and reworks (above).
- **Backups:** before editing a critical file outside this repo, copy it to
  `D:\<Device>\<DriveLetter>\<path>`.

## Project-specific rules

- **CI has two required checks, `check` and `build`** (no separate `unit-tests` check). `check`
  runs the unit tests only when a system or object changed (`tools/ci-scope.sh`); `build` covers the
  Linux and Windows builds.
- **`src/core` has no SDL and no I/O.** The simulation lives there and is all the tests link, so a
  host or headless server can run it for co-op later. The SDL3 front end is `src/game`.
- **Players drive planes through per-tick input commands**, never by the front end changing state
  directly, and nothing may assume there is only one player.
- **No platform-specific code** outside SDL: Windows and Linux now, Android kept possible.
- **Gameplay numbers are (start) values** in a live-reloaded tuning file, tuned by playtest.
- **Original art only.** Ultimate Air Combat is a style reference; nothing is copied from it.
- **Pixel-perfect view:** the world is drawn 1:1 in art pixels into a render target the size of the
  view, and shown at a whole-number scale (`core/view.hpp`). Never draw at a fractional scale.
  The pure maths (projection, view fit, camera) stays in `src/core`.
- **Ground generation is integer-only** (hashes, no floats), so a seed gives the same farmland on
  MSVC and GCC, as co-op clients will generate it themselves. The golden hash in
  `tests/test_ground.cpp` pins it on both. If you change the generator on purpose, update the hash
  in the same pull request and say why.
- **Smoke run:** `WingsOfFreedom --shot out/shot.png` flies 120 ticks with no input, saves the view
  as a PNG and exits. `--window`, `--nation us|de`, `--heading`, `--height`, `--pitch`,
  `--throttle`, `--at`, `--overlay`, `--wave N` and `--targets` set up the shot (the first wave
  arrives at 10 s, so `--ticks 600` or more shows enemies). `--sprite-sheet out/sheet.png` saves every
  baked pose of the `--nation`'s fighter, at `--sprite-scale PCT`. Shots and sheets skip the
  nation screen. `--menu pause|options|controls` shows a menu page over the shot; shots use the
  default settings, not `settings.txt`. `--panel 96|80|64` and `--units nation|imperial|metric`
  set the dashboard up; the cockpit clock reads 10:10 in a shot.
- **Depth and pixel-perfect:** everything is sized by its depth from the player's plane
  (`core/view_xf.hpp`). The ground below is drawn at full size into its own target and shrunk
  smoothly (the one exception to pixel-perfect). Planes are never stretched: each size step is
  baked as pixel art, in the background, and only until it's ready is the nearest one stretched.
- **`std::thread` (the sprite baker) uses only the pure core functions;** SDL textures are made
  on the main thread.
