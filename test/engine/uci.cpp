#include <iostream>
#include <sstream>
#include <string>

#include "test_support.hpp"

import engine.uci;

namespace {

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
    bool passed = test_support::check(commands.eof(), "UCI returns at end of input");

    std::istringstream empty;
    uci(empty);
    passed &= test_support::check(empty.eof(), "UCI accepts empty input");

    std::istringstream quit{"quit\nisready\n"};
    uci(quit);
    std::string remaining;
    std::getline(quit, remaining);
    passed &= test_support::check(remaining == "isready", "UCI quit stops consuming commands");

    passed &= test_support::check(
        capture.output.str() == "id name chess3\nid author me\nuciok\n"
                  "readyok\nreadyok\n"
                  "id name chess3\nid author me\nuciok\n"
                  "readyok\n",
        "UCI identification and readiness responses; new game, unknown commands and quit are silent");
    return passed;
}
