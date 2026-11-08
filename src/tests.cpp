#include "Strategy.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Board position(std::initializer_list<int> moves, int rows = 6, int columns = 7) {
    Board board(rows, columns);
    for (int column : moves) require(board.play(column), "Nielegalny ruch w tescie.");
    return board;
}

int main() {
    Board board;
    require(!board.play(-1) && !board.play(7), "Granice kolumn.");
    require(board.currentPlayer() == 1 && !board.result(), "Niepoprawny ruch zmienil stan.");
    require(board.play(0) && board.at(5, 0) == 1, "Pionek nie spadl na dol.");
    require(board.currentPlayer() == 2, "Zmiana gracza.");
    for (int i = 0; i < 5; ++i) require(board.play(0), "Wypelnianie kolumny.");
    require(!board.play(0) && board.currentPlayer() == 1, "Pelna kolumna zmienia stan.");
    auto legal = board.legalMoves();
    require(std::find(legal.begin(), legal.end(), 0) == legal.end(), "Pelna kolumna jest legalna.");

    Board horizontal = position({0, 6, 1, 6, 2, 5, 3});
    Board vertical = position({0, 1, 0, 1, 0, 2, 0});
    Board diagonal = position({1, 2, 0, 2, 2, 3, 3, 0, 3, 3, 0, 1, 2, 1, 1, 0}, 4, 4);
    Board reverse = position({3, 3, 0, 1, 1, 0, 0, 3, 3, 0, 2, 1, 1, 2, 2}, 4, 4);
    require(horizontal.result() == 1 && vertical.result() == 1, "Wygrana pozioma lub pionowa.");
    require(diagonal.result() == 2 && reverse.result() == 1, "Wygrana po przekatnej.");
    require(!horizontal.play(4) && horizontal.legalMoves().empty(), "Ruch po koncu gry.");
    Board draw = position({3, 2, 1, 3, 2, 2, 2, 1, 3, 3, 0, 1, 0, 1, 0, 0}, 4, 4);
    require(draw.result().has_value() && *draw.result() == 0, "Rozpoznawanie remisu.");

    Board winning = position({0, 6, 1, 6, 2, 5});
    Board blocking = position({6, 0, 6, 1, 5, 2});
    SearchStats stats;
    GreedyStrategy greedy;
    MinimaxStrategy minimax(2, true);
    require(greedy.choose(winning, stats) == 3, "Greedy pomija wygrana.");
    require(greedy.choose(blocking, stats) == 3, "Greedy nie blokuje.");
    require(minimax.choose(winning, stats) == 3, "Minimax pomija wygrana.");
    require(minimax.choose(blocking, stats) == 3, "Minimax nie blokuje.");
    require(winning.at(5, 3) == 0 && !winning.result(), "AI zmienia oryginalna plansze.");
    MonteCarloStrategy monteA(20, 2026), monteB(20, 2026);
    SearchStats first, second;
    require(monteA.choose(blocking, first) == monteB.choose(blocking, second), "Ziarno nie odtwarza ruchu.");
    require(first.states == second.states && first.score == second.score, "Niepowtarzalne symulacje.");
    for (const std::string name : {"greedy", "minimax", "alphabeta", "montecarlo"}) {
        auto strategy = makeStrategy(name, 2, 10, 2026);
        require(!strategy->choose(horizontal, stats) && !strategy->choose(draw, stats), "AI gra po koncu.");
    }

    std::mt19937 generator(2026);
    for (int test = 0; test < 30; ++test) {
        Board sample(test % 2 == 0 ? 6 : 8, test % 2 == 0 ? 7 : 9);
        for (int ply = 0; ply < test % 13 && !sample.result(); ++ply) {
            auto moves = sample.legalMoves();
            sample.play(moves[generator() % moves.size()]);
        }
        for (int depth = 1; depth <= 3; ++depth) {
            MinimaxStrategy plain(depth, false), pruned(depth, true);
            auto a = plain.choose(sample, first);
            auto b = pruned.choose(sample, second);
            require(first.complete && second.complete, "Test porownawczy przekroczyl czas.");
            require(a == b && first.score == second.score, "Alpha-Beta zmienia wynik Minimax.");
            require(second.states <= first.states, "Ciecia zwiekszaja liczbe stanow.");
        }
    }
    MinimaxStrategy limited(12, false, 1);
    auto fallback = limited.choose(Board(8, 9), stats);
    require(fallback.has_value() && !stats.complete, "Brak oznaczenia przerwanego wyszukiwania.");
    std::cout << "OK: plansza, trzy AI, 90 porownan Minimax/Alpha-Beta i limit czasu.\n";
}
