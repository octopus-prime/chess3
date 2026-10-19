#include <iostream>
#include <sstream>
#include <string>
#include <chrono>
#include <limits>
#include <cstdint>

#include "test_support.hpp"

import engine.uci;

namespace {

constexpr bool test_clock_budget() {
    using namespace std::chrono_literals;
    const auto initial = chess::clock_budget(1000, 0, 40, 20);
    const auto late = chess::clock_budget(40, 0, 3, 20);
    const auto last = chess::clock_budget(100, 0, 1, 20);
    const auto increment = chess::clock_budget(1000, 100, 25, 20);
    const auto huge = chess::clock_budget(1000, 0, std::numeric_limits<int64_t>::max(), 5000);
    const auto no_reserve = chess::clock_budget(1000, 0, 40, 0);
    return initial.soft == 5ms && initial.hard == 15ms &&
        late.soft == 0ms && late.hard == 0ms &&
        last.soft == 80ms && last.hard == 80ms &&
        increment.soft == 95ms && increment.hard == 285ms &&
        huge.soft == 0ms && huge.hard == 0ms &&
        no_reserve.soft == 25ms && no_reserve.hard == 75ms;
}

static_assert(test_clock_budget());

struct cout_capture_t final {
    std::ostringstream output;
    std::streambuf* previous = std::cout.rdbuf(output.rdbuf());

    ~cout_capture_t() {
        std::cout.rdbuf(previous);
    }
};

}

bool run_uci_tests() {
    cout_capture_t capture;
    chess::uci_t uci;
    std::istringstream commands{
        "uci\n"
        "isready\n"
        "ucinewgame\n"
        "isready\n"
        "unknown\n"
        "uci\n"
        "isready\n"
    };
    uci(commands);
    bool passed = test_support::check(test_clock_budget(),
                                      "clock budgets reserve overhead for every remaining move");
    passed &= test_support::check(commands.eof(), "UCI returns at end of input");

    std::istringstream empty;
    uci(empty);
    passed &= test_support::check(empty.eof(), "UCI accepts empty input");

    std::istringstream quit{"quit\nisready\n"};
    uci(quit);
    std::string remaining;
    std::getline(quit, remaining);
    passed &= test_support::check(remaining == "isready", "UCI quit stops consuming commands");

    passed &= test_support::check(
        capture.output.str() == "id name chess3\nid author me\n"
                  "option name Move Overhead type spin default 20 min 0 max 5000\nuciok\n"
                  "readyok\nreadyok\n"
                  "id name chess3\nid author me\n"
                  "option name Move Overhead type spin default 20 min 0 max 5000\nuciok\n"
                  "readyok\n",
        "UCI identification and readiness responses; new game, unknown commands and quit are silent");
    return passed;
}
