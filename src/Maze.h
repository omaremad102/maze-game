#pragma once

#include <utility>
#include <vector>

enum class CellFeature {
    None,
    Exit,
    Key,
    Door,
    Coin,
    Trap
};

struct Cell {
    bool visited = false;
    bool walls[4] = {true, true, true, true};
    CellFeature feature = CellFeature::None;
    bool collected = false;
};

struct MazeConfig {
    int rows = 12;
    int cols = 12;
    int keyCount = 1;
    int coinCount = 8;
    int trapCount = 3;
    bool enemyEnabled = false;
};

class Maze {
public:
    Maze(int rows, int cols);

    void Generate(const MazeConfig& config);
    bool CanMove(int row, int col, int dir, bool hasKey = true) const;
    bool InBounds(int row, int col) const;
    int GetRows() const;
    int GetCols() const;
    const Cell& GetCell(int row, int col) const;
    Cell& GetMutableCell(int row, int col);
    std::pair<int, int> GetStart() const;
    std::pair<int, int> GetGoal() const;
    std::pair<int, int> GetDoorCell() const;
    std::vector<std::pair<int, int>> GetNeighbors(int row, int col, bool hasKey = true) const;

private:
    int rows;
    int cols;
    std::vector<std::vector<Cell>> grid;
    std::pair<int, int> start{0, 0};
    std::pair<int, int> goal;
    std::pair<int, int> doorCell{-1, -1};

    std::vector<int> GetUnvisitedDirections(int row, int col) const;
    void RemoveWall(int row, int col, int nextRow, int nextCol, int dir);
    void ClearFeatures();
    void PlaceFeatures(const MazeConfig& config);
};
