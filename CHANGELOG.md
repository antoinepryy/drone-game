# Changelog

All notable changes to this project are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/), versions follow [SemVer](https://semver.org/lang/fr/).

## [Unreleased]

### Added
- LD2460 radar calibration sketch (`Radar2460Calibration/`) for the ESP32-C3 XIAO,
  built and run in Docker against a simulated radar (`SIM_FRAMES`, `SIM_TARGETS`,
  `SIM_PERIOD_MS`, `SIM_FAULTS`).
- `COMMANDES.md` beginner guide and `.vscode/tasks.json`.

### Fixed
- Radar frames are validated before parsing (length bounds, footer): a length
  field of 2 made the parser read past its 128-byte buffer.

### Changed
- Radar output uses integer arithmetic and one serial write per frame.

### Removed
- Drone Pilot C/SDL2 application (`src/`, `tests/`, `missions/`, `CMakeLists.txt`).
  Still available at tag `v0.3.0`.

## [0.3.0] - 2026-11-09

### Added
- Command scripts: `./build/drone_pilot missions/carre.txt` runs a mission file
  (one command per line, optional duration in seconds, `WAIT`, `#` comments).
- Script status box on the map (current step, progress, done / aborted).
- `P` key restarts the script; an emergency stop aborts it.
- Example missions: `missions/carre.txt` (5 m square), `missions/urgence.txt`.
- Unit tests for script parsing and execution.

## [0.2.0] - 2026-11-09

### Changed
- New cockpit-style dark UI: cards with text (altitude, battery, heading, speed, position, controls).
- Animated propellers, navigation lights, flight trail, radar sweep, state glow.
- Sharp rendering on Retina / HiDPI screens.

### Added
- Built-in 5x7 bitmap font (no extra dependency).

## [0.1.0] - 2026-11-09

### Added
- SDL2 window with a top-down view of the flight area and a landing pad.
- Drone simulation: takeoff, landing, horizontal moves, yaw, climb/descent.
- Emergency stop and automatic landing on low battery.
- Side panel: state light, altitude and battery gauges, compass.
- Unit tests for the flight logic (`ctest`).
