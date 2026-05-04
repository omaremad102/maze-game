#pragma once

#include "Maze.h"

#include <utility>
#include <vector>

class Solver {
public:
    static std::vector<std::pair<int, int>> SolveBFS(
        const Maze& maze,
        std::pair<int, int> start,
        std::pair<int, int> goal,
        bool hasKey = true);

    static std::vector<std::pair<int, int>> SolveAStar(
        const Maze& maze,
        std::pair<int, int> start,
        std::pair<int, int> goal,
        bool hasKey = true);
};
