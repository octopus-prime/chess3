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

bool test_lazy_move_ordering() {
    using namespace chess;
    const move_t low{a1, a2};
    const move_t middle{b1, b2};
    const move_t high{c1, c2};
    const std::array<move_t, 2> no_killers{};
    bool passed = true;
    for (const move_t best : {low, middle, high, move_t{}}) {
        std::array buffer{middle, low, high};
        size_t projections{};
        auto ordered = detail::ordered_moves(buffer, best, no_killers, [&](const move_t move) {
            ++projections;
            return uint16_t(move);
        });
        passed &= test_support::check(projections == 0, "move ordering is lazy at construction");
        auto it = ordered.begin();
        if (best != move_t{}) {
            passed &= test_support::check(*it == best && projections == 0,
                                          "legal TT move is first without sorting");
            ++it;
        }
        passed &= test_support::check(projections > 0, "remaining moves sort only when reached");
        std::array<move_t, 3> actual;
        size_t count{};
        if (best != move_t{})
            actual[count++] = best;
        for (; it != ordered.end(); ++it)
            actual[count++] = *it;
        std::array expected{high, middle, low};
        if (best != move_t{}) {
            const auto found = std::ranges::find(expected, best);
            std::rotate(expected.begin(), found, found + 1);
        }
        passed &= test_support::check(actual == expected && count == 3,
                                      "TT move and sorted remainder contain each move once");
    }
    std::array singleton{low};
    size_t projections{};
    auto one = detail::ordered_moves(singleton, low, no_killers, [&](const move_t move) {
        ++projections;
        return uint16_t(move);
    });
    passed &= test_support::check(std::ranges::distance(one) == 1 && projections == 0,
                                  "singleton TT move needs no sorting");
    auto empty = detail::ordered_moves(std::span<move_t>{}, low, no_killers,
                                       [](const move_t move) { return uint16_t(move); });
    passed &= test_support::check(empty.begin() == empty.end(), "empty move ordering");
    return passed;
}

bool test_phased_move_ordering() {
    using namespace chess;
    const std::array input{
        move_t{a1, a2}, move_t{b1, b2}, move_t{c1, c2},
        move_t{d1, d2}, move_t{e1, e2}, move_t{f1, f2}
    };
    constexpr std::array scores{-1, 0, 1, 99, 300, 900};
    const auto get_score = [&](const move_t move) {
        return scores[move.from().file()];
    };
    std::array<move_t, 7> candidates;
    std::ranges::copy(input, candidates.begin());
    candidates.back() = move_t{};
    bool passed = true;
    for (const move_t best : candidates) {
        for (const move_t killer0 : candidates) {
            for (const move_t killer1 : candidates) {
                auto buffer = input;
                const std::array killers{killer0, killer1};
                size_t projections{};
                auto ordered = detail::ordered_moves(buffer, best, killers, [&](const move_t move) {
                    ++projections;
                    return get_score(move);
                });
                passed &= test_support::check(projections == 0, "four-phase ordering is lazy");
                std::array<move_t, 6> expected;
                size_t expected_count{};
                const auto append = [&](const move_t move) {
                    if (std::ranges::find(input, move) != input.end() &&
                        std::ranges::find(expected.begin(), expected.begin() + expected_count, move) ==
                            expected.begin() + expected_count)
                        expected[expected_count++] = move;
                };
                append(best);
                for (size_t index = input.size(); index-- > 0;)
                    if (scores[index] > 0)
                        append(input[index]);
                append(killer0);
                append(killer1);
                for (size_t index = input.size(); index-- > 0;)
                    append(input[index]);

                std::array<move_t, 6> actual;
                size_t count{};
                for (const move_t move : ordered) {
                    if (count < actual.size())
                        actual[count] = move;
                    ++count;
                }
                passed &= test_support::check(count == input.size() && actual == expected,
                                              "four-phase order emits each legal move exactly once");
            }
        }
    }
    const std::array killers{input[0], input[0]};
    size_t projections{};
    std::array singleton{input[0]};
    auto one = detail::ordered_moves(singleton, input[0], killers, [&](const move_t move) {
        ++projections;
        return get_score(move);
    });
    auto it = one.begin();
    passed &= test_support::check(*it == input[0] && projections == 0,
                                  "TT cutoff avoids partitioning and sorting");
    ++it;
    passed &= test_support::check(it == one.end() && projections == 0,
                                  "singleton with duplicate TT killers is emitted once");
    auto empty = detail::ordered_moves(std::span<move_t>{}, input[0], killers, get_score);
    passed &= test_support::check(empty.begin() == empty.end(), "empty four-phase ordering");
    return passed;
}

bool run_perft_tests() {
    bool passed = test_tt_mate_scores();
    passed &= test_lazy_move_ordering();
    passed &= test_phased_move_ordering();
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
