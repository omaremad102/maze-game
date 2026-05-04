#include "Maze.h"

#include <algorithm>
#include <queue>
#include <random>
#include <stack>

namespace {
constexpr int kDRow[4] = {-1, 0, 1, 0};
constexpr int kDCol[4] = {0, 1, 0, -1};
constexpr int kOpposite[4] = {2, 3, 0, 1};

std::vector<std::pair<int, int>> FindPathThroughCarvedMaze(
    const std::vector<std::vector<Cell>>& grid,
    int rows,
    int cols,
    std::pair<int, int> start,
    std::pair<int, int> goal) {
    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
    std::vector<std::vector<std::pair<int, int>>> parent(rows, std::vector<std::pair<int, int>>(cols, {-1, -1}));
    std::queue<std::pair<int, int>> q;
    q.push(start);
    visited[start.first][start.second] = true;

    while (!q.empty()) {
        auto [r, c] = q.front();
        q.pop();
        if (std::make_pair(r, c) == goal) {
            break;
        }

        for (int dir = 0; dir < 4; ++dir) {
            if (grid[r][c].walls[dir]) {
                continue;
            }
            int nr = r + kDRow[dir];
            int nc = c + kDCol[dir];
            if (nr < 0 || nr >= rows || nc < 0 || nc >= cols || visited[nr][nc]) {
                continue;
            }
            visited[nr][nc] = true;
            parent[nr][nc] = {r, c};
            q.push({nr, nc});
        }
    }

    if (!visited[goal.first][goal.second]) {
        return {};
    }

    std::vector<std::pair<int, int>> path;
    for (auto current = goal; current != std::make_pair(-1, -1); current = parent[current.first][current.second]) {
        path.push_back(current);
        if (current == start) {
            break;
        }
    }
    std::reverse(path.begin(), path.end());
    return path;
}
}

Maze::Maze(int rows, int cols) : rows(rows), cols(cols), grid(rows, std::vector<Cell>(cols)), goal(rows - 1, cols - 1) {}

void Maze::Generate(const MazeConfig& config) {
    rows = config.rows;
    cols = config.cols;
    goal = {rows - 1, cols - 1};
    doorCell = {-1, -1};
    grid.assign(rows, std::vector<Cell>(cols));

    std::random_device rd;
    std::mt19937 gen(rd());

    std::stack<std::pair<int, int>> st;
    grid[start.first][start.second].visited = true;
    st.push(start);

    while (!st.empty()) {
        auto [r, c] = st.top();
        auto dirs = GetUnvisitedDirections(r, c);
        if (dirs.empty()) {
            st.pop();
            continue;
        }

        std::shuffle(dirs.begin(), dirs.end(), gen);
        int dir = dirs.front();
        int nr = r + kDRow[dir];
        int nc = c + kDCol[dir];
        RemoveWall(r, c, nr, nc, dir);
        grid[nr][nc].visited = true;
        st.push({nr, nc});
    }

    for (auto& row : grid) {
        for (auto& cell : row) {
            cell.visited = false;
        }
    }

    PlaceFeatures(config);
}

bool Maze::CanMove(int row, int col, int dir, bool hasKey) const {
    if (!InBounds(row, col)) {
        return false;
    }
    if (grid[row][col].walls[dir]) {
        return false;
    }

    int nr = row + kDRow[dir];
    int nc = col + kDCol[dir];
    if (!InBounds(nr, nc)) {
        return false;
    }

    const Cell& next = grid[nr][nc];
    if (next.feature == CellFeature::Door && !hasKey) {
        return false;
    }
    return true;
}

bool Maze::InBounds(int row, int col) const {
    return row >= 0 && row < rows && col >= 0 && col < cols;
}

int Maze::GetRows() const { return rows; }
int Maze::GetCols() const { return cols; }
const Cell& Maze::GetCell(int row, int col) const { return grid[row][col]; }
Cell& Maze::GetMutableCell(int row, int col) { return grid[row][col]; }
std::pair<int, int> Maze::GetStart() const { return start; }
std::pair<int, int> Maze::GetGoal() const { return goal; }
std::pair<int, int> Maze::GetDoorCell() const { return doorCell; }

std::vector<std::pair<int, int>> Maze::GetNeighbors(int row, int col, bool hasKey) const {
    std::vector<std::pair<int, int>> neighbors;
    for (int dir = 0; dir < 4; ++dir) {
        if (CanMove(row, col, dir, hasKey)) {
            neighbors.push_back({row + kDRow[dir], col + kDCol[dir]});
        }
    }
    return neighbors;
}

std::vector<int> Maze::GetUnvisitedDirections(int row, int col) const {
    std::vector<int> dirs;
    for (int dir = 0; dir < 4; ++dir) {
        int nr = row + kDRow[dir];
        int nc = col + kDCol[dir];
        if (InBounds(nr, nc) && !grid[nr][nc].visited) {
            dirs.push_back(dir);
        }
    }
    return dirs;
}

void Maze::RemoveWall(int row, int col, int nextRow, int nextCol, int dir) {
    grid[row][col].walls[dir] = false;
    grid[nextRow][nextCol].walls[kOpposite[dir]] = false;
}

void Maze::ClearFeatures() {
    for (auto& row : grid) {
        for (auto& cell : row) {
            cell.feature = CellFeature::None;
            cell.collected = false;
        }
    }
}

void Maze::PlaceFeatures(const MazeConfig& config) {
    ClearFeatures();

    goal = {rows - 1, cols - 1};
    grid[goal.first][goal.second].feature = CellFeature::Exit;

    std::pair<int, int> keyCell{-1, -1};
    doorCell = {-1, -1};
    if (config.keyCount > 0) {
        auto path = FindPathThroughCarvedMaze(grid, rows, cols, start, goal);
        if (path.size() >= 5) {
            std::size_t keyIndex = std::max<std::size_t>(1, path.size() / 3);
            std::size_t doorIndex = std::max<std::size_t>(keyIndex + 1, (path.size() * 2) / 3);
            doorIndex = std::min<std::size_t>(doorIndex, path.size() - 2);
            keyCell = path[keyIndex];
            doorCell = path[doorIndex];
            grid[keyCell.first][keyCell.second].feature = CellFeature::Key;
            grid[doorCell.first][doorCell.second].feature = CellFeature::Door;
        }
    }

    std::vector<std::pair<int, int>> cells;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if ((r == start.first && c == start.second) ||
                (r == goal.first && c == goal.second) ||
                (r == keyCell.first && c == keyCell.second) ||
                (r == doorCell.first && c == doorCell.second)) {
                continue;
            }
            cells.push_back({r, c});
        }
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(cells.begin(), cells.end(), gen);

    std::size_t index = 0;
    if (config.keyCount > 0 && keyCell.first == -1 && index < cells.size()) {
        auto [kr, kc] = cells[index++];
        grid[kr][kc].feature = CellFeature::Key;
        keyCell = {kr, kc};
    }

    if (config.keyCount > 0 && doorCell.first == -1 && index < cells.size()) {
        auto [dr, dc] = cells[index++];
        doorCell = {dr, dc};
        grid[dr][dc].feature = CellFeature::Door;
    }

    for (int i = 0; i < config.coinCount && index < cells.size(); ++i) {
        auto [r, c] = cells[index++];
        if (grid[r][c].feature == CellFeature::None) {
            grid[r][c].feature = CellFeature::Coin;
        }
    }

    for (int i = 0; i < config.trapCount && index < cells.size(); ++i) {
        auto [r, c] = cells[index++];
        if (grid[r][c].feature == CellFeature::None) {
            grid[r][c].feature = CellFeature::Trap;
        }
    }
}
