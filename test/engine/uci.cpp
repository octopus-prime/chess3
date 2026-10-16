#include <array>
#include <cstdio>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>

#include "test_support.hpp"

import engine.uci;

bool run_uci_tests() {
    const std::unique_ptr<std::FILE, decltype(&std::fclose)> output{std::tmpfile(), &std::fclose};
    if (!test_support::check(output != nullptr, "create UCI output capture"))
        return false;
    if (!test_support::check(std::fflush(stdout) == 0, "flush stdout before UCI capture"))
        return false;
    const int saved_stdout = dup(fileno(stdout));
    if (!test_support::check(saved_stdout != -1, "save stdout for UCI capture"))
        return false;
    if (!test_support::check(dup2(fileno(output.get()), fileno(stdout)) != -1,
                             "redirect UCI stdout")) {
        test_support::check(close(saved_stdout) == 0, "close saved stdout");
        return false;
    }

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

    passed &= test_support::check(std::fflush(stdout) == 0, "flush captured UCI output");
    passed &= test_support::check(dup2(saved_stdout, fileno(stdout)) != -1, "restore stdout");
    passed &= test_support::check(close(saved_stdout) == 0, "close saved stdout");
    if (!test_support::check(std::fseek(output.get(), 0, SEEK_SET) == 0, "rewind UCI output"))
        return false;

    std::string actual;
    std::array<char, 256> buffer;
    while (const auto count = std::fread(buffer.data(), 1, buffer.size(), output.get()))
        actual.append(buffer.data(), count);
    passed &= test_support::check(std::ferror(output.get()) == 0, "read captured UCI output");
    passed &= test_support::check(
        actual == "id name chess3\nid author me\nuciok\n"
                  "readyok\nreadyok\n"
                  "id name chess3\nid author me\nuciok\n"
                  "readyok\n",
        "UCI identification and readiness responses; new game, unknown commands and quit are silent");
    return passed;
}
