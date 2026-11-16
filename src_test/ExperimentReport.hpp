#pragma once

#include "Statistics.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace experiments {
inline std::string number(double value, int precision = 3) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

template<class T>
std::string measured(const std::vector<T>& values, double value) {
    return values.empty() ? "-" : number(value, 6);
}

inline std::string columnsText(const std::vector<int>& columns) {
    std::string text;
    for (int column : columns) {
        if (!text.empty()) text += ',';
        text += std::to_string(column + 1);
    }
    return text;
}

using Row = std::vector<std::string>;
struct Table {
    std::string id;
    std::string title;
    Row columns;
    std::vector<Row> rows;
};

inline std::string integer(std::uint64_t value) { return std::to_string(value); }

inline Row resultCells(const Results& r) {
    return {integer(r.games()), integer(r.wins), integer(r.losses), integer(r.draws),
            number(r.winRate(), 2), number(percent(r.losses, r.games()), 2),
            number(percent(r.draws, r.games()), 2), measured(r.lengths, mean(r.lengths)),
            measured(r.lengths, median(r.lengths)), r.moves.count ? number(r.moves.average(), 6) : "-",
            r.games() ? number(r.averageGame(), 6) : "-", r.games() ? number(r.averageGameAi(), 6) : "-",
            integer(r.moves.count), integer(r.moves.interrupted), integer(r.moves.interrupted)};
}

inline const Row resultColumns{"Partie", "Wygrane", "Przegrane", "Remisy", "Wygrane %", "Przegrane %", "Remisy %",
    "Srednia dlugosc", "Mediana dlugosci", "Sredni ruch AI ms", "Srednia partia ms", "Suma czasu AI / partia ms",
    "Ruchy AI", "Awaryjne", "Przerwane wyszukiwania"};

inline Row joined(Row row, const Row& extra) {
    row.insert(row.end(), extra.begin(), extra.end());
    return row;
}

inline Table performance(const Study& study) {
    Table table{"pomiary", "Pomiary wedlug glebokosci",
        {"Plansza", "Otwarcie", "Kolumny otwarcia", "Metoda", "d", "Ukonczone", "Przerwane", "Status",
         "Srednia ms", "Mediana ms", "Min ms", "Max ms", "Srednia wezlow", "Mediana wezlow",
         "Ciecia ukonczone", "Ciecia przerwane"}, {}};
    for (const auto& b : study.benchmarks) {
        const auto [min, max] = std::minmax_element(b.times.begin(), b.times.end());
        table.rows.push_back({boardLabel(b.size), integer(b.plies), columnsText(b.opening), searchLabel(b.pruned),
            integer(b.depth), integer(b.times.size()), integer(b.interrupted), b.status(),
            measured(b.times, mean(b.times)), measured(b.times, median(b.times)),
            b.times.empty() ? "-" : number(*min, 6), b.times.empty() ? "-" : number(*max, 6),
            measured(b.nodes, mean(b.nodes)), measured(b.nodes, median(b.nodes)),
            integer(b.cutoffs), integer(b.interruptedCutoffs)});
    }
    return table;
}

inline Table limits(const Study& study) {
    Table table{"granice", "Granica wydajnosci",
        {"Plansza", "Otwarcie", "Metoda", "Najwieksze ukonczone d", "Ostatnie d ponizej progu",
         "d przy medianie >= 1000 ms", "d przy limicie 2000 ms", "Mediana najwiekszego d ms",
         "Ukonczone", "Przerwane", "Najwieksze d pojedynczej proby"}, {}};
    for (int size : {0, 1}) for (int plies : {0, openingPlies}) for (bool pruned : {false, true}) {
        const Benchmark* highest = nullptr;
        int good = 0, threshold = 0, timeout = 0, complete = 0, interrupted = 0, anyDepth = 0;
        for (const auto& b : study.benchmarks) {
            if (b.size != size || b.plies != plies || b.pruned != pruned) continue;
            complete += static_cast<int>(b.times.size());
            interrupted += b.interrupted;
            if (!b.times.empty()) anyDepth = b.depth;
            if (b.interrupted && !timeout) timeout = b.depth;
            if (!b.full()) continue;
            highest = &b;
            if (b.aboveThreshold() && !threshold) threshold = b.depth;
            if (!b.aboveThreshold()) good = b.depth;
        }
        if (!complete && !interrupted) continue;
        const auto depth = [](int d) { return d ? integer(d) : std::string("-"); };
        table.rows.push_back({boardLabel(size), integer(plies), searchLabel(pruned), depth(highest ? highest->depth : 0),
            depth(good), depth(threshold), depth(timeout), highest ? number(median(highest->times), 6) : "-",
            integer(complete), integer(interrupted), depth(anyDepth)});
    }
    return table;
}

