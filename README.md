# MazeScape 3D Deluxe

A polished C++/raylib maze game built for a data structures course.

## What changed in this upgraded version
- Fixed inverted movement. `W` now moves forward relative to the camera.
- Added smooth mouse-look and sprinting.
- Added difficulty selection menu.
- Added BFS hint system with limited hint charges.
- Added A* hunter on hard mode.
- Added keys, locked door, coins, traps, score, minimap, pause menu, win/lose screens, and cleaner HUD.

## Data structures used
- `stack` for DFS maze generation
- `queue` for BFS shortest path hints
- `priority_queue` for A* enemy pathfinding
- `vector<vector<Cell>>` grid storage
- parent grids for path reconstruction

## Controls
- `W A S D` or arrow keys: move in small steps
- `Mouse`: look around
- `Space`: jump over traps in Medium and Hard
- `Shift`: sprint
- `H`: consume one BFS hint
- `M`: toggle minimap
- `Esc`: pause / resume
- `R`: restart current difficulty
- `Backspace`: return to main menu on end screens or pause screen
- `1 / 2 / 3`: pick difficulty from menu

## Build on Windows
Open PowerShell in the project folder and run:

```powershell
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
.\build\Release\MazeScape3D.exe
```

If your machine uses VS 2022 instead, replace the generator with:

```powershell
-G "Visual Studio 17 2022"
```

## Build notes
- The project uses CMake FetchContent to download raylib on first configure.
- An internet connection is needed the first time you configure the project.
