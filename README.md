# Wings of Freedom

A pixel-art isometric air-combat game: fly strike missions behind enemy lines, solo or in co-op.

## Getting the latest build

Every test build is published on the [releases page](https://github.com/Swooshiiroll/wings-of-freedom/releases)
(pre-releases included). The updater installs the newest one, and updates it in place the next
time. Your `tuning.txt` is kept.

- **Windows:** put `update-game.cmd` and `update-game.ps1` in the folder you want the game in, and
  double-click `update-game.cmd`. After that they're in the game folder: run it again to update.
  Options: `-Launch` starts the game, `-Force` reinstalls, `-Dir <folder>` installs elsewhere.
- **Linux / Steam Deck:** `./update-game.sh` (needs `curl` and `tar`). Options: `--launch`,
  `--force`, `--dir <folder>`.

The scripts are in `tools/` here, and inside every build from v0.1.0-alpha.5 on. It won't update
while the game is running.

## Development

How work is planned, built, tested and released: [workflow page](https://claude.ai/artifact/TvBohXytmXAzz5gBharuo3) and `CLAUDE.md`.
Board: https://github.com/users/Swooshiiroll/projects/5

```sh
tools/dev.sh setup   # once
tools/dev.sh check
tools/dev.sh test
tools/dev.sh build
```

Needs CMake 3.28+, a C++20 compiler and clang-format 22.1.3. Linux packages:
`sudo tools/linux-deps.sh`. The first `setup` downloads SDL3 and doctest (pinned by SHA256).
