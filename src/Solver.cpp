#include "Solver.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <vector>

namespace {
constexpr int kDRow[4] = {-1, 0, 1, 0};
constexpr int kDCol[4] = {0, 1, 0, -1};

int Heuristic(std::pair<int, int> a, std::pair<int, int> b) {
    return std::abs(a.first - b.first) + std::abs(a.second - b.second);
}

std::vector<std::pair<int, int>> ReconstructPath(
    const std::vector<std::vector<std::pair<int, int>>>& parent,
    std::pair<int, int> start,
    std::pair<int, int> goal) {
    std::vector<std::pair<int, int>> path;
    auto current = goal;
    while (current != std::make_pair(-1, -1)) {
        path.push_back(current);
        if (current == start) {
            break;
        }
        current = parent[current.first][current.second];
    }
    std::reverse(path.begin(), path.end());
    if (path.empty() || path.front() != start) {
        return {};
    }
    return path;
}
}

std::vector<std::pair<int, int>> Solver::SolveBFS(
    const Maze& maze,
    std::pair<int, int> start,
    std::pair<int, int> goal,
    bool hasKey) {
    const int rows = maze.GetRows();
    const int cols = maze.GetCols();

    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
    std::vector<std::vector<std::pair<int, int>>> parent(rows, std::vector<std::pair<int, int>>(cols, {-1, -1}));

    std::queue<std::pair<int, int>> q;
    q.push(start);
    visited[start.first][start.second] = true;

    while (!q.empty()) {
        auto [r, c] = q.front();
        q.pop();

        if (std::make_pair(r, c) == goal) {
            return ReconstructPath(parent, start, goal);
        }

        for (int dir = 0; dir < 4; ++dir) {
            if (!maze.CanMove(r, c, dir, hasKey)) {
                continue;
            }
            int nr = r + kDRow[dir];
            int nc = c + kDCol[dir];
            if (!maze.InBounds(nr, nc) || visited[nr][nc]) {
                continue;
            }
            visited[nr][nc] = true;
            parent[nr][nc] = {r, c};
            q.push({nr, nc});
        }
    }

    return {};
}

std::vector<std::pair<int, int>> Solver::SolveAStar(
    const Maze& maze,
    std::pair<int, int> start,
    std::pair<int, int> goal,
    bool hasKey) {
    const int rows = maze.GetRows();
    const int cols = maze.GetCols();

    struct Node {
        int fScore;
        int gScore;
        std::pair<int, int> cell;
        bool operator<(const Node& other) const {
            return fScore > other.fScore;
        }
    };

    std::priority_queue<Node> open;
    std::vector<std::vector<int>> gScore(rows, std::vector<int>(cols, 1'000'000));
    std::vector<std::vector<bool>> closed(rows, std::vector<bool>(cols, false));
    std::vector<std::vector<std::pair<int, int>>> parent(rows, std::vector<std::pair<int, int>>(cols, {-1, -1}));

    gScore[start.first][start.second] = 0;
    open.push({Heuristic(start, goal), 0, start});

    while (!open.empty()) {
        Node current = open.top();
        open.pop();

        auto [r, c] = current.cell;
        if (closed[r][c]) {
            continue;
        }
        closed[r][c] = true;

        if (current.cell == goal) {
            return ReconstructPath(parent, start, goal);
        }

        for (int dir = 0; dir < 4; ++dir) {
            if (!maze.CanMove(r, c, dir, hasKey)) {
                continue;
            }
            int nr = r + kDRow[dir];
            int nc = c + kDCol[dir];
            if (!maze.InBounds(nr, nc) || closed[nr][nc]) {
                continue;
            }
            int tentative = gScore[r][c] + 1;
            if (tentative < gScore[nr][nc]) {
                gScore[nr][nc] = tentative;
                parent[nr][nc] = {r, c};
                int f = tentative + Heuristic({nr, nc}, goal);
                open.push({f, tentative, {nr, nc}});
            }
        }
    }

    return {};
}