inline Table efficiencies(const Study& study) {
    Table table{"ciecia", "Efekt ciec Alpha-Beta",
        {"Plansza", "Otwarcie", "d", "Redukcja wezlow %", "Przyspieszenie x", "Zgodny ruch i ocena"}, {}};
    for (const auto& c : comparisons(study)) {
        table.rows.push_back({boardLabel(c.plain->size), integer(c.plain->plies), integer(c.plain->depth),
            number(c.reduction(), 2),
            median(c.pruned->times) > 0 ? number(c.speedup(), 3) : "-",
            c.equivalent() ? "tak" : "NIE"});
    }
    return table;
}

inline Table rankings(const Study& study) {
    Table table{"ranking", "Ranking i skutecznosc strategii",
        joined({"Seria", "Plansza", "Miejsce", "Strategia"}, resultColumns), {}};
    for (bool depths : {false, true}) for (int size : {0, 1}) {
        int index = 0, place = 0, lastWins = -1, lastGames = 1;
        for (const auto& s : standings(study, size, depths)) {
            const Results r = s.result();
            ++index;
            if (r.wins * lastGames != lastWins * r.games()) place = index;
            table.rows.push_back(joined({seriesLabel(depths), boardLabel(size), integer(place), bots[s.bot].label},
                                        resultCells(r)));
            lastWins = r.wins;
            lastGames = r.games();
        }
    }
    return table;
}

inline Table headToHead(const Study& study) {
    Table table{"pojedynki", "Wyniki bezposrednich pojedynkow",
        {"Seria", "Plansza", "A", "B", "Partie", "Wygrane A", "Wygrane B", "Remisy", "Wygrane A %",
         "Wygrane B %", "Remisy %", "Srednia dlugosc", "Mediana dlugosci", "Sredni ruch AI ms",
         "Srednia partia ms", "Suma czasu AI / partia ms", "Ruchy AI", "Awaryjne", "Przerwane wyszukiwania"}, {}};
    for (const auto& m : study.matches) {
        const int n = m.games();
        if (!n) continue;
        const int a = m.role[0][0].wins + m.role[0][1].wins;
        const int b = m.role[1][0].wins + m.role[1][1].wins;
        table.rows.push_back({seriesLabel(m.depths), boardLabel(m.size), bots[m.bot[0]].label, bots[m.bot[1]].label,
            integer(n), integer(a), integer(b), integer(m.draws), number(percent(a, n), 2), number(percent(b, n), 2),
            number(percent(m.draws, n), 2), number(mean(m.lengths), 6), number(median(m.lengths), 6),
            number(m.moves.average(), 6), number(m.gameMs / n, 6), number(m.moves.milliseconds / n, 6),
            integer(m.moves.count), integer(m.moves.interrupted), integer(m.moves.interrupted)});
    }
    return table;
}

inline Table orderEffect(const Study& study) {
    Table table{"kolejnosc", "Skutecznosc jako X i O przeciw temu samemu rywalowi",
        {"Seria", "Plansza", "Strategia", "Rywal", "Partie X", "Wygrane X", "Wygrane X %",
         "Partie O", "Wygrane O", "Wygrane O %", "X minus O pp"}, {}};
    for (const auto& m : study.matches) for (int side : {0, 1}) {
        const auto& x = m.role[side][0];
        const auto& o = m.role[side][1];
        if (!x.games() && !o.games()) continue;
        table.rows.push_back({seriesLabel(m.depths), boardLabel(m.size), bots[m.bot[side]].label,
            bots[m.bot[1 - side]].label, integer(x.games()), integer(x.wins),
            x.games() ? number(x.winRate(), 2) : "-", integer(o.games()), integer(o.wins),
            o.games() ? number(o.winRate(), 2) : "-",
            x.games() && o.games() ? number(x.winRate() - o.winRate(), 2) : "-"});
    }
    return table;
}

