#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd -- "$project_root"
build=false
args=()
for arg in "$@"; do
  case "$arg" in
    --build) build=true ;;
    --mute|--muted) : ;; # This game has no audio.
    --check) [[ -x "$project_root/build/clean-mines" ]] || { echo 'Missing build/clean-mines; use ./play.sh --build' >&2; exit 2; }; exit 0 ;;
    --help|-h) printf 'Usage: ./play.sh [--build] [GAME_ARGS...]
Runs the current local build. --build rebuilds from source first.
'; exit 0 ;;
    *) args+=("$arg") ;;
  esac
done
if "$build"; then
  cmake -S "$project_root" -B "$project_root/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$project_root/build" --parallel 2
fi
[[ -x "$project_root/build/clean-mines" ]] || { echo "Missing build/clean-mines; use ./play.sh --build" >&2; exit 2; }
export CLEAN_MINES_ASSET_DIR="$project_root/assets/native"
exec "$project_root/build/clean-mines" "${args[@]}"
