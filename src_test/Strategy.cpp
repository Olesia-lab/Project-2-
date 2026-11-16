#include "Strategy.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace {
constexpr int winScore = 1000000;
constexpr int infinity = 2000000;

int evaluate(const Board& board, int player) {
    const std::array<int, 5> weights{0, 1, 10, 100, winScore};
    const std::array<std::array<int, 2>, 4> directions{{{0, 1}, {1, 0}, {1, 1}, {1, -1}}};
    int score = 0;
    for (int row = 0; row < board.rows(); ++row) {
        for (int column = 0; column < board.columns(); ++column) {
            for (const auto& direction : directions) {
                int endRow = row + 3 * direction[0];
                int endColumn = column + 3 * direction[1];
                if (endRow >= board.rows() || endColumn < 0 || endColumn >= board.columns()) continue;
                int own = 0;
                int enemy = 0;
                for (int i = 0; i < 4; ++i) {
                    int cell = board.at(row + i * direction[0], column + i * direction[1]);
                    if (cell == player) ++own;
                    else if (cell != 0) ++enemy;
                }
                if (enemy == 0) score += weights[own];
                if (own == 0) score -= weights[enemy];
            }
        }
    }
    return score;
}

int terminalScore(int result, int player, int depth) {
    if (result == 0) return 0;
    return result == player ? winScore + depth : -winScore - depth;
}
}

std::optional<int> GreedyStrategy::choose(const Board& board, SearchStats& stats) {
    stats = {};
    stats.states = 1;
    std::optional<int> best;
    int bestScore = -infinity;
    int player = board.currentPlayer();
    for (int column : board.legalMoves()) {
        Board child = board;
        child.play(column);
        ++stats.states;
        int score = child.result() ? terminalScore(*child.result(), player, 0) : evaluate(child, player);
        if (!child.result()) {
            for (int reply : child.legalMoves()) {
                Board response = child;
                response.play(reply);
                ++stats.states;
                if (response.result() && *response.result() == 3 - player) score = -winScore;
            }
        }
        if (score > bestScore) {
            bestScore = score;
            best = column;
        }
    }
    stats.score = best ? bestScore : 0;
    return best;
}

MinimaxStrategy::MinimaxStrategy(int depth, bool pruning, int limitMs)
    : depth_(depth), pruning_(pruning), limitMs_(limitMs) {
    if (depth < 1 || depth > 12 || limitMs < 1)
        throw std::invalid_argument("Glebokosc 1..12, limit czasu co najmniej 1 ms.");
}

int MinimaxStrategy::search(const Board& board, int depth, int alpha, int beta, SearchStats& stats) {
    ++stats.states;
    if (std::chrono::steady_clock::now() >= deadline_) {
        stats.complete = false;
        return 0;
    }
    if (board.result()) return terminalScore(*board.result(), rootPlayer_, depth);
    if (depth == 0) return evaluate(board, rootPlayer_);
    bool maximizing = board.currentPlayer() == rootPlayer_;
    int value = maximizing ? -infinity : infinity;
    for (int column : board.legalMoves()) {
        Board child = board;
        child.play(column);
        int score = search(child, depth - 1, alpha, beta, stats);
        if (!stats.complete) return 0;
        if (maximizing) {
            value = std::max(value, score);
            alpha = std::max(alpha, value);
        } else {
            value = std::min(value, score);
            beta = std::min(beta, value);
        }
        if (pruning_ && alpha >= beta) {
            ++stats.cutoffs;
            break;
        }
    }
    return value;
}

std::optional<int> MinimaxStrategy::choose(const Board& board, SearchStats& stats) {
    stats = {};
    stats.states = 1;
    auto moves = board.legalMoves();
    if (moves.empty()) return std::nullopt;
    deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(limitMs_);
    rootPlayer_ = board.currentPlayer();
    int bestScore = -infinity;
    int alpha = -infinity;
    int best = moves.front();
    for (int column : moves) {
        Board child = board;
        child.play(column);
        int score = search(child, depth_ - 1, alpha, infinity, stats);
        if (!stats.complete) {
            stats.score = 0;
            return moves.front();
        }
        if (score > bestScore) {
            bestScore = score;
            best = column;
        }
        if (pruning_) alpha = std::max(alpha, bestScore);
    }
    stats.score = bestScore;
    return best;
}

MonteCarloStrategy::MonteCarloStrategy(int simulations, unsigned seed)
    : simulations_(simulations), generator_(seed) {
    if (simulations < 1) throw std::invalid_argument("Liczba symulacji musi byc dodatnia.");
}

std::optional<int> MonteCarloStrategy::choose(const Board& board, SearchStats& stats) {
    stats = {};
    stats.states = 1;
    std::optional<int> best;
    int bestScore = -simulations_ - 1;
    int player = board.currentPlayer();
    for (int column : board.legalMoves()) {
        int score = 0;
        for (int simulation = 0; simulation < simulations_; ++simulation) {
            Board trial = board;
            trial.play(column);
            ++stats.states;
            while (!trial.result()) {
                auto moves = trial.legalMoves();
                std::uniform_int_distribution<int> pick(0, static_cast<int>(moves.size()) - 1);
                trial.play(moves[pick(generator_)]);
                ++stats.states;
            }
            if (*trial.result() == player) ++score;
            else if (*trial.result() != 0) --score;
        }
        if (score > bestScore) {
            bestScore = score;
            best = column;
        }
    }
    stats.score = best ? bestScore : 0;
    return best;
}

std::unique_ptr<Strategy> makeStrategy(const std::string& name, int depth, int simulations, unsigned seed) {
    if (name == "greedy") return std::make_unique<GreedyStrategy>();
    if (name == "minimax") return std::make_unique<MinimaxStrategy>(depth, false);
    if (name == "alphabeta") return std::make_unique<MinimaxStrategy>(depth, true);
    if (name == "montecarlo") return std::make_unique<MonteCarloStrategy>(simulations, seed);
    throw std::invalid_argument("Nieznana strategia.");
}

std::optional<int> measure(Strategy& strategy, const Board& board, SearchStats& stats) {
    auto start = std::chrono::steady_clock::now();
    auto move = strategy.choose(board, stats);
    stats.milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return move;
}