inline Table boardEffect(const Study& study) {
    Table table{"plansze", "Porownanie plansz",
        {"Plansza", "Seria", "Partie", "Wygrane X %", "Wygrane O %", "Remisy %", "Srednia dlugosc",
         "Mediana dlugosci", "Sredni ruch AI ms", "Srednia partia ms", "Minimax d3 pusta ms",
         "Alpha-Beta d3 pusta ms", "Minimax d3 pusta wezly", "Alpha-Beta d3 pusta wezly"}, {}};
    for (int size : {0, 1}) for (bool depths : {false, true}) {
        std::vector<int> lengths;
        Moves moves;
        double totalMs = 0;
        int n = 0, x = 0, o = 0, draws = 0;
        for (const auto& m : study.matches) {
            if (m.size != size || m.depths != depths) continue;
            lengths.insert(lengths.end(), m.lengths.begin(), m.lengths.end());
            moves.add(m.moves);
            totalMs += m.gameMs;
            n += m.games();
            x += m.winsX;
            o += m.winsO;
            draws += m.draws;
        }
        const auto* plain = study.find(size, 0, 3, false);
        const auto* pruned = study.find(size, 0, 3, true);
        const auto time = [](const Benchmark* b) { return b && b->full() ? number(median(b->times), 6) : "-"; };
        const auto nodes = [](const Benchmark* b) { return b && b->full() ? number(median(b->nodes), 0) : "-"; };
        if (!n && (depths || (!plain && !pruned))) continue;
        table.rows.push_back({boardLabel(size), seriesLabel(depths), integer(n),
            n ? number(percent(x, n), 2) : "-", n ? number(percent(o, n), 2) : "-",
            n ? number(percent(draws, n), 2) : "-", measured(lengths, mean(lengths)),
            measured(lengths, median(lengths)), moves.count ? number(moves.average(), 6) : "-",
            n ? number(totalMs / n, 6) : "-", depths ? "-" : time(plain), depths ? "-" : time(pruned),
            depths ? "-" : nodes(plain), depths ? "-" : nodes(pruned)});
    }
    return table;
}

struct Overview {
    int games = 0;
    int moves = 0;
    int fallback = 0;
    int complete = 0;
    int interrupted = 0;
};

inline Overview overview(const Study& study) {
    Overview result;
    for (const auto& m : study.matches) {
        result.games += m.games();
        result.moves += m.moves.count;
        result.fallback += m.moves.interrupted;
    }
    for (const auto& b : study.benchmarks) {
        result.complete += static_cast<int>(b.times.size());
        result.interrupted += b.interrupted;
    }
    return result;
}

inline std::vector<std::string> findings(const Study& study) {
    std::vector<std::string> text;
    Comparison fastest{nullptr, nullptr};
    int disagreements = 0;
    for (const auto& c : comparisons(study)) {
        if (!c.equivalent()) ++disagreements;
        else if (median(c.pruned->times) > 0 && (!fastest.plain || c.speedup() > fastest.speedup())) fastest = c;
    }
    if (fastest.plain) {
        const auto& b = *fastest.plain;
        text.push_back("Najwieksze przyspieszenie Alpha-Beta: " + number(fastest.speedup(), 2) + "x, plansza " +
            boardLabel(b.size) + ", otwarcie " + integer(b.plies) + ", d=" + integer(b.depth) +
            ", redukcja wezlow " + number(fastest.reduction(), 2) + "%.");
    }
    if (disagreements) text.push_back("Rozbieznosci ruchu lub oceny w pelnych porownaniach: " + integer(disagreements) + ".");
    for (int size : {0, 1}) for (bool depths : {false, true}) {
        const auto rank = standings(study, size, depths);
        if (rank.empty()) continue;
        const auto best = rank.front().result();
        std::string leaders;
        for (const auto& item : rank) {
            const auto r = item.result();
            if (r.wins * best.games() != best.wins * r.games()) break;
            if (!leaders.empty()) leaders += " / ";
            leaders += std::string(bots[item.bot].label) + " (" + integer(r.wins) + "/" + integer(r.games()) + ")";
        }
        text.push_back(std::string("Lider wg odsetka wygranych, ") + seriesLabel(depths) + ", " +
                       boardLabel(size) + ": " + leaders + ", " + number(best.winRate(), 2) + "%.");
    }
    for (const auto& row : boardEffect(study).rows) {
        if (row[3] == "-") continue;
        text.push_back("Role po otwarciu, " + row[1] + ", " + row[0] + ": X " + row[3] +
                       "% wygranych, O " + row[4] + "%, remisy " + row[5] + "%.");
    }
    const auto* small = study.find(0, 0, 3, false);
    const auto* large = study.find(1, 0, 3, false);
    if (small && large && small->full() && large->full() && median(small->times) > 0) {
        text.push_back("Pusta plansza, Minimax d=3: 8 x 9 / 6 x 7 = " +
            number(median(large->times) / median(small->times), 2) + "x mediany czasu i " +
            number(median(large->nodes) / median(small->nodes), 2) +
            "x wezlow. Zmienia sie liczba kolumn i koszt stanu.");
    }
    for (const auto& row : limits(study).rows) {
        std::string message = row[0] + ", otwarcie " + row[1] + ", " + row[2] + ": ";
        if (row[5] != "-") message += "prog 1000 ms na d=" + row[5] + " (mediana " + row[7] + " ms).";
        else if (row[6] != "-") message += "limit 2000 ms na d=" + row[6] + "; pelna mediana nieustalona.";
        else if (row[3] != "-") message += "prog nieosiagniety do ukonczonego d=" + row[3] + ".";
        else continue;
        text.push_back(message);
    }
    return text;
}

