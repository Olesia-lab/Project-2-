#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace experiments {
constexpr int repeats = 5;
constexpr int thresholdMs = 1000;
constexpr int timeoutMs = 2000;
constexpr int openingPlies = 6;
constexpr unsigned firstSeed = 2026;
constexpr int simulations = 30;
constexpr std::array<std::array<int, 2>, 2> sizes{{{6, 7}, {8, 9}}};

template<class Number>
double mean(const std::vector<Number>& values) {
    return values.empty() ? 0 : std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

template<class Number>
double median(std::vector<Number> values) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    if (values.size() % 2) return static_cast<double>(values[middle]);
    return (static_cast<double>(values[middle - 1]) + values[middle]) / 2;
}

inline double percent(int count, int total) {
    return total ? 100.0 * count / total : 0;
}

inline std::string boardLabel(int size) {
    return std::to_string(sizes[size][0]) + " x " + std::to_string(sizes[size][1]);
}

struct SearchSample {
    double milliseconds;
    std::uint64_t nodes;
    std::uint64_t cutoffs;
    bool complete;
    int column;
    int score;
};

struct Benchmark {
    int size;
    int plies;
    int depth;
    bool pruned;
    std::vector<int> opening;
    std::vector<double> times;
    std::vector<std::uint64_t> nodes;
    std::uint64_t cutoffs = 0;
    std::uint64_t interruptedCutoffs = 0;
    int interrupted = 0;
    int column = -1;
    int score = 0;
    bool consistent = true;

    void record(const SearchSample& sample) {
        if (sample.complete) {
            if (!times.empty() && (column != sample.column || score != sample.score)) consistent = false;
            times.push_back(sample.milliseconds);
            nodes.push_back(sample.nodes);
            cutoffs += sample.cutoffs;
            column = sample.column;
            score = sample.score;
        } else {
            ++interrupted;
            interruptedCutoffs += sample.cutoffs;
        }
    }

    bool full() const { return times.size() == repeats; }
    bool aboveThreshold() const { return full() && median(times) >= thresholdMs; }
    const char* status() const {
        if (interrupted) return "przerwano_limit_czasu";
        return full() ? "ukonczono" : "w_toku";
    }
};

struct Bot {
    const char* id;
    const char* label;
    const char* legacyLabel;
    int depth;
};

constexpr std::array<Bot, 5> bots{{
    {"greedy", "Greedy", "Strategia zachlanna", 0},
    {"alphabeta", "Alpha-Beta d=3", "Minimax z cieciami Alpha-Beta", 3},
    {"montecarlo", "Monte Carlo", "Monte Carlo", 0},
    {"alphabeta", "Alpha-Beta d=1", "Minimax z cieciami Alpha-Beta (d=1)", 1},
    {"alphabeta", "Alpha-Beta d=5", "Minimax z cieciami Alpha-Beta (d=5)", 5}
}};

inline const char* seriesLabel(bool depths) { return depths ? "Glebokosci" : "Glowny"; }
inline const char* searchLabel(bool pruned) { return pruned ? "Alpha-Beta" : "Minimax"; }

struct Moves {
    int count = 0;
    int interrupted = 0;
    double milliseconds = 0;

    void record(double time, bool complete) {
        ++count;
        milliseconds += time;
        if (!complete) ++interrupted;
    }
    void add(const Moves& other) {
        count += other.count;
        interrupted += other.interrupted;
        milliseconds += other.milliseconds;
    }
    double average() const { return count ? milliseconds / count : 0; }
};

struct Results {
    int wins = 0;
    int losses = 0;
    int draws = 0;
    std::vector<int> lengths;
    double gameMs = 0;
    double gameAiMs = 0;
    Moves moves{};

    void record(int outcome, int length, double elapsed, double aiMs, const Moves& ownMoves) {
        if (outcome > 0) ++wins;
        else if (outcome < 0) ++losses;
        else ++draws;
        lengths.push_back(length);
        gameMs += elapsed;
        gameAiMs += aiMs;
        moves.add(ownMoves);
    }

    void add(const Results& other) {
        wins += other.wins;
        losses += other.losses;
        draws += other.draws;
        lengths.insert(lengths.end(), other.lengths.begin(), other.lengths.end());
        gameMs += other.gameMs;
        gameAiMs += other.gameAiMs;
        moves.add(other.moves);
    }

