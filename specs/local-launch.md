# Local launch entrypoint

## Status and ownership

Implemented by executable root `play.sh`.

## Behavior

Runs the current executable `build/clean-mines` from the owning root with its asset directory and forwarded game arguments. `--build` explicitly rebuilds using the documented CMake or Odin toolchain; the default performs no build or dependency installation. `--check` checks the executable. The game has no audio; mute flags are accepted.

Playpad discovers the root without starting its runtime. Execution occurs only on a launch request. Paths resolve from the script, independently of the caller’s working directory. No remote push, publication, release export or background prewarming is part of this entrypoint.

## Verification

Shell syntax and non-starting help/preflight paths are checked. Runtime availability is separate from a successful game/desktop visual run.