inline void atomicReport(const std::filesystem::path& destination, const std::string& contents) {
    auto temp = destination;
    temp += ".tmp";
    {
        std::ofstream file(temp, std::ios::binary);
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file << contents;
        file.close();
    }
#ifdef _WIN32
    // A reader or Windows file scanner can briefly deny replacement.
    for (int attempt = 0; ; ++attempt) {
        if (MoveFileExW(temp.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) break;
        const auto error = GetLastError();
        const bool busy = error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION ||
                          error == ERROR_LOCK_VIOLATION;
        if (!busy || attempt == 9)
            throw std::system_error(static_cast<int>(error), std::system_category(), destination.string());
        std::this_thread::sleep_for(std::chrono::milliseconds(20 * (attempt + 1)));
    }
#else
    std::filesystem::rename(temp, destination);
#endif
}

inline void printTable(const Table& table, const std::vector<std::size_t>& selection) {
    if (table.rows.empty()) return;
    std::vector<std::size_t> widths;
    for (auto column : selection) {
        std::size_t width = table.columns[column].size();
        for (const auto& row : table.rows) width = std::max(width, row[column].size());
        widths.push_back(width);
    }
    auto print = [&](const Row& row) {
        for (std::size_t i = 0; i < selection.size(); ++i) {
            std::cout << std::left << std::setw(static_cast<int>(widths[i])) << row[selection[i]];
            if (i + 1 != selection.size()) std::cout << " | ";
        }
        std::cout << '\n';
    };
    print(table.columns);
    for (const auto& row : table.rows) print(row);
}

inline void terminal(const Study& study) {
    const auto o = overview(study);
    std::cout << "\n1. OGOLNE PODSUMOWANIE BADAN\n"
              << "Data: " << study.date << " | Partie: " << o.games << '/' << study.plannedGames()
              << " | Proby: " << o.complete << " ukonczonych, " << o.interrupted << " przerwanych"
              << " | Ruchy awaryjne: " << o.fallback << "\n";
    std::cout << "\n2. WYDAJNOSC MINIMAX I ALPHA-BETA\n";
    printTable(limits(study), {0,1,2,3,4,5,6,8,9});
    auto timings = performance(study);
    timings.rows.erase(std::remove_if(timings.rows.begin(), timings.rows.end(), [](const Row& row) {
        return row[4] != "1" && row[4] != "3" && row[4] != "5";
    }), timings.rows.end());
    std::cout << "\nWybrane glebokosci (pelne dane w raporcie):\n";
    printTable(timings, {0,1,3,4,5,6,8,9,10,11,13,14});
    if (study.pairs) {
        std::cout << "\n3. RANKING I SKUTECZNOSC (miejsce 1: lider, ex aequo przy remisie)\n";
        printTable(rankings(study), {0,1,2,3,4,5,6,7,8,13,14});
        std::cout << "\n4. WYNIKI POJEDYNKOW AI (w tym Alpha-Beta d=1, 3, 5)\n";
        printTable(headToHead(study), {0,1,2,3,4,5,6,7,8,9,10});
        std::cout << "\n5. WPLYW KOLEJNOSCI. X wykonuje pierwszy ruch AI po otwarciu.\n";
        printTable(orderEffect(study), {0,1,2,3,4,6,7,9,10});
        std::cout << "\n6. POROWNANIE PLANSZ\n";
        printTable(boardEffect(study), {0,1,2,3,4,5,6,7,8,9});
    }
    std::cout << "\n7. WNIOSKI\n";
    for (const auto& item : findings(study)) std::cout << "- " << item << '\n';
    std::cout << "\nPelne tabele i wykresy: raport.html. Dane: statystyki_koncowe.csv i raporty INI.\n";
    std::cout << std::right;
}

inline std::vector<Table> tables(const Study& study) {
    return {performance(study), limits(study), efficiencies(study), rankings(study),
            headToHead(study), orderEffect(study), boardEffect(study)};
}

inline void exportCsv(const Study& study) {
    std::ostringstream out;
    out << "\xEF\xBB\xBFtabela";
    const auto all = tables(study);
    Row columns;
    for (const auto& t : all) for (const auto& name : t.columns) {
        if (std::find(columns.begin(), columns.end(), name) == columns.end()) columns.push_back(name);
    }
    auto cell = [&](const std::string& value) {
        out << ";\"";
        for (const char ch : value) {
            if (ch == '"') out << '"';
            out << ch;
        }
        out << '"';
    };
    cell("Data badania");
    cell("Status badania");
    for (const auto& column : columns) cell(column);
    out << "\r\n";
    for (const auto& t : all) for (const auto& row : t.rows) {
        out << t.id;
        cell(study.date);
        cell(study.finished ? "ukonczono" : "w_toku");
        for (const auto& column : columns) {
            auto item = std::find(t.columns.begin(), t.columns.end(), column);
            cell(item == t.columns.end() ? "" : row[static_cast<std::size_t>(item - t.columns.begin())]);
        }
        out << "\r\n";
    }
    atomicReport("statystyki_koncowe.csv", out.str());
}

inline std::string htmlEscape(const std::string& value) {
    std::string text;
    for (const char ch : value) {
        switch (ch) {
        case '&': text += "&amp;"; break;
        case '<': text += "&lt;"; break;
        case '>': text += "&gt;"; break;
        case '"': text += "&quot;"; break;
        default: text += ch;
        }
    }
    return text;
}

inline void htmlTable(std::ostream& out, const Table& table) {
    if (table.rows.empty()) return;
    out << "<details" << (table.id == "granice" || table.id == "ranking" ? " open" : "") << "><summary>"
        << htmlEscape(table.title) << "</summary><div class='scroll'><table><thead><tr>";
    for (const auto& label : table.columns) out << "<th scope='col'>" << htmlEscape(label) << "</th>";
    out << "</tr></thead><tbody>";
    for (const auto& row : table.rows) {
        out << "<tr" << (table.id == "ranking" && row[2] == "1" ? " class='leader'" : "") << ">";
        for (const auto& cell : row) out << "<td>" << htmlEscape(cell) << "</td>";
        out << "</tr>";
    }
    out << "</tbody></table></div></details>";
}

struct ChartSeries {
    std::string label;
    std::vector<double> values; // wartosc ujemna oznacza brak pomiaru
};

struct ChartGroup {
    std::string label;
    Row categories;
    std::vector<ChartSeries> series;
};

inline void jsonString(std::ostream& out, const std::string& value) {
    out << '"';
    for (const char ch : value) {
        if (ch == '<') out << "\\u003c";
        else {
            if (ch == '"' || ch == '\\') out << '\\';
            out << ch;
        }
    }
    out << '"';
}

inline void chart(std::ostream& out, const std::string& title, const std::string& unit,
                  bool line, bool timeScale, const std::vector<ChartGroup>& groups) {
    out << "<figure><figcaption>" << htmlEscape(title) << "</figcaption><canvas role='img' aria-label='"
        << htmlEscape(title) << "'></canvas><div class='legend'></div><script type='application/json'>{\"unit\":";
    jsonString(out, unit);
    out << ",\"line\":" << (line ? "true" : "false")
        << ",\"log\":" << (timeScale ? "true" : "false") << ",\"groups\":[";
    bool comma = false;
    for (const auto& group : groups) {
        if (comma) out << ',';
        comma = true;
        out << "{\"label\":";
        jsonString(out, group.label);
        out << ",\"labels\":[";
        for (std::size_t i = 0; i < group.categories.size(); ++i) {
            if (i) out << ',';
            jsonString(out, group.categories[i]);
        }
        out << "],\"series\":[";
        for (std::size_t i = 0; i < group.series.size(); ++i) {
            if (i) out << ',';
            out << "{\"label\":";
            jsonString(out, group.series[i].label);
            out << ",\"values\":[";
            const auto& values = group.series[i].values;
            for (std::size_t j = 0; j < values.size(); ++j) {
                if (j) out << ',';
                if (values[j] < 0) out << "null";
                else out << std::setprecision(10) << values[j];
            }
            out << "]}";
        }
        out << "]}";
    }
    out << "]}</script></figure>";
}

inline void charts(std::ostream& out, const Study& study) {
    std::vector<ChartGroup> time, nodes, speed;
    for (int size : {0, 1}) for (int plies : {0, openingPlies}) {
        const std::string label = boardLabel(size) + (plies ? " / po otwarciu" : " / pusta");
        ChartGroup t{label, {}, {{"Minimax", {}}, {"Alpha-Beta", {}}, {"Prog 1000 ms", {}}}};
        ChartGroup n{label, {}, {{"Minimax", {}}, {"Alpha-Beta", {}}}};
        ChartGroup s{label, {}, {{"Przyspieszenie", {}}}};
        for (int depth = 1; depth <= study.maxDepth; ++depth) {
            const auto* plain = study.find(size, plies, depth, false);
            const auto* pruned = study.find(size, plies, depth, true);
            if (!plain && !pruned) break;
            t.categories.push_back(integer(depth));
            n.categories.push_back(integer(depth));
            s.categories.push_back(integer(depth));
            int i = 0;
            for (const auto* b : {plain, pruned}) {
                t.series[i].values.push_back(b && b->full() ? median(b->times) : -1);
                n.series[i].values.push_back(b && b->full() ? median(b->nodes) : -1);
                ++i;
            }
            t.series[2].values.push_back(thresholdMs);
            double ratio = -1;
            if (plain && pruned && plain->full() && pruned->full()) {
                Comparison c{plain, pruned};
                if (c.equivalent() && median(pruned->times) > 0) ratio = c.speedup();
            }
            s.series[0].values.push_back(ratio);
        }
        time.push_back(std::move(t));
        nodes.push_back(std::move(n));
        speed.push_back(std::move(s));
    }
    chart(out, "Czas wyboru ruchu wedlug glebokosci", "ms", true, true, time);
    chart(out, "Odwiedzone wezly wedlug glebokosci", "wezly", true, true, nodes);
    chart(out, "Przyspieszenie Alpha-Beta wedlug glebokosci", "x", false, false, speed);
    if (!study.pairs) return;

    std::vector<ChartGroup> wins, roles;
    for (bool depths : {false, true}) for (int size : {0, 1}) {
        const std::string label = boardLabel(size) + " / " + seriesLabel(depths);
        ChartGroup w{label, {}, {{"Wygrane", {}}, {"Przegrane", {}}, {"Remisy", {}}}};
        ChartGroup r{label, {}, {{"Jako X", {}}, {"Jako O", {}}}};
        for (const auto& s : standings(study, size, depths)) {
            const auto total = s.result();
            auto bot = bots[s.bot].label;
            w.categories.push_back(bot);
            w.series[0].values.push_back(total.winRate());
            w.series[1].values.push_back(percent(total.losses, total.games()));
            w.series[2].values.push_back(percent(total.draws, total.games()));
            r.categories.push_back(bot);
            for (int i : {0, 1}) r.series[i].values.push_back(s.role[i].games() ? s.role[i].winRate() : -1);
        }
        wins.push_back(std::move(w));
        roles.push_back(std::move(r));
    }
    chart(out, "Skutecznosc strategii i glebokosci", "%", false, false, wins);
    chart(out, "Wygrane jako X i jako O", "%", false, false, roles);

    ChartGroup a{"Mediana d=3, pusta plansza", {boardLabel(0), boardLabel(1)},
        {{"Minimax", {}}, {"Alpha-Beta", {}}}};
    for (int i : {0, 1}) for (int size : {0, 1}) {
        const auto* b = study.find(size, 0, 3, i == 1);
        a.series[i].values.push_back(b && b->full() ? median(b->times) : -1);
    }
    chart(out, "Porownanie rozmiarow plansz", "ms", false, false, {a});
}




inline void publishHtml(const Study& study) {
    std::ostringstream out;
    const auto o = overview(study);
    out << R"HTML(<!doctype html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Cztery w rzędzie — raport z eksperymentów</title>
<style>
:root{color-scheme:light;--ink:#152c39;--muted:#4a6472;--teal:#00766e;--line:#d6e2e7;--paper:#f2f6f7}
*{box-sizing:border-box}
body{margin:0;background:var(--paper);color:var(--ink);font:15px/1.55 "Segoe UI",sans-serif}
header{background:#112d3c;color:#f5fbfc;padding:3.25rem max(1.5rem,calc((100% - 1160px)/2));border-bottom:5px solid #49c8b2}
header p{color:#c5dfe5;margin:.5rem 0 0;max-width:64ch}
h1{font-size:clamp(2.1rem,6vw,3.9rem);margin:.2rem 0;line-height:1.1;letter-spacing:-.045em}
h2{font-size:1.55rem;letter-spacing:-.025em;margin:0 0 .65rem}
main{max-width:1210px;margin:auto;padding:1.5rem}
section{margin-bottom:2rem}
.badge{display:inline-block;color:#a7f3e4;border:1px solid #4f807f;padding:.25rem .65rem;border-radius:5px;font-weight:650}
.cards,.charts{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:1rem}
.cards{grid-template-columns:repeat(4,minmax(0,1fr))}
.card,figure,details,.panel{background:white;border:1px solid var(--line);border-radius:12px}
.card{padding:1rem 1.1rem;color:var(--muted)}
.card strong{display:block;font-size:1.8rem;line-height:1.25;color:var(--teal);font-variant-numeric:tabular-nums}
.panel{padding:1rem 1.25rem}
.panel ul{margin:.4rem 0;padding-left:1.3rem}
.panel li+li{margin-top:.35rem}
figure{margin:0;padding:1rem;min-width:0}
figcaption,summary{font-weight:650}
select{margin:.5rem 0;padding:.35rem;color:var(--ink);border:1px solid #92aab5;border-radius:5px;max-width:100%;background:white}
canvas{display:block;width:100%;height:235px}
.legend{display:flex;flex-wrap:wrap;gap:.25rem 1rem;color:var(--muted);font-size:13px}
.legend span::before{content:"";display:inline-block;width:.7em;height:.7em;margin-right:.4em;background:var(--swatch);border-radius:2px}
details{margin:.65rem 0;overflow:hidden}
summary{cursor:pointer;padding:.8rem 1rem}
.scroll{overflow:auto;border-top:1px solid var(--line);max-height:36rem}
table{width:100%;border-collapse:collapse;font-variant-numeric:tabular-nums;font-size:13px;white-space:nowrap}
th,td{padding:.48rem .68rem;text-align:right;border-bottom:1px solid #e5edf0}
th{position:sticky;top:0;background:#eaf2f5;color:#234453}
th:first-child,td:first-child{text-align:left}
tbody tr:nth-child(even){background:#f7fafb}
tbody tr.leader{background:#e0f4ed;color:#104b3f;font-weight:650}
.caption,footer{color:var(--muted);font-size:14px;max-width:102ch}
@media(max-width:750px){main{padding:1rem}.charts{grid-template-columns:1fr}.cards{grid-template-columns:repeat(2,minmax(0,1fr))}}
@media print{body,header{background:white;color:#15313f}header{padding:1rem 0}header p,.badge{color:#15313f}main{padding:0}figure,.panel,.cards{break-inside:avoid}.scroll{max-height:none}}
</style>
</head>
<body>
<header><span class="badge">)HTML" << (study.finished ? "Badanie zakończone" : "Badanie w toku · wyniki częściowe")
        << "</span><h1>Cztery w rzędzie</h1><p>Sprawozdanie z badań strategii AI i kosztu przeszukiwania</p>"
        << "<p><time>" << htmlEscape(study.date) << "</time> · C++17 · plansze 6 × 7 i 8 × 9</p></header><main>"
        << "<section class='cards' aria-label='Najważniejsze liczby'>"
        << "<div class='card'><strong>" << o.games << " / " << study.plannedGames() << "</strong>Rozegrane partie</div>"
        << "<div class='card'><strong>" << o.complete << "</strong>Ukończone pomiary</div>"
        << "<div class='card'><strong>" << o.interrupted << "</strong>Pomiary przerwane limitem</div>"
        << "<div class='card'><strong>" << o.fallback << "</strong>Ruchy awaryjne w turnieju</div></section>"
        << "<section><h2>Wnioski</h2><div class='panel'>";
    const auto conclusions = findings(study);
    if (conclusions.empty()) out << "<p>Wnioski pojawią się po zebraniu wyników.</p>";
    else {
        out << "<ul>";
        for (const auto& line : conclusions) out << "<li>" << htmlEscape(line) << "</li>";
        out << "</ul>";
    }
    out << "</div></section><section><h2>Wykresy eksperymentów</h2>"
        << "<p class='caption'>Czas i węzły używają skali logarytmicznej. Punkty Minimaxu i Alpha-Beta reprezentują pełne serie pięciu prób. Tabela granic pokazuje przerwania. Na wykresie ról X wykonuje pierwszy ruch AI po otwarciu.</p><div class='charts'>";
    charts(out, study);
    out << "</div></section><section><h2>Tabele wyników</h2>";
    for (const auto& table : tables(study)) htmlTable(out, table);
    out << "</section><footer><h2>Metodyka</h2><p>Maksymalna zadana głębokość: " << study.maxDepth
        << ". Pięć prób na konfigurację. Próg mediany 1000 ms, limit wyszukiwania 2000 ms. "
        << "Statystyki czasu i węzłów w tabeli pomiarów obejmują tylko ukończone próby. "
        << "Ziarno pomiarów: " << firstSeed << ". Porównywane warianty otrzymują identyczne stany.</p>";
    if (study.pairs) {
        out << "<p>Turnieje: " << study.pairs << " par na zestaw, sześć losowych ruchów otwarcia,"
            << " zamiana ról na tej samej planszy, ziarna otwarć od " << firstSeed
            << ". Seria główna: Greedy, Alpha-Beta d=3 i Monte Carlo z 30 symulacjami na legalną kolumnę."
            << " Seria głębokości: wszystkie pary Alpha-Beta d=1, 3, 5. Ranking według odsetka wygranych,"
            << " remisy pozostają w mianowniku. Czas partii obejmuje otwarcie i przebieg obliczeń bez zapisu raportu,"
            << " a długość obejmuje sześć ruchów otwarcia. Czas ruchu obejmuje rzeczywiste działania AI, także ruchy awaryjne."
            << " Na wyniki wpływają pozycje, rozmiar planszy i sprzęt. Zbiór częściowy może mieć nierówną obsadę ról.</p>";
    }
    out << R"HTML(</footer></main>
<script>
const colors = ['#00766e','#3c66aa','#a96017'];
const resize = new ResizeObserver(entries => entries.forEach(e => e.target.redraw()));
document.querySelectorAll('figure').forEach(figure => {
  const spec = JSON.parse(figure.querySelector('script').textContent);
  const canvas = figure.querySelector('canvas');
  const legend = figure.querySelector('.legend');
  const context = canvas.getContext('2d');
  const select = document.createElement('select');
  select.setAttribute('aria-label', 'Konfiguracja: ' + figure.querySelector('figcaption').textContent);
  spec.groups.forEach((group,i)=>select.add(new Option(group.label,i)));
  if(spec.groups.length>1) figure.insertBefore(select,canvas);
  const format = value => new Intl.NumberFormat('pl-PL',{
    maximumSignificantDigits:3, notation:value>=100000?'compact':'standard'
  }).format(value);
  function draw(){
    const group=spec.groups[Number(select.value)||0];
    const width=canvas.clientWidth,height=canvas.clientHeight,dpr=window.devicePixelRatio||1;
    canvas.width=Math.round(width*dpr);canvas.height=Math.round(height*dpr);
    context.setTransform(dpr,0,0,dpr,0,0);
    context.font='12px Segoe UI, sans-serif';
    legend.replaceChildren();
    group.series.forEach((series,i)=>{
      const item=document.createElement('span');item.textContent=series.label;
      item.style.setProperty('--swatch',colors[i]);legend.append(item);
    });
    const values=group.series.flatMap(series=>series.values.filter(v=>v!==null&&(!spec.log||v>0)));
    if(!values.length){
      context.fillStyle='#4a6472';context.fillText('Oczekiwanie na dane',12,36);return;
    }
    const left=60,right=width-16,top=27,bottom=height-42;
    const low=spec.log?Math.floor(Math.log10(Math.min(...values))):0;
    const high=spec.log?Math.max(low+1,Math.ceil(Math.log10(Math.max(...values)))):
      spec.unit==='%'?100:Math.max(1,Math.ceil(Math.max(...values)*100)/100);
    const y=value=>bottom-(bottom-top)*((spec.log?Math.log10(value):value)-low)/(high-low);
    const ticks=spec.log?Math.min(high-low,8):4;
    context.fillStyle='#4a6472';context.fillText(spec.unit+(spec.log?' · log':''),5,14);
    for(let i=0;i<=ticks;i++){
      const v=low+(high-low)*i/ticks;
      const value=spec.log?10**v:v;
      const yy=y(value);
      context.strokeStyle='#dbe6eb';context.beginPath();context.moveTo(left,yy);context.lineTo(right,yy);context.stroke();
      context.textAlign='right';context.fillText(format(value),left-7,yy+4);
    }
    const slot=(right-left)/group.labels.length;
    const x=i=>left+slot*(i+0.5);
    context.textAlign='center';
    const labelEvery=Math.max(1,Math.ceil(42/slot));
    group.labels.forEach((label,i)=>{
      if(i%labelEvery!==0)return;
      const lastPart=/ d=\d+$/.exec(label);
      context.fillStyle='#4a6472';
      context.fillText(lastPart?'Alpha-Beta':label,x(i),bottom+17,Math.max(18,slot-4));
      if(lastPart) context.fillText(lastPart[0].trim(),x(i),bottom+33);
    });
    group.series.forEach((series,j)=>{
      context.fillStyle=colors[j];context.strokeStyle=colors[j];context.lineWidth=2;
      if(spec.line){
        context.beginPath();
        let open=false;
        series.values.forEach((value,i)=>{
          if(value===null||(spec.log&&value<=0)){open=false;return;}
          if(open)context.lineTo(x(i),y(value));else context.moveTo(x(i),y(value));
          open=true;
        });
        context.setLineDash(j===2?[5,4]:[]);context.stroke();context.setLineDash([]);
        if(j!==2)series.values.forEach((v,i)=>{
          if(v===null||(spec.log&&v<=0))return;
          context.beginPath();context.arc(x(i),y(v),3,0,2*Math.PI);context.fill();
        });
      }else{
        const bar=Math.min(27,slot/(group.series.length+1));
        series.values.forEach((value,i)=>{
          if(value===null)return;
          const xx=x(i)+(j-group.series.length/2)*bar;
          context.fillRect(xx,y(value),bar-2,bottom-y(value));
        });
      }
    });
  }
  figure.redraw=draw;
  select.addEventListener('change',draw);
  resize.observe(figure);
  draw();
});
</script></body></html>
)HTML";
    atomicReport("raport.html", out.str());
}
}
