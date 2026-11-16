#pragma once
#include "Board.hpp"
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>

struct SearchStats {
    std::uint64_t states = 0;
    std::uint64_t cutoffs = 0;
    double milliseconds = 0;
    bool complete = true;
    int score = 0;
};

class Strategy {
public:
    virtual ~Strategy() = default;
    virtual std::optional<int> choose(const Board& board, SearchStats& stats) = 0;
};

class GreedyStrategy : public Strategy {
public:
    std::optional<int> choose(const Board& board, SearchStats& stats) override;
};

class MinimaxStrategy : public Strategy {
    int depth_;
    bool pruning_;
    int limitMs_;
    int rootPlayer_ = 1;
    std::chrono::steady_clock::time_point deadline_;
    int search(const Board& board, int depth, int alpha, int beta, SearchStats& stats);

public:
    MinimaxStrategy(int depth, bool pruning, int limitMs = 2000);
    std::optional<int> choose(const Board& board, SearchStats& stats) override;
};

class MonteCarloStrategy : public Strategy {
    int simulations_;
    std::mt19937 generator_;

public:
    MonteCarloStrategy(int simulations, unsigned seed);
    std::optional<int> choose(const Board& board, SearchStats& stats) override;
};

std::unique_ptr<Strategy> makeStrategy(const std::string& name, int depth, int simulations, unsigned seed);
std::optional<int> measure(Strategy& strategy, const Board& board, SearchStats& stats);
