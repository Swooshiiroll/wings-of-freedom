# Wings of Freedom

A pixel-art isometric air-combat game: fly strike missions behind enemy lines, solo or in co-op.

## Development

How work is planned, built, tested and released: [workflow page]({{DOC_URL}}) and `CLAUDE.md`.
Board: {{BOARD_URL}}

```sh
tools/dev.sh setup   # once
tools/dev.sh check
tools/dev.sh test
tools/dev.sh build
```

Needs CMake 3.28+, a C++20 compiler and clang-format 22.1.3. Linux packages:
`sudo tools/linux-deps.sh`. The first `setup` downloads SDL3 and doctest (pinned by SHA256).
