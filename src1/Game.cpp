#include "Game.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

int readInt(const std::string& prompt, int minimum, int maximum) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) throw std::runtime_error("Zakonczono wejscie.");
        std::istringstream input(line);
        int value;
        char extra;
        if (input >> value && !(input >> extra) && value >= minimum && value <= maximum) return value;
        std::cout << "Wpisz liczbe od " << minimum << " do " << maximum << ".\n";
    }
}

void waitForEnter(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) throw std::runtime_error("Zakonczono wejscie.");
}

Game::Game(int rows, int columns, std::unique_ptr<Strategy> first, std::unique_ptr<Strategy> second)
    : board_(rows, columns), players_{std::move(first), std::move(second)} {}

void Game::print() const {
    for (int row = 0; row < board_.rows(); ++row) {
        for (int column = 0; column < board_.columns(); ++column)
            std::cout << ".XO"[board_.at(row, column)] << ' ';
        std::cout << '\n';
    }
    for (int column = 1; column <= board_.columns(); ++column) std::cout << column << ' ';
    std::cout << '\n';
}

void Game::run() {
    std::cout << "Gracz 1: X. Gracz 2: O.\n";
    while (!board_.result()) {
        print();
        if (players_[0] && players_[1]) waitForEnter("Nacisnij Enter, aby AI wykonalo nastepny ruch.\n");
        int player = board_.currentPlayer();
        auto& strategy = players_[player - 1];
        if (strategy) {
            SearchStats stats;
            auto column = measure(*strategy, board_, stats);
            if (!column || !board_.play(*column)) throw std::logic_error("AI zwrocilo nielegalny ruch.");
            std::cout << "Gracz " << player << ": kolumna " << *column + 1
                      << ", czas " << stats.milliseconds << " ms, stany " << stats.states << '\n';
            if (!stats.complete) std::cout << "Limit czasu. Wykonano pierwszy legalny ruch awaryjny.\n";
        } else {
            int column = readInt("Gracz " + std::to_string(player) + ", wybierz kolumne: ", 1, board_.columns());
            if (!board_.play(column - 1)) std::cout << "Ta kolumna jest pelna. Wybierz inna.\n";
        }
    }
    print();
    if (*board_.result() == 0) std::cout << "Remis.\n";
    else std::cout << "Wygrywa gracz " << *board_.result() << ".\n";
}
