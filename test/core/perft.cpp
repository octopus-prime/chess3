#include <array>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <ranges>
#include <span>
#include "test_support.hpp"

import core.perft;

bool run_perft_tests() {
    bool passed = true;
    chess::perft_t perft{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
    constexpr std::array expected{1, 20, 400, 8902, 197281, 4865609, 119060324};
    for (size_t depth = 0; depth < expected.size(); ++depth) {
        const size_t actual = perft(static_cast<int>(depth));
        if (actual != expected[depth])
            std::cerr << "Initial position, depth " << depth << ": expected "
                      << expected[depth] << ", actual " << actual << '\n';
        passed &= test_support::check(actual == expected[depth], "initial-position perft");
    }
    passed &= test_support::check(perft(2) == expected[2], "perft restores the position");

    chess::perft_t kiwipete{
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"};
    constexpr std::array kiwipete_expected{1, 48, 2039, 97862, 4085603, 193690690};
    for (size_t depth = 0; depth < kiwipete_expected.size(); ++depth) {
        const size_t actual = kiwipete(static_cast<int>(depth));
        if (actual != kiwipete_expected[depth])
            std::cerr << "Kiwipete, depth " << depth << ": expected "
                      << kiwipete_expected[depth] << ", actual " << actual << '\n';
        passed &= test_support::check(actual == kiwipete_expected[depth], "Kiwipete perft");
    }
    passed &= test_support::check(kiwipete(2) == kiwipete_expected[2],
                                  "Kiwipete perft restores the position");

    constexpr std::array ep_positions{
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
        "4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1"
    };
    constexpr std::array ep_expected{1, 7, 38};
    for (const auto fen : ep_positions) {
        chess::perft_t ep_perft{fen};
        for (size_t depth = 0; depth < ep_expected.size(); ++depth) {
            const size_t actual = ep_perft(static_cast<int>(depth));
            if (actual != ep_expected[depth])
                std::cerr << "En-passant position " << fen << ", depth " << depth
                          << ": expected " << ep_expected[depth] << ", actual " << actual << '\n';
            passed &= test_support::check(actual == ep_expected[depth], "en-passant perft");
        }
        passed &= test_support::check(ep_perft(2) == ep_expected[2],
                                      "en-passant perft restores the position");
    }
    return passed;
}
