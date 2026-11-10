#include "Board.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <stdexcept>

Board::Board(int rows, int columns) {
    if (rows < 4 || columns < 4 || rows > 8 || columns > 9)
        throw std::invalid_argument("Wymiary musza miescic sie w zakresie 4..8 na 4..9.");
    cells_.assign(rows, std::vector<int>(columns, 0));
}

std::vector<int> Board::legalMoves() const {
    std::vector<int> moves;
    if (result_) return moves;
    for (int column = 0; column < columns(); ++column)
        if (cells_[0][column] == 0) moves.push_back(column);
    std::stable_sort(moves.begin(), moves.end(), [this](int a, int b) {
        return std::abs(2 * a - columns() + 1) < std::abs(2 * b - columns() + 1);
    });
    return moves;
}

bool Board::play(int column) {
    if (result_ || column < 0 || column >= columns() || cells_[0][column] != 0)
        return false;
    int row = rows() - 1;
    while (cells_[row][column] != 0) --row;
    cells_[row][column] = player_;
    ++moves_;
    const std::array<std::array<int, 2>, 4> directions{{{0, 1}, {1, 0}, {1, 1}, {1, -1}}};
    for (const auto& direction : directions) {
        int count = 1;
        for (int sign : {-1, 1}) {
            int r = row + sign * direction[0];
            int c = column + sign * direction[1];
            while (r >= 0 && r < rows() && c >= 0 && c < columns() && cells_[r][c] == player_) {
                ++count;
                r += sign * direction[0];
                c += sign * direction[1];
            }
        }
        if (count >= 4) result_ = player_;
    }
    if (!result_ && moves_ == rows() * columns()) result_ = 0;
    if (!result_) player_ = 3 - player_;
    return true;
}
