#include "Experiments.hpp"
#include "Strategy.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::ofstream openReport(const std::string& path, const std::string& title) {
    std::ofstream file(path);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file << std::fixed << std::setprecision(3);
    file << "[raport]\ntytul = " << title
         << "\nformat = Sekcje i nazwane pola w stylu INI\njednostka_czasu = milisekundy\n\n";
    return file;
}

std::string strategyName(const std::string& name) {
    if (name == "greedy") return "Strategia zachlanna";
    if (name == "alphabeta") return "Minimax z cieciami Alpha-Beta";
    if (name == "minimax") return "Minimax bez ciec";
    return "Monte Carlo";
}

void writeStats(std::ostream& file, const SearchStats& stats, int column) {
    file << "wybrana_kolumna = " << column + 1
         << "\nczas_ms = " << stats.milliseconds
         << "\nsprawdzone_stany = " << stats.states
         << "\nodciecia_galezi = " << stats.cutoffs
         << "\nstatus = " << (stats.complete ? "ukonczono" : "przerwano_limit_czasu")
         << "\nocena = " << (stats.complete ? std::to_string(stats.score) : "nieustalona") << "\n\n";
}

Board opening(int rows, int columns, unsigned seed, int plies) {
    Board board(rows, columns);
    std::mt19937 generator(seed);
    for (int i = 0; i < plies; ++i) {
        auto moves = board.legalMoves();
        std::uniform_int_distribution<int> pick(0, static_cast<int>(moves.size()) - 1);
        board.play(moves[pick(generator)]);
    }
    return board;
}
}

void runBenchmarks(int maxDepth) {
    if (maxDepth < 1 || maxDepth > 12) throw std::invalid_argument("Glebokosc 1..12.");
    auto file = openReport("pomiary_minimax.ini", "Czas i koszt wyboru ruchu przez Minimax");
    file << "opis = Kazda sekcja pomiaru opisuje jeden wybor ruchu.\n"
         << "powtorzenia = 5\nprog_nieakceptowalnej_mediany_ms = 1000\nlimit_wyszukiwania_ms = 2000\n"
         << "status_przerwania = Wynik nie opisuje ukonczonego wyszukiwania na zadanej glebokosci.\n"
         << "jednostka_oceny = Punkty heurystyki Minimax, wygrana ma wartosc okolo miliona.\n\n";
    int measurement = 0;
    int summaryId = 0;
    for (const auto size : {std::array<int, 2>{6, 7}, std::array<int, 2>{8, 9}}) {
        for (int plies : {0, 6}) {
            std::cout << "Pomiary: plansza " << size[0] << " x " << size[1] << ", ruchy otwarcia: " << plies << '\n';
            Board board = opening(size[0], size[1], 2026, plies);
            std::array<bool, 2> stopped{false, false};
            for (int depth = 1; depth <= maxDepth; ++depth) {
                std::array<std::vector<double>, 2> times;
                std::array<bool, 2> complete{true, true};
                for (int repeat = 1; repeat <= 5; ++repeat) {
                    for (bool pruning : {false, true}) {
                        int mode = pruning ? 1 : 0;
                        if (stopped[mode] || !complete[mode]) continue;
                        MinimaxStrategy strategy(depth, pruning);
                        SearchStats stats;
                        auto column = measure(strategy, board, stats);
                        file << "[pomiar_" << ++measurement << "]\nplansza = " << size[0] << " x " << size[1]
                             << "\npozycja = " << (plies == 0 ? "Pusta plansza" : "Po szesciu ruchach otwarcia")
                             << "\nruchy_otwarcia = " << plies << "\nziarno = 2026\nstrategia = "
                             << strategyName(pruning ? "alphabeta" : "minimax")
                             << "\nglebokosc_zadana = " << depth << "\npowtorzenie = " << repeat << '\n';
                        writeStats(file, stats, *column);
                        times[mode].push_back(stats.milliseconds);
                        if (!stats.complete) {
                            complete[mode] = false;
                        }
                    }
                }
                for (int mode = 0; mode < 2; ++mode) {
                    if (stopped[mode]) continue;
                    std::sort(times[mode].begin(), times[mode].end());
                    file << "[podsumowanie_" << ++summaryId << "]\nplansza = " << size[0] << " x " << size[1]
                         << "\nruchy_otwarcia = " << plies << "\nstrategia = "
                         << strategyName(mode == 1 ? "alphabeta" : "minimax")
                         << "\nglebokosc_zadana = " << depth << "\nliczba_prob = " << times[mode].size()
                         << "\nstatus = " << (complete[mode] ? "ukonczono" : "przerwano_limit_czasu")
                         << "\nmediana_czasu_ms = ";
                    if (complete[mode]) file << times[mode][times[mode].size() / 2];
                    else file << "nieustalona";
                    file << "\n\n";
                    stopped[mode] = !complete[mode] || times[mode][times[mode].size() / 2] >= 1000;
                }
                if (stopped[0] && stopped[1]) break;
            }
        }
    }
    file.close();
    std::cout << "Zapisano pomiary_minimax.ini. Status przerwano_limit_czasu oznacza niepelne obliczenie.\n";
}

