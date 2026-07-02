# Changelog

All notable changes to Clean Mines are documented here.

## 0.1.1 - 2026-07-02

- Reworked SDL rendering to use fixed 32x32 full-cell BMP assets for flags, wrong flags, mines, and hit mines.
- Removed the old separate marker BMP fallback assets, including the question marker asset.
- Preserved the fixed-size SDL board while centering header counters, menu rows, and cell number rendering.

## 0.1.0 - 2026-07-02

- Added a native Minesweeper engine with first-click-safe board generation, reveal flood-fill, flagging, chording, win/loss state, and built-in self-tests.
- Added SDL2 graphical mode with dark mode by default, BMP tile assets, keyboard/mouse/touch controls, and an in-window menu for size, difficulty, theme, flag mode, and reset.
- Added terminal play with both a full-screen arrow-key controller and a line-command mode for scripts, pipes, and limited terminals.
- Added preset board sizes with nested difficulty levels plus custom board options.
- Added CMake install rules, a man page, MIT licensing, and GitHub Actions CI/release builds for Linux, Windows, and macOS on x64 and ARM64.
