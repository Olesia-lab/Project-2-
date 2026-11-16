#include "Experiments.hpp"
#include "Strategy.hpp"
#include "ExperimentReport.hpp"
#include <cctype>
#include <ctime>

namespace {
using namespace experiments;

Study startStudy(int maxDepth, int pairs) {
    const auto now = std::time(nullptr);
    std::ostringstream date;
    const auto* local = std::localtime(&now);
    if (local) date << std::put_time(local, "%Y-%m-%d %H:%M:%S");
    return {maxDepth, pairs, false, date.str(), {}, {}};
}

std::ofstream openReport(const std::string& path, const std::string& title, const Study& study) {
    std::ofstream file(path);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file << std::fixed << std::setprecision(3);
    file << "[raport]\ntytul = " << title
         << "\nformat = Sekcje i nazwane pola w stylu INI\njednostka_czasu = milisekundy"
         << "\ndata = " << study.date << "\n\n";
    return file;
}

const char* benchmarkLegacy(bool pruning) {
    return pruning ? bots[1].legacyLabel : "Minimax bez ciec";
}

void writeStats(std::ostream& file, const SearchStats& stats, int column) {
    file << "wybrana_kolumna = " << column + 1
         << "\nczas_ms = " << stats.milliseconds
         << "\nczas_ms_dokladny = " << number(stats.milliseconds, 6)
         << "\nsprawdzone_stany = " << stats.states
         << "\nodciecia_galezi = " << stats.cutoffs
         << "\nstatus = " << (stats.complete ? "ukonczono" : "przerwano_limit_czasu")
         << "\nocena = " << (stats.complete ? std::to_string(stats.score) : "nieustalona") << "\n\n";
}

struct Opening {
    Board board;
    std::vector<int> columns;
};

Opening opening(int size, unsigned seed, int plies) {
    Opening position{Board(sizes[size][0], sizes[size][1]), {}};
    std::mt19937 generator(seed);
    for (int i = 0; i < plies; ++i) {
        auto moves = position.board.legalMoves();
        if (moves.empty()) throw std::logic_error("Otwarcie zakonczylo partie.");
        std::uniform_int_distribution<int> pick(0, static_cast<int>(moves.size()) - 1);
        int column = moves[pick(generator)];
        if (!position.board.play(column)) throw std::logic_error("Nielegalny ruch otwarcia.");
        position.columns.push_back(column);
    }
    return position;
}

std::string iniKey(const std::string& label) {
    std::string key;
    for (unsigned char ch : label) {
        if (std::isalnum(ch)) key += static_cast<char>(std::tolower(ch));
        else if (ch == '%') key += "pct";
        else if (ch == '/') key += "na";
        else if (ch == ' ') key += '_';
    }
    return key;
}

void writeTableIni(std::ostream& file, const Table& table) {
    int i = 0;
    for (const auto& row : table.rows) {
        file << '[' << table.id << '_' << ++i << "]\n";
        for (std::size_t column = 0; column < row.size(); ++column)
            file << iniKey(table.columns[column]) << " = " << row[column] << '\n';
        file << '\n';
    }
}

void writeBenchmark(std::ostream& file, int index, const Benchmark& b) {
    file << "[podsumowanie_" << index << "]\nplansza = " << boardLabel(b.size)
         << "\nruchy_otwarcia = " << b.plies << "\nstrategia = " << benchmarkLegacy(b.pruned)
         << "\nglebokosc_zadana = " << b.depth << "\nliczba_prob = " << b.times.size() + b.interrupted
         << "\nstatus = " << b.status() << "\nmediana_czasu_ms = "
         << (b.full() ? number(median(b.times)) : "nieustalona")
         << "\nukonczone_pomiary = " << b.times.size() << "\nprzerwane_pomiary = " << b.interrupted
         << "\nsredni_czas_ms = " << measured(b.times, mean(b.times))
         << "\nmediana_czasu_ukonczonych_ms = " << measured(b.times, median(b.times));
    const auto [min, max] = std::minmax_element(b.times.begin(), b.times.end());
    file << "\nmin_czas_ms = " << (b.times.empty() ? "-" : number(*min, 6))
         << "\nmax_czas_ms = " << (b.times.empty() ? "-" : number(*max, 6))
         << "\nsrednia_wezlow = " << measured(b.nodes, mean(b.nodes))
         << "\nmediana_wezlow = " << measured(b.nodes, median(b.nodes))
         << "\nodciecia_galezi_ukonczone = " << b.cutoffs
         << "\nodciecia_galezi_przerwane = " << b.interruptedCutoffs << "\n\n";
}

void benchmarkStudy(Study& study) {
    auto file = openReport("pomiary_minimax.ini", "Czas i koszt wyboru ruchu przez Minimax", study);
    file << "opis = Kazda sekcja pomiaru opisuje jeden wybor ruchu.\n"
         << "powtorzenia = " << repeats << "\nprog_nieakceptowalnej_mediany_ms = " << thresholdMs
         << "\nlimit_wyszukiwania_ms = " << timeoutMs
         << "\nstatus_przerwania = Wynik nie opisuje ukonczonego wyszukiwania na zadanej glebokosci.\n"
         << "jednostka_oceny = Punkty heurystyki Minimax, wygrana ma wartosc okolo miliona.\n\n";
    int measurement = 0;
    int summary = 0;
    for (int size : {0, 1}) for (int plies : {0, openingPlies}) {
        const auto position = opening(size, firstSeed, plies);
        std::cout << "Pomiary: plansza " << boardLabel(size) << ", ruchy otwarcia: " << plies << '\n';
        std::array<bool, 2> stopped{};
        for (int depth = 1; depth <= study.maxDepth; ++depth) {
            std::array<std::size_t, 2> indexes{};
            for (int mode : {0, 1}) {
                if (stopped[mode]) continue;
                indexes[mode] = study.benchmarks.size();
                study.benchmarks.push_back({size, plies, depth, mode == 1, position.columns, {}, {}});
            }
            for (int repeat = 1; repeat <= repeats; ++repeat) {
                for (int mode : {0, 1}) {
                    if (stopped[mode]) continue;
                    auto& b = study.benchmarks[indexes[mode]];
                    MinimaxStrategy strategy(depth, b.pruned, timeoutMs);
                    SearchStats stats;
                    auto column = measure(strategy, position.board, stats);
                    if (!column) throw std::logic_error("Brak ruchu w pomiarze.");
                    b.record({stats.milliseconds, stats.states, stats.cutoffs, stats.complete, *column, stats.score});
                    file << "[pomiar_" << ++measurement << "]\nplansza = " << boardLabel(size)
                         << "\npozycja = " << (plies == 0 ? "Pusta plansza" : "Po szesciu ruchach otwarcia")
                         << "\nruchy_otwarcia = " << plies << "\nziarno = " << firstSeed
                         << "\nkolumny_otwarcia = " << columnsText(position.columns)
                         << "\nstrategia = " << benchmarkLegacy(b.pruned)
                         << "\nglebokosc_zadana = " << depth << "\npowtorzenie = " << repeat << '\n';
                    writeStats(file, stats, *column);
                    if (b.full() || b.interrupted) {
                        writeBenchmark(file, ++summary, b);
                        stopped[mode] = b.interrupted || b.aboveThreshold();
                    }
                    file.flush();
                    publishHtml(study);
                }
                if (stopped[0] && stopped[1]) break;
            }
            if (stopped[0] && stopped[1]) break;
        }
    }
    writeTableIni(file, limits(study));
    writeTableIni(file, efficiencies(study));
    file.close();
}

void playGame(Study& study, Match& match, std::ofstream& log, int game, int pair, int leg) {
    const auto started = std::chrono::steady_clock::now();
    const unsigned seed = firstSeed + pair;
    auto position = opening(match.size, seed, openingPlies);
    auto& board = position.board;
    const int aPlayer = leg + 1;
    const auto& a = bots[match.bot[0]];
    const auto& b = bots[match.bot[1]];
    std::array<std::unique_ptr<Strategy>, 2> players{
        makeStrategy(a.id, a.depth, simulations, seed),
        makeStrategy(b.id, b.depth, simulations, seed + 10000)};
    std::ostringstream gameLog;
    gameLog << std::fixed << std::setprecision(3)
            << "[partia_" << game << "]\nplansza = " << boardLabel(match.size)
            << "\nstrategia_a = " << a.legacyLabel << "\nstrategia_b = " << b.legacyLabel
            << "\npara = " << pair + 1 << "\nspotkanie_w_parze = " << leg + 1
            << "\ngracz_strategii_a = " << aPlayer << "\nziarno_otwarcia = " << seed
            << "\nziarno_strategii_a = " << seed << "\nziarno_strategii_b = " << seed + 10000
            << "\nseria = " << seriesLabel(match.depths)
            << "\nkolumny_otwarcia = " << columnsText(position.columns)
            << "\nglebokosc_a = " << a.depth << "\nglebokosc_b = " << b.depth << "\n\n";
    int ply = openingPlies;
    std::array<Moves, 2> actions{};
    while (!board.result()) {
        int player = board.currentPlayer();
        int side = player == aPlayer ? 0 : 1;
        SearchStats stats;
        auto column = measure(*players[side], board, stats);
        if (!column || !board.play(*column)) throw std::logic_error("Nielegalny ruch AI.");
        actions[side].record(stats.milliseconds, stats.complete);
        gameLog << "[partia_" << game << ".ruch_" << ++ply << "]\ngracz = " << player
                << "\nstrategia = " << bots[match.bot[side]].legacyLabel << '\n';
        writeStats(gameLog, stats, *column);
    }
    const int winner = *board.result();
    const double time = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    const double aiTime = actions[0].milliseconds + actions[1].milliseconds;
    match.record(aPlayer, winner, ply, time, actions);
    gameLog << "[wynik_partii_" << game << "]\nrezultat = " << (winner ? "Wygrana" : "Remis")
            << "\nzwyciezca = " << (winner == 0 ? "Brak" : (winner == aPlayer ? a.legacyLabel : b.legacyLabel))
            << "\nliczba_ruchow_z_otwarciem = " << ply << "\nzwyciezca_gracz = " << winner
            << "\nczas_partii_ms = " << number(time, 6) << "\nsuma_czasu_ai_ms = " << number(aiTime, 6)
            << "\nruchy_awaryjne = " << actions[0].interrupted + actions[1].interrupted << "\n\n";
    log << gameLog.str();
    log.flush();
    publishHtml(study);
}

void writeMatch(std::ostream& file, const Match& match, int index, int pairs) {
    const auto a = match.strategy(0);
    const auto b = match.strategy(1);
    const int n = match.games();
    file << (match.depths ? "[pojedynek_glebokosci_" : "[pojedynek_") << index << "]\nplansza = "
         << boardLabel(match.size) << "\nstrategia_a = " << bots[match.bot[0]].legacyLabel
         << "\nstrategia_b = " << bots[match.bot[1]].legacyLabel
         << "\nliczba_par = " << pairs << "\nliczba_partii = " << n;
    if (!match.depths) file << "\nglebokosc_minimax = 3";
    file << "\nsymulacje_monte_carlo_na_kolumne = " << simulations << "\nruchy_otwarcia = " << openingPlies
         << "\nwygrane_a = " << a.wins << "\nwygrane_b = " << b.wins << "\nremisy = " << match.draws
         << "\nruchy_awaryjne = " << match.moves.interrupted
         << "\nseria = " << seriesLabel(match.depths)
         << "\nglebokosc_a = " << bots[match.bot[0]].depth << "\nglebokosc_b = " << bots[match.bot[1]].depth
         << "\nwygrane_a_pct = " << a.winRate() << "\nwygrane_b_pct = " << b.winRate()
         << "\nremisy_pct = " << percent(match.draws, n) << "\nwygrane_x = " << match.winsX
         << "\nwygrane_o = " << match.winsO << "\nsrednia_dlugosc = " << mean(match.lengths)
         << "\nmediana_dlugosci = " << median(match.lengths)
         << "\nsredni_czas_ruchu_ms = " << number(match.moves.average(), 6)
         << "\nsredni_czas_partii_ms = " << number(match.gameMs / n, 6)
         << "\nsrednia_suma_czasu_ai_ms = " << number(match.moves.milliseconds / n, 6)
         << "\nnieukonczone_wyszukiwania = " << match.moves.interrupted << "\n\n";
}

void tournamentStudy(Study& study) {
    auto log = openReport("turniej_ruchy.ini", "Przebieg partii turnieju AI", study);
    auto summary = openReport("turniej_podsumowanie.ini", "Wyniki pojedynkow trzech strategii AI", study);
    log << "opis = Partia zawiera otwarcie, kolejne ruchy i wynik. Gracz 1 to X, gracz 2 to O.\n"
        << "glebokosc_minimax = 3\nsymulacje_monte_carlo_na_kolumne = " << simulations
        << "\nruchy_otwarcia = " << openingPlies
        << "\nstatus_przerwania = Wykonano pierwszy legalny ruch awaryjny.\n"
        << "jednostka_oceny_zachlannej_i_minimax = Punkty heurystyki\n"
        << "jednostka_oceny_monte_carlo = Liczba wygranych minus liczba porazek w symulacjach\n\n";
    summary << "opis = Kazdy pojedynek obejmuje pary partii ze zmiana stron na tym samym otwarciu.\n"
            << "uwaga = Ruchy awaryjne trzeba uwzglednic przy interpretacji skutecznosci AI.\n\n";
    int game = 0;
    for (bool depths : {false, true}) {
        int index = 0;
        const auto lineup = depths ? std::array<int, 3>{3, 1, 4} : std::array<int, 3>{0, 1, 2};
        for (int size : {0, 1}) for (std::size_t a = 0; a < lineup.size(); ++a) {
            for (std::size_t b = a + 1; b < lineup.size(); ++b) {
                study.matches.push_back({size, depths, {lineup[a], lineup[b]}, {}, {}});
                auto& match = study.matches.back();
                std::cout << "Turniej " << seriesLabel(depths) << ": " << boardLabel(size) << ", "
                          << bots[lineup[a]].label << " kontra " << bots[lineup[b]].label << '\n';
                for (int pair = 0; pair < study.pairs; ++pair) {
                    for (int leg : {0, 1}) playGame(study, match, log, ++game, pair, leg);
                }
                writeMatch(summary, match, ++index, study.pairs);
                summary.flush();
            }
        }
    }
    writeTableIni(summary, rankings(study));
    writeTableIni(summary, orderEffect(study));
    writeTableIni(summary, boardEffect(study));
    const auto o = overview(study);
    summary << "[statystyki_koncowe]\nliczba_partii = " << o.games << "\nliczba_ruchow_ai = " << o.moves
            << "\nruchy_awaryjne = " << o.fallback << "\nnieukonczone_wyszukiwania = " << o.fallback << '\n';
    log.close();
    summary.close();
}

void finishStudy(Study& study) {
    study.finished = true;
    exportCsv(study);
    publishHtml(study);
    terminal(study);
}
}

void runBenchmarks(int maxDepth) {
    if (maxDepth < 1 || maxDepth > 12) throw std::invalid_argument("Glebokosc 1..12.");
    std::cout << "Pomiary zapisza takze raport.html i statystyki_koncowe.csv w biezacym katalogu.\n";
    auto study = startStudy(maxDepth, 0);
    publishHtml(study);
    benchmarkStudy(study);
    finishStudy(study);
}

void runDuels(int pairs) {
    if (pairs < 1 || pairs > 50) throw std::invalid_argument("Liczba par 1..50.");
    std::cout << "Pelny eksperyment: pomiary Minimax i turnieje AI. Zalecane co najmniej 25 par.\n"
              << "Pomiary i turnieje zastapia raporty INI, statystyki_koncowe.csv i raport.html.\n";
    auto study = startStudy(12, pairs);
    publishHtml(study);
    benchmarkStudy(study);
    tournamentStudy(study);
    finishStudy(study);
}

void runTournament(int pairs) {
    if (pairs < 1 || pairs > 50) throw std::invalid_argument("Liczba par 1..50.");
    auto study = startStudy(0, pairs);
    publishHtml(study);
    tournamentStudy(study);
    finishStudy(study);
}