void runDuels(int pairs) {
    if (pairs < 1 || pairs > 50) throw std::invalid_argument("Liczba par 1..50.");
    auto movesFile = openReport("turniej_ruchy.ini", "Przebieg partii turnieju AI");
    auto summary = openReport("turniej_podsumowanie.ini", "Wyniki pojedynkow trzech strategii AI");
    movesFile << "opis = Partia zawiera otwarcie, kolejne ruchy i wynik. Gracz 1 to X, gracz 2 to O.\n"
              << "glebokosc_minimax = 3\nsymulacje_monte_carlo_na_kolumne = 30\nruchy_otwarcia = 6\n"
              << "status_przerwania = Wykonano pierwszy legalny ruch awaryjny.\n"
              << "jednostka_oceny_zachlannej_i_minimax = Punkty heurystyki\n"
              << "jednostka_oceny_monte_carlo = Liczba wygranych minus liczba porazek w symulacjach\n\n";
    summary << "opis = Kazdy pojedynek obejmuje pary partii ze zmiana stron na tym samym otwarciu.\n"
            << "uwaga = Ruchy awaryjne trzeba uwzglednic przy interpretacji skutecznosci AI.\n\n";
    const std::array<std::string, 3> names{"greedy", "alphabeta", "montecarlo"};
    int game = 0;
    int matchup = 0;
    for (const auto size : {std::array<int, 2>{6, 7}, std::array<int, 2>{8, 9}}) {
        for (int a = 0; a < 3; ++a) {
            for (int b = a + 1; b < 3; ++b) {
                std::cout << "Turniej: " << size[0] << " x " << size[1] << ", "
                          << strategyName(names[a]) << " kontra " << strategyName(names[b]) << '\n';
                std::array<int, 3> results{};
                int incomplete = 0;
                for (int pair = 0; pair < pairs; ++pair) {
                    unsigned seed = 2026 + pair;
                    for (int leg = 0; leg < 2; ++leg) {
                        Board board = opening(size[0], size[1], seed, 6);
                        int aPlayer = leg == 0 ? 1 : 2;
                        auto strategyA = makeStrategy(names[a], 3, 30, seed);
                        auto strategyB = makeStrategy(names[b], 3, 30, seed + 10000);
                        movesFile << "[partia_" << ++game << "]\nplansza = " << size[0] << " x " << size[1]
                                  << "\nstrategia_a = " << strategyName(names[a]) << "\nstrategia_b = " << strategyName(names[b])
                                  << "\npara = " << pair + 1 << "\nspotkanie_w_parze = " << leg + 1
                                  << "\ngracz_strategii_a = " << aPlayer << "\nziarno_otwarcia = " << seed
                                  << "\nziarno_strategii_a = " << seed << "\nziarno_strategii_b = " << seed + 10000 << "\n\n";
                        int ply = 6;
                        while (!board.result()) {
                            int player = board.currentPlayer();
                            bool isA = player == aPlayer;
                            Strategy& strategy = isA ? *strategyA : *strategyB;
                            SearchStats stats;
                            auto column = measure(strategy, board, stats);
                            if (!column || !board.play(*column)) throw std::logic_error("Nielegalny ruch AI.");
                            ++ply;
                            if (!stats.complete) ++incomplete;
                            movesFile << "[partia_" << game << ".ruch_" << ply << "]\ngracz = " << player
                                      << "\nstrategia = " << strategyName(names[isA ? a : b]) << '\n';
                            writeStats(movesFile, stats, *column);
                        }
                        int result = *board.result();
                        ++results[result == 0 ? 2 : (result == aPlayer ? 0 : 1)];
                        movesFile << "[wynik_partii_" << game << "]\nrezultat = "
                                  << (result == 0 ? "Remis" : "Wygrana") << "\nzwyciezca = "
                                  << (result == 0 ? "Brak" : strategyName(names[result == aPlayer ? a : b]))
                                  << "\nliczba_ruchow_z_otwarciem = " << ply << "\n\n";
                    }
                }
                summary << "[pojedynek_" << ++matchup << "]\nplansza = " << size[0] << " x " << size[1]
                        << "\nstrategia_a = " << strategyName(names[a]) << "\nstrategia_b = " << strategyName(names[b])
                        << "\nliczba_par = " << pairs << "\nliczba_partii = " << 2 * pairs
                        << "\nglebokosc_minimax = 3\nsymulacje_monte_carlo_na_kolumne = 30\nruchy_otwarcia = 6"
                        << "\nwygrane_a = " << results[0] << "\nwygrane_b = " << results[1]
                        << "\nremisy = " << results[2] << "\nruchy_awaryjne = " << incomplete << "\n\n";
            }
        }
    }
    movesFile.close();
    summary.close();
    std::cout << "Zapisano turniej_ruchy.ini i turniej_podsumowanie.ini.\n";
}
