#include "Experiments.hpp"
#include "Game.hpp"
#include <exception>
#include <iostream>
#include <utility>

namespace {
std::unique_ptr<Strategy> selectPlayer(int player) {
    std::cout << "Gracz " << player << ": 0 czlowiek, 1 greedy, 2 minimax, 3 alphabeta, 4 montecarlo\n";
    int choice = readInt("Wybor: ", 0, 4);
    if (choice == 0) return nullptr;
    int depth = choice == 2 || choice == 3 ? readInt("Glebokosc 1..12: ", 1, 12) : 3;
    int simulations = choice == 4 ? readInt("Symulacje na ruch 1..1000: ", 1, 1000) : 30;
    const std::string names[]{"", "greedy", "minimax", "alphabeta", "montecarlo"};
    return makeStrategy(names[choice], depth, simulations, 2026 + player);
}
}

int main() {
    try {
        while (true) {
            std::cout << "1 gra, 2 pomiary Minimax, 3 turniej AI, 0 wyjscie\n";
            int mode = readInt("Wybor: ", 0, 3);
            if (mode == 0) return 0;
            if (mode == 2) {
                std::cout << "Plik pomiary_minimax.ini w biezacym katalogu zostanie zastapiony.\n";
                runBenchmarks(readInt("Maksymalna glebokosc 1..12: ", 1, 12));
            } else if (mode == 3) {
                std::cout << "Pliki turniej_ruchy.ini i turniej_podsumowanie.ini zostana zastapione.\n";
                runDuels(readInt("Liczba par na zestaw 1..50: ", 1, 50));
            } else {
                int size = readInt("Plansza: 1 dla 6x7, 2 dla 8x9: ", 1, 2);
                auto first = selectPlayer(1);
                auto second = selectPlayer(2);
                Game game(size == 1 ? 6 : 8, size == 1 ? 7 : 9, std::move(first), std::move(second));
                game.run();
            }
            waitForEnter("\nNacisnij Enter, aby wrocic do menu.\n");
        }
    } catch (const std::exception& error) {
        std::cerr << "Blad: " << error.what() << '\n';
        return 1;
    }
}
