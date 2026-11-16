#pragma once
#include "Strategy.hpp"
#include <array>
#include <memory>
#include <string>

int readInt(const std::string& prompt, int minimum, int maximum);
void waitForEnter(const std::string& prompt);

class Game {
    Board board_;
    std::array<std::unique_ptr<Strategy>, 2> players_;
    void print() const;

public:
    Game(int rows, int columns, std::unique_ptr<Strategy> first, std::unique_ptr<Strategy> second);
    void run();
};
