#!/usr/bin/env bash
# Wings of Freedom developer commands, the same locally (Git Bash on Windows, any Linux shell) and in CI.
#   tools/dev.sh <command> [args]
#
#   setup               install or fetch what the project needs (once on a fresh checkout)
#   check               static checks: lint/format, version consistent, warnings-as-errors build
#   test [args]         unit tests (CI runs them inside the check job)
#   build               build into build/ (CI uploads it as the test-build artifact)
#   version <x.y.z>     set the version everywhere (VERSION and CMakeLists.txt's project() line)
#
# Logs go to out/. Exit code 0 = pass.
# WOF_CMAKE_ARGS adds arguments to the CMake configure step in setup (CI uses it for a
# core-only build, -DWOF_BUILD_GAME=OFF, and the ccache compiler launchers).
set -uo pipefail
cd "$(dirname "$0")/.."
mkdir -p out build

# --- C++20, SDL3 and CMake ---------------------------------------------------------------------
# One CMake tree in out/cmake (Release, warnings as errors), shared by check, test and build.
# Windows: Visual Studio generator (no vcvars needed). Linux: Ninja (sudo tools/linux-deps.sh).
CMAKE_DIR=out/cmake
CLANG_FORMAT_VERSION=22.1.3

is_windows() { [ "${OS:-}" = "Windows_NT" ]; }

find_tool() { # $1 = tool name; prints its path
  if command -v "$1" >/dev/null 2>&1; then command -v "$1"; return 0; fi
  if is_windows; then
    local p
    for p in "/c/Program Files/CMake/bin/$1.exe" \
             "/c/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/Llvm/x64/bin/$1.exe"; do
      [ -x "$p" ] && { echo "$p"; return 0; }
    done
  fi
  return 1
}

CMAKE=$(find_tool cmake || echo cmake)
CTEST=$(find_tool ctest || echo ctest)

configured() { [ -f "$CMAKE_DIR/CMakeCache.txt" ]; }

stack_setup() {
  local gen=(-G Ninja -DCMAKE_BUILD_TYPE=Release)
  is_windows && gen=(-A x64)
  local extra=()
  # Word-split on purpose: WOF_CMAKE_ARGS holds several -D arguments.
  read -r -a extra <<< "${WOF_CMAKE_ARGS:-}"
  "$CMAKE" -S . -B "$CMAKE_DIR" "${gen[@]}" -DWOF_WERROR=ON -DBUILD_TESTING=ON "${extra[@]}"
}

stack_check() {
  local fail=0 cf have
  echo "-- clang-format $CLANG_FORMAT_VERSION"
  if cf=$(find_tool clang-format); then
    have=$("$cf" --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
    if [ "$have" != "$CLANG_FORMAT_VERSION" ]; then
      echo "clang-format $have found; $CLANG_FORMAT_VERSION is required so local and CI agree"
      echo "  (pipx install clang-format==$CLANG_FORMAT_VERSION)"; fail=1
    else
      mapfile -t files < <(find src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) | sort)
      "$cf" --dry-run --Werror "${files[@]}" && echo "ok (${#files[@]} files)" || fail=1
    fi
  else
    echo "clang-format not found (pipx install clang-format==$CLANG_FORMAT_VERSION)"; fail=1
  fi
  echo "-- warnings-as-errors build"
  configured || stack_setup || return 1
  "$CMAKE" --build "$CMAKE_DIR" --config Release --parallel || fail=1
  return $fail
}

stack_test() {
  configured || stack_setup || return 1
  "$CMAKE" --build "$CMAKE_DIR" --config Release --parallel --target wof_tests || return 1
  "$CTEST" --test-dir "$CMAKE_DIR" -C Release --output-on-failure "$@"
}

# Leave release-ready FILES directly in build/ (an archive, a jar, an exe), not folders: the
# release workflow attaches build/* to the GitHub release.
stack_build() {
  configured || stack_setup || return 1
  "$CMAKE" --build "$CMAKE_DIR" --config Release --parallel --target wof || return 1
  local v stage=out/stage name=wings-of-freedom
  v=$(tr -d '[:space:]' < VERSION)
  rm -rf "$stage" && mkdir -p "$stage"
  "$CMAKE" --install "$CMAKE_DIR" --config Release --component game --prefix "$stage/$name" || return 1
  cp README.md CHANGELOG.md "$stage/$name/"
  rm -f build/*
  if is_windows; then
    (cd "$stage" && "$CMAKE" -E tar cf "../../build/$name-$v-windows-x64.zip" --format=zip "$name") || return 1
  else
    tar -czf "build/$name-$v-linux-$(uname -m).tar.gz" -C "$stage" "$name" || return 1
  fi
  echo "staged: $stage/$name"; ls -l build/
}

# Write version $1 into the stack's own files (package.json, *.csproj, gradle.properties...).
# Leave empty if VERSION is the only place.
stack_set_version() {
  sed -i -E "s/^(project\(wings_of_freedom VERSION )[0-9]+\.[0-9]+\.[0-9]+/\1$1/" CMakeLists.txt
}

# Print the version the stack's files carry, or nothing if VERSION is the only place.
stack_get_version() {
  sed -n -E 's/^project\(wings_of_freedom VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' CMakeLists.txt
}
# ----------------------------------------------------------------------------------------------

cmd=${1:-help}; shift || true
case "$cmd" in
  setup)
    stack_setup "$@" 2>&1 | tee out/setup.log; exit "${PIPESTATUS[0]}" ;;

  check)
    fail=0
    echo "== version consistent"
    v=$(tr -d '[:space:]' < VERSION)
    s=$(stack_get_version)
    if [ -n "$s" ] && [ "$s" != "$v" ]; then
      echo "VERSION says $v but the stack files say $s: run tools/dev.sh version $v"; fail=1
    else
      echo "ok ($v)"
    fi
    echo "== stack checks"
    stack_check "$@" 2>&1 | tee out/check.log; [ "${PIPESTATUS[0]}" -eq 0 ] || fail=1
    exit $fail ;;

  test)
    stack_test "$@" 2>&1 | tee out/test.log; exit "${PIPESTATUS[0]}" ;;

  build)
    stack_build "$@" 2>&1 | tee out/build.log; exit "${PIPESTATUS[0]}" ;;

  version)
    v=${1:-}
    [[ "$v" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "usage: tools/dev.sh version x.y.z"; exit 2; }
    echo "$v" > VERSION
    stack_set_version "$v"
    echo "version $v" ;;

  help|*)
    sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//' ;;
esac
