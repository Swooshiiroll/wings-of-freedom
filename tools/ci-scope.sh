#!/usr/bin/env bash
# Decides whether CI's check job runs the full unit tests for a push (owner's rule: the full set of
# tests only runs when a system or object is implemented or changed). Prints "full" or "skip".
#   tools/ci-scope.sh <previous head sha, or empty>
#
# Systems and objects: the simulation core and its tests, plus anything that changes how they're
# built or checked: src/core/, tests/, cmake/, any CMakeLists.txt, .github/workflows/, tools/.
# Front-end tweaks (src/game), docs and design files skip the tests; the format checks and the
# warnings-as-errors build still run.
#
# What it compares against:
# - the push's previous head, but only if that commit's check job passed (so nothing a cancelled
#   or failed run left untested slips through);
# - otherwise where the branch left main;
# - and if neither gives a base (or the base is this commit, as on main), it says "full".
# Needs GH_TOKEN and GITHUB_REPOSITORY (set in CI) to read the previous commit's check result.
set -uo pipefail
cd "$(dirname "$0")/.."

before=${1:-}
pattern='^(src/core/|tests/|cmake/|\.github/workflows/|tools/)|(^|/)CMakeLists\.txt$'
head=$(git rev-parse HEAD)
base=""

if [[ "$before" =~ ^[0-9a-f]{40}$ && "$before" != "0000000000000000000000000000000000000000" ]] &&
   git cat-file -e "$before^{commit}" 2>/dev/null && [ -n "${GITHUB_REPOSITORY:-}" ]; then
  result=$(gh api "repos/$GITHUB_REPOSITORY/commits/$before/check-runs?check_name=check" \
             --jq '[.check_runs[].conclusion] | first // ""' 2>/dev/null || true)
  [ "$result" = "success" ] && base=$before
fi
if [ -z "$base" ]; then
  base=$(git merge-base origin/main HEAD 2>/dev/null || true)
fi
if [ -z "$base" ] || [ "$base" = "$head" ]; then
  echo full
  exit 0
fi

if git diff --name-only "$base" HEAD | grep -qE "$pattern"; then
  echo full
else
  echo skip
fi
