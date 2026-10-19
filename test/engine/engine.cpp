#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <type_traits>
#include <algorithm>
#include <stop_token>
#include <ranges>
#include <span>
#include "test_support.hpp"

import engine.engine;
import engine.ordering;
import engine.history;
import engine.killer;
import engine.transposition;
import engine.evaluator;
import core.position;
import types.piece;
import types.hash;
import types.ply;

size_t test_allocation_count() noexcept;

namespace {

constexpr auto search_timeout = std::chrono::hours{1};

chess::score_t test_search(chess::engine_t& search, const chess::ply_t depth) {
    return search(depth, search_timeout);
}

}

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
        for (const ply_t stored_height : {ply_t{0}, ply_t{7}, std::numeric_limits<ply_t>::max()}) {
            for (const ply_t probed_height : {ply_t{0}, ply_t{3}, std::numeric_limits<ply_t>::max()}) {
                for (const bool winning : {false, true}) {
                    const score_t stored_score = winning
                        ? score_e(MAX - 5 - static_cast<int>(stored_height))
                        : score_e(MIN + 5 + static_cast<int>(stored_height));
                    const score_t expected_score = winning
                        ? score_e(MAX - 5 - static_cast<int>(probed_height))
                        : score_e(MIN + 5 + static_cast<int>(probed_height));
                    constexpr ply_t depth = std::numeric_limits<ply_t>::max();
                    table.put(hash_t{1}, move, stored_score, flag, depth, stored_height);
                    const auto entry = table.get(hash_t{1}, probed_height);
                    static_assert(std::is_same_v<decltype(entry->depth), ply_t>);
                    passed &= test_support::check(
                        entry && entry->score == expected_score && entry->move == move &&
                            entry->flag == flag && entry->depth == depth,
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

bool test_static_eval_cache() {
    using namespace chess;
    transposition_t table{0};
    bool passed = test_support::check(!table.get_eval(0) && !table.get_eval(1),
                                      "empty eval cache misses, including the zero key");
    table.put_eval(0, DRAW);
    table.put_eval(1, score_e(-123));
    passed &= test_support::check(table.get_eval(0) == score_t{DRAW} &&
                                  table.get_eval(1) == score_t{score_e(-123)},
                                  "eval cache preserves zero and negative evaluations");
    table.put_eval(65537, score_e(456));
    passed &= test_support::check(!table.get_eval(1) &&
                                  table.get_eval(65537) == score_t{score_e(456)} &&
                                  table.get_eval(0) == score_t{DRAW},
                                  "eval cache collisions compare full keys and replace only their slot");
    const move_t move{e2, e4};
    table.put(65537, move, score_e(MAX - 5), LOWER, 12, 2);
    const auto bound = table.get(65537, 4);
    passed &= test_support::check(
        bound && bound->score == MAX - 7 &&
            table.get_eval(65537) == score_t{score_e(456)} &&
            !table.get(1, 0),
        "cached evaluation stays separate from mate-normalized bounds and search entries");
    table.put_eval(65537, score_e(-999));
    const auto unchanged = table.get(65537, 4);
    passed &= test_support::check(unchanged && unchanged->move == move &&
                                  unchanged->depth == 12 && unchanged->flag == LOWER &&
                                  unchanged->score == MAX - 7,
                                  "eval writes do not change TT bounds, depths or moves");
    table.clear();
    passed &= test_support::check(!table.get_eval(0) && !table.get_eval(65537) &&
                                  !table.get(65537, 0) && table.full() == 0,
                                  "TT clear also invalidates cached evaluations");
    const auto before = test_allocation_count();
    bool correct = true;
    for (hash_t hash = 0; hash < 65536; ++hash)
        table.put_eval(hash, score_e(int(hash % 2001) - 1000));
    for (hash_t hash = 0; hash < 65536; ++hash)
        correct &= table.get_eval(hash) == score_t{score_e(int(hash % 2001) - 1000)};
    table.clear();
    const auto after = test_allocation_count();
    passed &= test_support::check(correct && after == before,
                                  "filling, probing and clearing the entire eval cache allocates nothing");
    return passed;
}

bool test_move_ordering(const bool with_continuation = false) {
    using namespace chess;
    const position_t position{"4k3/P7/8/3pP3/8/8/8/4K1N1 w - d6 0 1"};
    std::array<move_t, 256> buffer;
    const auto legal_moves = position.generate_all_moves(buffer);
    std::array<move_t, 256> original;
    std::ranges::copy(legal_moves, original.begin());
    const auto moves = std::span{buffer}.first(legal_moves.size());

    const move_t best{a7, a8, Q};
    const std::array killers{move_t{e1, f1}, move_t{g1, f3}};
    const auto contains = [&](const move_t move) {
        return std::ranges::find(original.begin(), original.begin() + legal_moves.size(), move) !=
            original.begin() + legal_moves.size();
    };
    bool passed = test_support::check(contains(best), "ordering fixture includes the TT move");
    for (const move_t killer : killers)
        passed &= test_support::check(contains(killer) && !position.tactical(killer),
                                      "ordering fixture includes quiet killer moves");

    history_t history;
    history_t continuation;
    for (size_t i = 0; i < legal_moves.size(); ++i) {
        const move_t move = original[i];
        if (!position.tactical(move) && move != killers[0] && move != killers[1])
            history.update(position.at(move.from()), move.to(),
                           static_cast<int>((legal_moves.size() - i) * 32));
        if (!position.tactical(move))
            continuation.update(position.at(move.from()), move.to(),
                                static_cast<int>(i * 64));
    }

    auto ordered = ordered_moves(position, moves, best, killers, history,
                                 with_continuation ? &continuation : nullptr);
    std::array<move_t, 256> actual;
    size_t count{};
    int previous_phase = -1;
    move_t previous_move{};
    size_t previous_killer_rank{};
    size_t phase_counts[5]{};
    for (const move_t move : ordered) {
        const int phase = move == best ? 0 :
            position.tactical(move) && position.see(move) >= 0 ? 1 :
            move == killers[0] || move == killers[1] ? 2 :
            !position.tactical(move) ? 3 : 4;
        passed &= test_support::check(phase >= previous_phase,
                                      "move ordering follows the five phases");
        ++phase_counts[phase];

        passed &= test_support::check(contains(move), "ordered move is legal");
        passed &= test_support::check(
            std::ranges::find(actual.begin(), actual.begin() + count, move) == actual.begin() + count,
            "ordered move is emitted once");
        if (count < actual.size())
            actual[count] = move;

        if (phase == previous_phase && phase == 1)
            passed &= test_support::check(position.see(previous_move) >= position.see(move),
                                          "good tactics are sorted by SEE");
        if (phase == previous_phase && phase == 2) {
            const size_t killer_rank = move == killers[0] ? 0 : 1;
            passed &= test_support::check(previous_killer_rank <= killer_rank,
                                          "killers retain priority order");
            previous_killer_rank = killer_rank;
        } else if (phase == 2) {
            previous_killer_rank = move == killers[0] ? 0 : 1;
        }
        if (phase == previous_phase && phase == 3) {
            const auto previous_score = history.get(position.at(previous_move.from()),
                                                    previous_move.to()) +
                (with_continuation ? continuation.get(position.at(previous_move.from()),
                                                      previous_move.to()) : 0);
            const auto current_score = history.get(position.at(move.from()), move.to()) +
                (with_continuation ? continuation.get(position.at(move.from()), move.to()) : 0);
            passed &= test_support::check(previous_score >= current_score,
                                          "quiet moves are sorted by combined history");
        }
        if (phase == previous_phase && phase == 4)
            passed &= test_support::check(position.see(previous_move) >= position.see(move),
                                          "bad tactics are sorted by SEE");

        previous_move = move;
        previous_phase = phase;
        ++count;
    }

    passed &= test_support::check(count == legal_moves.size(), "all legal moves are ordered");
    passed &= test_support::check(phase_counts[0] == 1 && phase_counts[2] == 2,
                                  "TT move and both killers appear in their phases");
    return passed;
}

constexpr bool test_combined_history_score() {
    using namespace chess;
    history_t history;
    history_t continuation;
    history.update(WN, f3, 4096);
    continuation.update(WN, f3, 4096);
    if (history.score(WN, f3, nullptr) != 4096 ||
        history.score(WN, f3, &continuation) != 8192 ||
        history.score(WN, f3, &continuation) / 8192 != 1)
        return false;
    history.update(WN, f3, -history_t::LIMIT);
    continuation.update(WN, f3, -history_t::LIMIT);
    if (history.score(WN, f3, &continuation) != -2 * history_t::LIMIT ||
        history.score(WN, f3, &continuation) / 8192 != -4)
        return false;
    history.update(WN, f3, history_t::LIMIT);
    if (history.score(WN, f3, &continuation) != 0)
        return false;
    continuation.update(WN, f3, history_t::LIMIT);
    return history.score(WN, f3, &continuation) == 2 * history_t::LIMIT &&
        history.score(WN, f3, &continuation) / 8192 == 4 &&
        history.score(BN, f3, &continuation) == 0;
}

static_assert(test_combined_history_score());

bool test_continuation_history() {
    using namespace chess;
    continuation_history_t continuation;
    auto& reply = continuation.get(BN, f6);
    bool passed = test_support::check(reply.get(WN, f3) == 0,
                                      "continuation history starts at zero");
    reply.update(WN, f3, 100);
    reply.update(WN, f3, 100);
    reply.update(WN, f3, -100);
    passed &= test_support::check(reply.get(WN, f3) == 99,
                                  "continuation history uses history gravity");
    passed &= test_support::check(
        continuation.get(WN, f6).get(WN, f3) == 0 &&
        continuation.get(BB, f6).get(WN, f3) == 0 &&
        continuation.get(BN, e6).get(WN, f3) == 0 &&
        reply.get(BN, f3) == 0 && reply.get(WB, f3) == 0 && reply.get(WN, e3) == 0,
        "continuation history isolates both colored pieces and destinations");
    reply.update(WN, f3, 1000000);
    reply.update(WB, c4, -1000000);
    passed &= test_support::check(reply.get(WN, f3) == history_t::LIMIT &&
                                  reply.get(WB, c4) == -history_t::LIMIT,
                                  "continuation rewards and penalties are bounded");
    continuation.get(BK, h8).update(WK, a1, 2048);
    continuation.clear();
    bool cleared = true;
    for (size_t previous_piece = 0; previous_piece < MAX_PIECE; ++previous_piece) {
        for (size_t previous_to = 0; previous_to < MAX_SQUARE; ++previous_to) {
            const auto& entry = continuation.get(piece_e(previous_piece), square_e(previous_to));
            for (size_t piece = 0; piece <= BK; ++piece)
                for (size_t to = 0; to < MAX_SQUARE; ++to)
                    cleared &= entry.get(piece_e(piece), square_e(to)) == 0;
        }
    }
    passed &= test_support::check(cleared, "clear resets every continuation context");
    return passed;
}

bool test_history_search_reset() {
    using namespace chess;
    struct case_t {
        std::string_view fen;
        std::string_view moves;
    };
    constexpr std::array cases{
        case_t{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", ""},
        case_t{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
               "e2e4 d7d5 e4d5"},
        case_t{"4k2r/8/8/8/8/8/8/R3K2R w KQk - 0 1", "e1g1"},
        case_t{"4k3/P7/8/8/8/8/8/4K2R w - - 0 1", "a7a8n"},
        case_t{"4k2r/8/8/3pP3/8/8/8/4K2R w - d6 0 1", "e5d6"}
    };
    bool passed = true;
    for (const auto& test : cases) {
        engine_t search{test.fen};
        search.setup(test.fen, test.moves);
        const auto score = test_search(search, 5);
        const auto nodes = search.nodes();
        std::array<move_t, MAX_PLY> first_buffer;
        const auto first_pv = search.extract_pv(first_buffer, 5);
        search.clear();
        const auto repeated = test_search(search, 5);
        std::array<move_t, MAX_PLY> second_buffer;
        const auto second_pv = search.extract_pv(second_buffer, 5);
        passed &= test_support::check(
            !score.ignore() && repeated == score && search.nodes() == nodes &&
                std::ranges::equal(first_pv, second_pv),
            "clearing histories reproduces search after normal and special moves");
    }
    return passed;
}

constexpr bool test_history_updates() {
    using namespace chess;
    history_t history;
    if (history.get(WN, f3) != 0)
        return false;
    history.update(WN, f3, 100);
    if (history.get(WN, f3) != 100 || history.get(BN, f3) != 0 ||
        history.get(WB, f3) != 0 || history.get(WN, e3) != 0)
        return false;
    history.update(WN, f3, 100);
    if (history.get(WN, f3) != 200)
        return false;
    history.update(WN, f3, -100);
    if (history.get(WN, f3) != 99)
        return false;
    for (int iteration = 0; iteration < 1000; ++iteration) {
        history.update(WN, f3, 2048);
        history.update(BN, f6, -2048);
        if (history.get(WN, f3) > history_t::LIMIT ||
            history.get(BN, f6) < -history_t::LIMIT)
            return false;
    }
    if (history.get(WN, f3) <= 0 || history.get(BN, f6) >= 0)
        return false;
    history.update(WN, f3, 1000000);
    history.update(BN, f6, -1000000);
    if (history.get(WN, f3) != history_t::LIMIT ||
        history.get(BN, f6) != -history_t::LIMIT)
        return false;
    history.update(WN, f3, -history_t::LIMIT);
    history.update(BN, f6, history_t::LIMIT);
    return history.get(WN, f3) == -history_t::LIMIT &&
        history.get(BN, f6) == history_t::LIMIT &&
        history_t::bonus(0) == 0 && history_t::bonus(1) == 16 &&
        history_t::bonus(2) == 64 && history_t::bonus(11) == 1936 &&
        history_t::bonus(12) == 2048 &&
        history_t::bonus(std::numeric_limits<ply_t>::max()) == 2048;
}

static_assert(test_history_updates());

constexpr bool test_killer_heights() {
    using namespace chess;
    killer_t killer;
    constexpr ply_t last = std::numeric_limits<ply_t>::max();
    const move_t first{e2, e4};
    const move_t second{d2, d4};
    killer.put(first, last);
    killer.put(second, last);
    killer.put(second, last);
    return killer.get(last)[0] == second && killer.get(last)[1] == first &&
        killer.get(0)[0] == move_t{} && killer.get(last - 1)[0] == move_t{};
}

static_assert(test_killer_heights());
static_assert(std::is_same_v<decltype(&chess::engine_t::operator()),
                            chess::score_t (chess::engine_t::*)(
                                chess::ply_t, std::chrono::steady_clock::duration) noexcept>);
static_assert(std::is_same_v<decltype(&chess::engine_t::extract_pv),
                            std::span<chess::move_t> (chess::engine_t::*)(
                                std::span<chess::move_t, 256>, chess::ply_t) noexcept>);

bool test_search_draws() {
    using namespace chess;
    struct case_t {
        std::string_view fen;
        std::string_view description;
    };
    constexpr std::array cases{
        case_t{"4k3/8/8/8/8/8/8/4K3 w - - 0 1", "bare kings draw"},
        case_t{"4k3/8/8/8/8/8/8/2B1K3 w - - 0 1", "single bishop draw"},
        case_t{"4k1n1/8/8/8/8/8/8/4K3 b - - 0 1", "single knight draw"},
        case_t{"4k1n1/8/8/8/8/8/8/2B1K3 w - - 0 1",
               "one minor per side draws under current material heuristic"},
        case_t{"4k3/8/8/8/8/8/8/R3K3 w - - 100 1", "50-move draw for stronger side"},
        case_t{"4k3/8/8/8/8/8/8/R3K3 b - - 100 1", "50-move draw for weaker side"},
        case_t{"4k3/8/8/8/8/8/8/R3K3 w - - 101 1", "50-move draw above threshold"},
        case_t{"k3R3/8/8/8/8/8/8/4K3 b - - 100 1",
               "50-move draw in check with legal evasions"}
    };
    bool passed = true;
    for (const auto& test : cases) {
        engine_t search{test.fen};
        passed &= test_support::check(test_search(search, 3) == DRAW, test.description);
        std::array<move_t, MAX_PLY> buffer;
        passed &= test_support::check(search.extract_pv(buffer, 3).empty(),
                                      "immediate draw does not create a TT principal variation");
    }

    for (const auto fen : {"4k3/8/8/8/8/8/8/R3K3 w - - 99 1",
                          "4k3/8/8/8/8/8/8/R3K3 b - - 99 1"}) {
        engine_t search{fen};
        passed &= test_support::check(test_search(search, 2) == DRAW,
                                      "quiet child reaches 100 halfmoves in quiescence and full search");
        passed &= test_support::check(search.nodes() > 1,
                                      "99 halfmoves is not an immediate root draw");
    }

    engine_t stalemate{"k7/8/1QK5/8/8/8/8/8 b - - 0 1"};
    passed &= test_support::check(test_search(stalemate, 3) == DRAW,
                                  "stalemate is a draw despite material advantage");

    engine_t mate_at_threshold{"k7/1Q6/2K5/8/8/8/8/8 b - - 100 1"};
    passed &= test_support::check(test_search(mate_at_threshold, 3) == MIN,
                                  "checkmate takes precedence over 50-move draw");

    engine_t mate_on_last_halfmove{"k7/8/1QK5/8/8/8/8/8 w - - 99 1"};
    passed &= test_support::check(test_search(mate_on_last_halfmove, 1) == MAX - 1,
                                  "mating move at 100 halfmoves wins instead of drawing");
    return passed;
}

bool test_selective_search() {
    using namespace chess;
    struct case_t {
        std::string_view fen;
        move_t best;
        size_t max_nodes;
    };
    constexpr std::array cases{
        case_t{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
               {e2, e4}, 200000},
        case_t{"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
               {e2, a6}, 400000},
        case_t{"r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - - 0 1",
               {c3, d5}, 200000},
        case_t{"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
               {b4, f4}, 40000}
    };
    bool passed = true;
    for (const auto& test : cases) {
        engine_t search{test.fen};
        const score_t score = test_search(search, 9);
        passed &= test_support::check(!score.ignore(), "selective depth nine completes");
        passed &= test_support::check(search.nodes() > 0 && search.nodes() < test.max_nodes,
                                      "selective search stays within the node budget");
        std::array<move_t, MAX_PLY> pv_buffer;
        const auto pv = search.extract_pv(pv_buffer, 9);
        passed &= test_support::check(!pv.empty() && pv.front() == test.best,
                                      "selective search preserves the reference best move");
        position_t position{test.fen};
        std::array<move_t, 256> moves;
        for (const move_t move : pv) {
            const auto legal = position.generate_all_moves(moves);
            const bool valid = std::ranges::find(legal, move) != legal.end();
            passed &= test_support::check(valid, "selective search PV contains only legal moves");
            if (!valid)
                break;
            position.do_move(move);
        }
    }
    return passed;
}

bool test_search_timeout() {
    using namespace chess;
    using namespace std::chrono;
    constexpr std::string_view fen{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
    engine_t search{fen};
    const auto start = steady_clock::now();
    const score_t score = search(std::numeric_limits<ply_t>::max(), milliseconds{20});
    const auto elapsed = steady_clock::now() - start;
    bool passed = test_support::check(!score.ignore(), "timeout preserves a completed search score");
    passed &= test_support::check(elapsed < seconds{1}, "full and quiescence search respect the deadline");
    passed &= test_support::check(search.nodes() > 0, "timed search visits nodes");

    evaluator_t evaluator;
    const score_t expected = evaluator.evaluate(position_t{fen});
    passed &= test_support::check(test_search(search, 0) == expected,
                                  "timeout restores the root position");
    passed &= test_support::check(search(std::numeric_limits<ply_t>::max(), milliseconds{0}) == expected,
                                  "zero-time search returns the root evaluation");
    passed &= test_support::check(search.nodes() == 0, "node count resets for each search");
    return passed;
}

bool run_mate_tests() {
    using namespace chess;
    struct case_t {
        std::string_view fen;
        ply_t depth;
        ply_t mate_plies;
        std::string_view description;
    };
    constexpr std::array cases{
        case_t{"1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -", 5, 5,
               "mate in 3 at depth 5"},
        case_t{"4r1k1/pp4pp/8/q1p1N3/5P2/1P4P1/P6P/3Q2K1 w - -", 9, 9,
               "mate in 5 at depth 9"}
        // case_t{"n1N3br/2p1Bpkr/1pP2R1b/pP1p1PpR/Pp4P1/1P6/1K1P4/8 w - -", 18, 11,
        //        "mate in 6 at selective depth 18"}
        // case_t{"r3k3/1pp1p1Kp/1p2P3/1p6/2N5/P2P2pr/5B1p/3Q1R2 w q -", 15, 15,
        //        "mate in 8 at depth 15"}
    };
    bool passed = true;
    for (const auto& test : cases) {
        engine_t search{test.fen};
        const score_t score = test_search(search, test.depth);
        const score_t expected{score_e(MAX - test.mate_plies)};
        if (score != expected)
            std::cerr << test.description << ": expected " << int(score_e(expected))
                      << ", got " << int(score_e(score)) << '\n';
        passed &= test_support::check(score == expected, test.description);
        std::array<move_t, MAX_PLY> pv_buffer;
        const auto pv = search.extract_pv(pv_buffer, test.depth);
        position_t position{test.fen};
        std::array<move_t, 256> move_buffer;
        for (const move_t move : pv) {
            const auto legal = position.generate_all_moves(move_buffer);
            const bool valid = std::ranges::find(legal, move) != legal.end();
            passed &= test_support::check(valid, "mate PV contains only legal moves");
            if (!valid)
                break;
            position.do_move(move);
        }
        passed &= test_support::check(!pv.empty(), "mate search produces a principal variation");
    }
    return passed;
}

bool run_engine_tests() {
    bool passed = test_tt_mate_scores();
    passed &= test_static_eval_cache();
    passed &= test_move_ordering();
    passed &= test_move_ordering(true);
    passed &= test_continuation_history();
    passed &= test_support::check(test_combined_history_score(),
                                  "combined history drives signed LMR adjustments without overflow");
    passed &= test_history_search_reset();
    passed &= test_support::check(test_history_updates(), "piece-to history gravity updates");
    passed &= test_support::check(test_killer_heights(), "killers support the maximum ply height");
    passed &= test_search_draws();
    passed &= test_selective_search();
    passed &= test_search_timeout();
    {
        chess::engine_t search{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
        for (const size_t limit : {size_t{1}, size_t{200}}) {
            const auto score = search.run(40, search_timeout, search_timeout, limit, {});
            passed &= test_support::check(!score.ignore() && search.nodes() <= limit,
                                          "node-limited search preserves a score without exceeding its budget");
        }
        std::stop_source source;
        source.request_stop();
        const auto score = search.run(40, search_timeout, search_timeout, 200, source.get_token());
        passed &= test_support::check(!score.ignore() && search.nodes() == 0,
                                      "stop requested before search startup is not lost");
    }
    for (const auto fen : {"k7/4P3/8/8/8/8/8/7K w - - 0 1",
                          "7k/8/8/8/8/8/4p3/K7 b - - 0 1"}) {
        chess::engine_t search{fen};
        const auto score = test_search(search, 0);
        passed &= test_support::check(score > chess::PAWN,
                                      "quiescence searches quiet promotions");
        passed &= test_support::check(search.nodes() > 1,
                                      "quiescence expands promotion moves");
        passed &= test_support::check(test_search(search, 0) == score,
                                      "quiescence restores the position");
    }
    chess::engine_t mate{"k7/1Q6/2K5/8/8/8/8/8 b - - 0 1"};
    passed &= test_support::check(test_search(mate, 0) == chess::MIN,
                                  "quiescence detects checkmate without stand pat");
    passed &= test_support::check(
        mate(std::numeric_limits<chess::ply_t>::max(), std::chrono::seconds{1}) == chess::MIN,
                                  "search accepts the maximum ply depth");
    passed &= test_support::check(mate.nodes() == chess::MAX_PLY,
                                  "depth 255 completes without counter wraparound");
    chess::engine_t mate_in_one{"k7/8/1QK5/8/8/8/8/8 w - - 0 1"};
    passed &= test_support::check(test_search(mate_in_one, 1) == chess::MAX - 1,
                                  "search scores mate in one at the correct distance");
    std::array<chess::move_t, 256> pv_buffer;
    passed &= test_support::check(mate_in_one.extract_pv(pv_buffer, 0).empty(),
                                  "zero-length PV extraction leaves the position unchanged");
    const auto pv = mate_in_one.extract_pv(pv_buffer, chess::MAX_PLY - 2);
    passed &= test_support::check(pv.size() == 1,
                                  "PV extraction accepts the maximum ply length");
    passed &= test_support::check(test_search(mate_in_one, 1) == chess::MAX - 1,
                                  "PV extraction restores the root position");
    chess::engine_t evasion{"k3r3/8/8/8/8/8/8/4K3 w - - 0 1"};
    const auto evasion_score = test_search(evasion, 0);
    passed &= test_support::check(evasion.nodes() > 1 && evasion_score > chess::LOSS,
                                  "quiescence searches quiet check evasions");
    passed &= test_support::check(test_search(evasion, 0) == evasion_score,
                                  "quiescence restores the position after evasions");

    return passed;
}
