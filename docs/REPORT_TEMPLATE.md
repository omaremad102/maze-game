# MazeScape 3D Deluxe - Report Template

## 1. Project overview
MazeScape 3D Deluxe is a C++ 3D maze game designed for a data structures course. The project demonstrates how core data structures support real gameplay systems such as procedural generation, shortest-path hints, and enemy AI.

## 2. Objectives
- Build a playable 3D maze game in C++.
- Use data structures in visible gameplay features.
- Compare DFS, BFS, and A* in one project.
- Improve player experience through polished controls and UI.

## 3. Framework and tools
- Language: C++17
- Framework: raylib
- Build system: CMake
- Compiler: MSVC / Visual Studio Build Tools

## 4. Data structures used
### Stack
Used in depth-first search recursive backtracking to generate the maze.

### Queue
Used in breadth-first search to calculate the shortest path hint from player to exit.

### Priority Queue
Used in A* pathfinding for the hunter enemy on hard mode.

### 2D Vector Grid
Stores every maze cell, walls, and cell features.

## 5. Core gameplay systems
- Procedural maze generation
- Difficulty selection
- Camera-relative movement with mouse look
- BFS hint path
- A* hunter enemy
- Key and locked door mechanic
- Coins, traps, timer, score, minimap, pause menu

## 6. Algorithms
### DFS Maze Generation
The maze is generated using a stack-based backtracking algorithm that visits cells, removes walls between connected cells, and backtracks when it reaches a dead end.

### BFS Hint Solver
BFS explores the maze level by level, which guarantees the shortest path in an unweighted maze. This path is shown as a hint.

### A* Enemy Pathfinding
A* combines the actual path cost with a Manhattan-distance heuristic to chase the player more efficiently.

## 7. Player experience improvements
- Inverted movement bug fixed
- Smooth movement interpolation
- Camera-relative controls
- Difficulty tuning
- On-screen HUD and minimap
- Win and lose states with feedback

## 8. Testing plan
- Confirm maze is always solvable.
- Verify BFS path reaches exit.
- Verify A* enemy approaches player.
- Confirm locked door blocks player until a key is collected.
- Confirm traps, coins, timer, and scoring update correctly.

## 9. Future improvements
- Add textures and sound assets
- Add save/load system
- Add multiple hunters or dynamic obstacles
- Add algorithm visualization mode for DFS/BFS/A*
