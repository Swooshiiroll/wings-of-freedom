#!/usr/bin/env bash
# Linux build dependencies (Debian/Ubuntu): compiler, CMake, Ninja, and the system libraries SDL3
# builds its video/audio/input backends against. CI runs it; on a local Debian/Ubuntu box:
#   sudo tools/linux-deps.sh
#   tools/linux-deps.sh --list    print the package names only (CI's apt cache reads this list)
# Other distros: install the same packages under their names (see SDL's docs/README-linux.md).
set -euo pipefail

PACKAGES=(
  build-essential cmake ninja-build pkg-config
  libasound2-dev libpulse-dev libpipewire-0.3-dev
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev
  libxtst-dev libxkbcommon-dev
  libwayland-dev wayland-protocols libdecor-0-dev
  libegl-dev libgl-dev libgles-dev libdrm-dev libgbm-dev
  libdbus-1-dev libudev-dev libibus-1.0-dev
)

if [ "${1:-}" = "--list" ]; then
  echo "${PACKAGES[*]}"
  exit 0
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update -q
apt-get install -y -q --no-install-recommends "${PACKAGES[@]}"
