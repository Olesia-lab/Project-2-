#pragma once
#include <optional>
#include <vector>

class Board {
    std::vector<std::vector<int>> cells_;
    int player_ = 1;
    int moves_ = 0;
    std::optional<int> result_;

public:
    Board(int rows = 6, int columns = 7);
    int rows() const { return static_cast<int>(cells_.size()); }
    int columns() const { return static_cast<int>(cells_[0].size()); }
    int at(int row, int column) const { return cells_.at(row).at(column); }
    int currentPlayer() const { return player_; }
    std::optional<int> result() const { return result_; }
    std::vector<int> legalMoves() const;
    bool play(int column);
};