    int games() const { return wins + losses + draws; }
    double winRate() const { return percent(wins, games()); }
    double averageGame() const { return games() ? gameMs / games() : 0; }
    double averageGameAi() const { return games() ? gameAiMs / games() : 0; }
};

struct Match {
    int size;
    bool depths;
    std::array<int, 2> bot;
    std::array<std::array<Results, 2>, 2> role{}; // [strategia A/B][X/O]
    std::vector<int> lengths;
    int winsX = 0;
    int winsO = 0;
    int draws = 0;
    double gameMs = 0;
    Moves moves{};

    int games() const { return winsX + winsO + draws; }
    Results strategy(int side) const {
        Results result = role[side][0];
        result.add(role[side][1]);
        return result;
    }

    void record(int aPlayer, int winner, int length, double elapsed, const std::array<Moves, 2>& actions) {
        int aRole = aPlayer - 1;
        int bRole = 1 - aRole;
        int outcome = winner == 0 ? 0 : (winner == aPlayer ? 1 : -1);
        double aiMs = actions[0].milliseconds + actions[1].milliseconds;
        role[0][aRole].record(outcome, length, elapsed, aiMs, actions[0]);
        role[1][bRole].record(-outcome, length, elapsed, aiMs, actions[1]);
        lengths.push_back(length);
        gameMs += elapsed;
        moves.add(actions[0]);
        moves.add(actions[1]);
        if (winner == 1) ++winsX;
        else if (winner == 2) ++winsO;
        else ++draws;
    }
};

struct Study {
    int maxDepth;
    int pairs;
    bool finished = false;
    std::string date;
    std::vector<Benchmark> benchmarks;
    std::vector<Match> matches;

    int plannedGames() const { return pairs * 24; }
    const Benchmark* find(int size, int plies, int depth, bool pruned) const {
        auto same = [&](const Benchmark& b) {
            return b.size == size && b.plies == plies && b.depth == depth && b.pruned == pruned;
        };
        auto item = std::find_if(benchmarks.begin(), benchmarks.end(), same);
        return item == benchmarks.end() ? nullptr : &*item;
    }
};

struct Comparison {
    const Benchmark* plain;
    const Benchmark* pruned;

    bool equivalent() const {
        return plain->consistent && pruned->consistent &&
               plain->column == pruned->column && plain->score == pruned->score;
    }
    double speedup() const { return median(plain->times) / median(pruned->times); }
    double reduction() const { return 100 * (1 - mean(pruned->nodes) / mean(plain->nodes)); }
};

inline std::vector<Comparison> comparisons(const Study& study) {
    std::vector<Comparison> result;
    for (const auto& sample : study.benchmarks) {
        if (sample.pruned || !sample.full()) continue;
        const auto* pruned = study.find(sample.size, sample.plies, sample.depth, true);
        if (pruned && pruned->full()) result.push_back({&sample, pruned});
    }
    return result;
}

struct Standing {
    int bot;
    std::array<Results, 2> role;
    Results result() const {
        Results all = role[0];
        all.add(role[1]);
        return all;
    }
};

inline std::vector<Standing> standings(const Study& study, int size, bool depths) {
    std::vector<Standing> result;
    for (int bot : depths ? std::array<int, 3>{3, 1, 4} : std::array<int, 3>{0, 1, 2}) {
        Standing standing{bot, {}};
        for (const auto& match : study.matches) {
            if (match.size != size || match.depths != depths) continue;
            for (int side : {0, 1}) {
                if (match.bot[side] != bot) continue;
                standing.role[0].add(match.role[side][0]);
                standing.role[1].add(match.role[side][1]);
            }
        }
        if (standing.role[0].games() + standing.role[1].games()) result.push_back(std::move(standing));
    }
    std::stable_sort(result.begin(), result.end(), [](const Standing& a, const Standing& b) {
        const int aGames = a.role[0].games() + a.role[1].games();
        const int bGames = b.role[0].games() + b.role[1].games();
        return (a.role[0].wins + a.role[1].wins) * bGames >
               (b.role[0].wins + b.role[1].wins) * aGames;
    });
    return result;
}
}
