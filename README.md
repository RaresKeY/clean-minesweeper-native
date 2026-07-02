# Clean Mines

A tiny native Minesweeper game with both graphical and terminal modes.

No browser and no web UI. The graphical mode uses SDL2 and small BMP assets. Terminal mode can be built without SDL2.

## Gameplay

Clean Mines is a local native Minesweeper game. The first reveal is safe, empty areas flood open, numbered cells show adjacent mine counts, flags mark suspected mines, and chording a revealed number opens its surrounding cells when the flag count matches.

The SDL graphical mode is the default when SDL2 support is built in. It opens in dark mode by default and includes an in-window `Menu` button for board size, difficulty, theme, touch flag mode, and reset.

Preset boards are split into size levels and mine difficulty:

- `small`: 9x9 with easy, normal, or hard mine counts
- `medium`: 16x16 with easy, normal, or hard mine counts
- `large`: 30x16 with easy, normal, or hard mine counts

Custom boards are available with `--width`, `--height`, and `--mines`.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Executable:

- Linux/macOS: `build/clean-mines`
- Windows: `build/Release/clean-mines.exe`

## Run

```bash
./build/clean-mines
```

The default mode is graphical when SDL2 support is built in. The graphical theme defaults to dark; use `--light` for the light theme.

Options:

```bash
clean-mines --graphics
clean-mines --terminal
clean-mines --commands
clean-mines --level medium --difficulty hard
clean-mines --dark
clean-mines --light
clean-mines --theme dark
clean-mines --width 16 --height 16 --mines 40
clean-mines --seed 12345
clean-mines --self-test
clean-mines --version
```

Graphical controls:

- Left click or `Space`/`Enter` reveals
- Right click or `f` flags
- `c` chords a revealed number
- Arrow keys move keyboard focus
- The `Menu` button opens nested size, difficulty, and theme choices plus flag mode and reset
- Touchscreen users can enable `Flag On` in the menu to place flags with taps
- `Tab` or `o` opens the menu; arrows move menu focus; `Enter` selects; `Esc` closes the menu
- `t` toggles light and dark themes
- `r` restarts the current board
- `1`/`s`/`b`, `2`/`m`/`i`, `3`/`l`/`x` select small, medium, or large size
- `e`, `n`, `h` select easy, normal, or hard difficulty inside the current size
- `Esc` or `q` quits

Full-screen terminal controls:

- Arrow keys move the flashing focus cell
- `Space`/`Enter` reveals
- `f` flags
- `c` chords a revealed number
- `?` toggles help
- `r` restarts the current board
- `1`/`s`/`b`, `2`/`m`/`i`, `3`/`l`/`x` select small, medium, or large size
- `e`, `n`, `h` select easy, normal, or hard difficulty inside the current size
- `Esc` or `q` quits

Line-command terminal mode is used automatically when stdin/stdout are not TTYs, and can be forced with `--commands`.

The full-screen terminal mode redraws on the alternate screen and is controlled directly with keys. The line-command terminal mode is kept for scripts, pipes, and very limited terminals.

Line-command terminal commands:

- `r x y` reveal a cell
- `f x y` flag or unflag a cell
- `c x y` chord a revealed number
- `n` restarts the current board
- `n s h` starts a new board by size and difficulty
- `s m` selects medium size with the current difficulty
- `d e` selects easy difficulty with the current size
- `h` show help
- `q` quit

Coordinates are 1-based.

Preset sizes are `small`, `medium`, and `large`; short names are `s`, `m`, and `l`.
Preset difficulties are `easy`, `normal`, and `hard`; short names are `e`, `n`, and `h`.
The old `n b`, `n i`, and `n e` terminal shortcuts still start small-normal, medium-normal, and large-normal boards.

## Terminal-only Build

```bash
cmake -S . -B build-terminal -DCLEAN_MINES_WITH_SDL=OFF
cmake --build build-terminal
./build-terminal/clean-mines --terminal
```

## CI

GitHub Actions builds the graphical SDL2 binary and runs the self-test on Linux, Windows, and macOS across x64 and ARM64 hosted runners.

Release tags named `v*` build native archives and attach them to a GitHub Release.

## License

MIT.
