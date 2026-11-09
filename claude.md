# CLAUDE.md

Drone Pilot: a C11 + SDL2 prototype for flying a simulated drone in a window.
It mostly serves as a demo project for versioning (git, tags, SemVer, changelog),
so keep history and releases clean.

## Build and test

```bash
cmake -S . -B build
cmake --build build
./build/drone_pilot                      # manual flight
./build/drone_pilot missions/carre.txt   # run a mission script
ctest --test-dir build                   # unit tests
```

Requirements: C compiler, CMake 3.16+, SDL2 (`brew install sdl2`).
Warnings are enabled (`-Wall -Wextra -Wpedantic`): a change must build without new warnings.

## Architecture

- `dronecore` (static lib, **no SDL**): `src/drone.[ch]` (flight model and state machine),
  `src/command.[ch]` (command enum, name <-> command), `src/script.[ch]` (mission parsing and execution).
  All game logic belongs here so it stays unit-testable.
- `drone_pilot` (executable): `src/main.c` (window, main loop, keyboard), `src/render.[ch]`
  (SDL drawing), `src/font.[ch]` (built-in 5x7 bitmap font, no extra dependency).
- `tests/test_drone.c`: single test binary with a small `CHECK` macro, linked against `dronecore` only.
- `missions/`: example mission scripts.

Frame order: `script_update()` is called before `drone_update()` each frame.
Drone states: `LANDED -> TAKING_OFF -> FLYING -> LANDING`, plus `EMERGENCY`.
Movement commands only act while flying; low battery (10 %) triggers auto-landing.

## Conventions

- Code, comments and commit messages in English; README in French.
- Commits follow Conventional Commits (`feat: ...`, `fix: ...`).
- Adding a command: extend the `Command` enum and the name table in `command.c`,
  handle it in `drone_apply()`, add a key in `main.c`, then update the README tables and tests.
- New logic comes with a test in `tests/test_drone.c` (no SDL in tests).

## Versioning and releases

- The version lives in **one place only**: `project(drone_pilot VERSION x.y.z)` in `CMakeLists.txt`.
  It is exposed as `APP_VERSION` (window title, test output). Never hardcode it elsewhere.
- Follow SemVer: new feature -> minor, fix -> patch (still 0.x, so breaking changes bump minor).
- Every user-visible change gets an entry under `## [Unreleased]` in `CHANGELOG.md`
  (Keep a Changelog format, English).
- Release: move `Unreleased` entries under `## [x.y.z] - YYYY-MM-DD`, bump `CMakeLists.txt`,
  commit `feat: ... (vx.y.z)` and tag `vx.y.z`.
