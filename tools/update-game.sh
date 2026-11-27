#!/usr/bin/env bash
# Installs or updates Wings of Freedom to the newest build on GitHub (Linux and the Steam Deck).
#
#   ./update-game.sh [--dir DIR] [--force] [--launch]
#     --dir DIR   where to install (default: the folder this script is in, which is the game's own
#                 folder once it's installed: the script ships inside each build)
#     --force     reinstall even if the newest build is already installed
#     --launch    start the game afterwards
#
# Every test build is published as a GitHub release (pre-releases included). This fetches the
# newest Linux build and replaces the game's files; tuning.txt and anything else of yours is kept.
# No GitHub account is needed: the repository is public. Needs curl and tar.
set -euo pipefail

main() {
  local repo=Swooshiiroll/wings-of-freedom exe=WingsOfFreedom stamp=installed-release.txt
  local dir force=0 launch=0
  dir=$(cd "$(dirname "$0")" && pwd)
  while [ $# -gt 0 ]; do
    case "$1" in
      --dir) dir=$2; shift 2 ;;
      --force) force=1; shift ;;
      --launch) launch=1; shift ;;
      *) echo "usage: $0 [--dir DIR] [--force] [--launch]" >&2; return 2 ;;
    esac
  done
  mkdir -p "$dir"
  dir=$(cd "$dir" && pwd)

  echo "Checking for the newest build of Wings of Freedom..."
  local json tag url prerelease
  json=$(curl -fsSL -H 'User-Agent: wings-of-freedom-updater' \
    "https://api.github.com/repos/$repo/releases?per_page=10")
  # The API lists releases newest first; take the first one's tag and its Linux build.
  tag=$(printf '%s' "$json" | grep -o '"tag_name": *"[^"]*"' | head -1 | sed 's/.*"\([^"]*\)"$/\1/')
  url=$(printf '%s' "$json" | grep -o '"browser_download_url": *"[^"]*linux-x86_64\.tar\.gz"' \
    | grep "/download/$tag/" | head -1 | sed 's/.*"\(https[^"]*\)"$/\1/')
  prerelease=$(printf '%s' "$json" | grep -o '"prerelease": *[a-z]*' | head -1 | sed 's/.*: *//')
  if [ -z "$tag" ] || [ -z "$url" ]; then
    echo "No Linux build found at https://github.com/$repo/releases" >&2
    return 1
  fi

  local installed=""
  [ -f "$dir/$stamp" ] && installed=$(tr -d '[:space:]' < "$dir/$stamp")
  if [ "$installed" = "$tag" ] && [ -f "$dir/$exe" ] && [ "$force" -eq 0 ]; then
    echo "Already up to date: $installed in $dir"
  else
    if command -v pgrep >/dev/null 2>&1 && pgrep -x "$exe" >/dev/null 2>&1; then
      echo "Wings of Freedom is running. Close it, then run this again." >&2
      return 1
    fi
    echo "Installing ${installed:+$installed -> }$tag into $dir"
    local work
    work=$(mktemp -d)
    trap 'rm -rf "$work"' RETURN
    curl -fL --progress-bar -H 'User-Agent: wings-of-freedom-updater' -o "$work/build.tar.gz" "$url"
    mkdir -p "$work/x"
    tar -xzf "$work/build.tar.gz" -C "$work/x"
    # The build is one folder (wings-of-freedom/...); its contents go into the install folder.
    local source="$work/x"
    [ -f "$work/x/wings-of-freedom/$exe" ] && source="$work/x/wings-of-freedom"
    cp -Rf "$source"/. "$dir"/
    chmod +x "$dir/$exe" "$dir/update-game.sh" 2>/dev/null || true
    printf '%s\n' "$tag" > "$dir/$stamp"
    echo "Done: $tag$([ "$prerelease" = true ] && echo ' (pre-release)')"
    echo "Notes: https://github.com/$repo/releases/tag/$tag"
  fi

  if [ "$launch" -eq 1 ]; then
    (cd "$dir" && nohup "./$exe" >/dev/null 2>&1 &)
  fi
}

# Everything runs from main, on the last line: bash has read the whole script before it starts, so
# the update can safely replace this file.
main "$@"; exit $?
