#include <array>
#include <cstdint>
#include <iostream>

#include "perft.hpp"
#include "test_support.hpp"

bool test_tt_mate_scores() {
    using namespace chess;
    static_assert(score_to_tt(score_e(MAX - 12), 7) == MAX - 5);
    static_assert(score_to_tt(score_e(MIN + 12), 7) == MIN + 5);
    static_assert(score_from_tt(score_e(MAX - 5), 3) == MAX - 8);
    static_assert(score_from_tt(score_e(MIN + 5), 3) == MIN + 8);
    constexpr std::array unchanged{
        score_t{DRAW}, score_t{PAWN}, score_t{score_e(-QUEEN)},
        score_t{score_e(WIN - 1)}, score_t{score_e(LOSS + 1)},
        score_t{IGNORE}, score_t{score_e(-IGNORE)}
    };
    static_assert([&] {
        for (const score_t score : unchanged)
            if (score_to_tt(score, 17) != score || score_from_tt(score, 17) != score)
                return false;
        return true;
    }());

    transposition_t table{0};
    const move_t move{e7, e8, Q};
    bool passed = true;
    for (const flag_e flag : {EXACT, LOWER, UPPER}) {
        for (const size_t stored_height : {size_t{0}, size_t{7}, size_t{1023}}) {
            for (const size_t probed_height : {size_t{0}, size_t{3}, size_t{1023}}) {
                for (const bool winning : {false, true}) {
                    const score_t stored_score = winning
                        ? score_e(MAX - 5 - static_cast<int>(stored_height))
                        : score_e(MIN + 5 + static_cast<int>(stored_height));
                    const score_t expected_score = winning
                        ? score_e(MAX - 5 - static_cast<int>(probed_height))
                        : score_e(MIN + 5 + static_cast<int>(probed_height));
                    table.put(hash_t{1}, move, stored_score, flag, 9, stored_height);
                    const auto entry = table.get(hash_t{1}, probed_height);
                    passed &= test_support::check(
                        entry && entry->score == expected_score && entry->move == move &&
                            entry->flag == flag && entry->depth == 9,
                        "TT mate distance adjusts to probe ply for exact and bound entries");
                    const auto same_ply = table.get(hash_t{1}, stored_height);
                    passed &= test_support::check(same_ply && same_ply->score == stored_score,
                                                  "TT mate score round trip");
                }
            }
        }
        for (const score_t score : unchanged) {
            table.put(hash_t{1}, move, score, flag, 9, 7);
            const auto entry = table.get(hash_t{1}, 3);
            passed &= test_support::check(entry && entry->score == score,
                                          "TT preserves non-mate scores and ignore sentinels");
        }
    }
    return passed;
}

bool run_perft_tests() {
    bool passed = test_tt_mate_scores();
    for (const auto fen : {"k7/4P3/8/8/8/8/8/7K w - - 0 1",
                          "7k/8/8/8/8/8/4p3/K7 b - - 0 1"}) {
        chess::negamax_t search{fen};
        const auto score = search(0);
        passed &= test_support::check(score > chess::PAWN,
                                      "quiescence searches quiet promotions");
        passed &= test_support::check(search.nodes() > 1,
                                      "quiescence expands promotion moves");
        passed &= test_support::check(search(0) == score,
                                      "quiescence restores the position");
    }
    chess::negamax_t mate{"k7/1Q6/2K5/8/8/8/8/8 b - - 0 1"};
    passed &= test_support::check(mate(0) == chess::MIN,
                                  "quiescence detects checkmate without stand pat");
    chess::negamax_t mate_in_one{"k7/8/1QK5/8/8/8/8/8 w - - 0 1"};
    passed &= test_support::check(mate_in_one(1) == chess::MAX - 1,
                                  "search scores mate in one at the correct distance");
    chess::negamax_t evasion{"k3r3/8/8/8/8/8/8/4K3 w - - 0 1"};
    const auto evasion_score = evasion(0);
    passed &= test_support::check(evasion.nodes() > 1 && evasion_score > chess::LOSS,
                                  "quiescence searches quiet check evasions");
    passed &= test_support::check(evasion(0) == evasion_score,
                                  "quiescence restores the position after evasions");

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
