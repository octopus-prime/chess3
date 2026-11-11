#include <algorithm>
#include <array>
#include <memory>
#include <random>
#include <string>
#include "../nnue/reference_positions.hpp"
#include "test_support.hpp"

import engine.evaluator;
import core.position;
import core.move;
import types.square;
import types.score;
import types.ply;
import nnue.nnue;

using namespace chess;

size_t test_allocation_count() noexcept;

namespace {

bool test_random_cache_transitions() {
    const auto network = nnue::load();
    evaluator_t evaluator;
    std::mt19937 random{20261018};
    std::array<nnue::feature_buffer_t, 2> buffers;
    nnue::entry_t entries[2];
    const auto matches_full_refresh = [&](const position_t& position) {
        const auto features = nnue::active_features(position, buffers);
        for (const side_t side : {WHITE, BLACK}) {
            network->initialize(entries[side]);
            network->update(entries[side], {}, features[side]);
        }
        const auto side = position.side();
        const auto expected = network->evaluate(
            entries[side], entries[~side], position);
        return test_support::check(evaluator.evaluate(position) == expected,
                                   "random-walk cached evaluation matches a full accumulator rebuild");
    };

    for (const auto& test : nnue_reference::cases) {
        position_t position{test.fen};
        std::array<move_t, 48> moves;
        size_t count = 0;
        if (!matches_full_refresh(position))
            return false;
        for (; count < moves.size(); ++count) {
            std::array<move_t, 256> buffer;
            const auto legal = position.generate_all_moves(buffer);
            if (legal.empty())
                break;
            moves[count] = legal[random() % legal.size()];
            position.do_move(moves[count]);
            if (!matches_full_refresh(position))
                return false;
            if (!position.check()) {
                position.do_null_move();
                if (!matches_full_refresh(position))
                    return false;
                position.undo_null_move();
                if (!matches_full_refresh(position))
                    return false;
            }
        }
        while (count) {
            position.undo_move(moves[--count]);
            if (!matches_full_refresh(position))
                return false;
        }
        if (!test_support::check(evaluator.evaluate(position) == test.score,
                                 "undoing the random walk restores the exact Stockfish reference score"))
            return false;
    }
    return true;
}

bool test_allocation_free_evaluation() {
    constexpr size_t king_positions = 64;
    const auto positions = std::make_unique<position_t[]>(
        king_positions + nnue_reference::cases.size());
    for (size_t king = 0; king < king_positions; ++king) {
        std::string fen;
        for (int rank = 7; rank >= 0; --rank) {
            int empty = 0;
            for (int file = 0; file < 8; ++file) {
                const auto square = static_cast<size_t>(rank * 8 + file);
                const char piece = square == king ? 'K' : square == (king + 32) % 64 ? 'k' : '\0';
                if (!piece) {
                    ++empty;
                    continue;
                }
                if (empty) {
                    fen += static_cast<char>('0' + empty);
                    empty = 0;
                }
                fen += piece;
            }
            if (empty)
                fen += static_cast<char>('0' + empty);
            if (rank)
                fen += '/';
        }
        positions[king].setup(fen + " w - - 0 1");
    }
    for (size_t i = 0; i < nnue_reference::cases.size(); ++i)
        positions[king_positions + i].setup(nnue_reference::cases[i].fen);

    evaluator_t evaluator;
    std::array<score_t, king_positions> king_scores;
    const auto before = test_allocation_count();
    bool correct = true;
    for (size_t round = 0; round < 2; ++round) {
        for (size_t i = 0; i < king_positions; ++i) {
            if (!round)
                king_scores[i] = evaluator.evaluate(positions[i]);
            else
                correct &= evaluator.evaluate(positions[i]) == king_scores[i];
            evaluator.reset_stack();
            correct &= evaluator.evaluate_incremental(positions[i]) == king_scores[i];
        }
        for (size_t i = 0; i < nnue_reference::cases.size(); ++i) {
            correct &= evaluator.evaluate(positions[king_positions + i]) == nnue_reference::cases[i].score;
            evaluator.reset_stack();
            correct &= evaluator.evaluate_incremental(positions[king_positions + i]) ==
                nnue_reference::cases[i].score;
            for (size_t ply = 1; ply < MAX_PLY; ++ply)
                evaluator.push();
            correct &= evaluator.evaluate_incremental(positions[king_positions + i]) ==
                nnue_reference::cases[i].score;
            for (size_t ply = 1; ply < MAX_PLY; ++ply)
                evaluator.pop();
        }
    }
    const auto after = test_allocation_count();
    return test_support::check(correct, "allocation test retains exact reference evaluations") &&
        test_support::check(after == before, "evaluation allocates nothing, including all cold king-cache entries");
}

bool test_incremental_stack() {
    const auto network = nnue::load();
    evaluator_t evaluator;
    std::mt19937 random{20261107};
    std::array<nnue::feature_buffer_t, 2> buffers;
    std::array<nnue::entry_t, 2> rebuilt;
    const auto matches = [&](const position_t& position) {
        const auto features = nnue::active_features(position, buffers);
        for (const side_t side : {WHITE, BLACK}) {
            network->initialize(rebuilt[side]);
            network->update(rebuilt[side], {}, features[side]);
        }
        const auto side = position.side();
        return test_support::check(
            evaluator.evaluate_incremental(position) == network->evaluate(
                rebuilt[side], rebuilt[~side], position),
            "lazy search-stack evaluation matches a full rebuild");
    };
    const auto check_delta = [&](const position_t& before, const position_t& after) {
        const nnue::feature_state_t old_state{before}, new_state{after};
        std::array<nnue::entry_t, 2> updated;
        const auto old_features = nnue::active_features(before, buffers);
        for (const side_t side : {WHITE, BLACK}) {
            network->initialize(updated[side]);
            network->update(updated[side], {}, old_features[side]);
        }
        std::array<nnue::feature_buffer_t, 2> removed, added;
        const auto changes = nnue::changed_features(old_state, new_state, removed, added);
        for (const side_t side : {WHITE, BLACK}) {
            if (!nnue::compatible(old_state, new_state, side))
                continue;
            network->update(updated[side], changes[side][0], changes[side][1]);
            const auto features = nnue::active_features(after, buffers);
            network->initialize(rebuilt[side]);
            network->update(rebuilt[side], {}, features[side]);
            if (!test_support::check(
                    std::ranges::equal(updated[side].accumulation, rebuilt[side].accumulation),
                    "incremental deltas reproduce all 1024 accumulator lanes exactly"))
                return false;
        }
        return true;
    };
    for (const auto& test : nnue_reference::cases) {
        position_t position{test.fen};
        evaluator.reset_stack();
        if (!matches(position))
            return false;
        std::array<move_t, 80> path;
        size_t count = 0;
        for (; count < path.size(); ++count) {
            std::array<move_t, 256> buffer;
            const auto moves = position.generate_all_moves(buffer);
            if (moves.empty())
                break;
            const auto before = position;
            path[count] = moves[random() % moves.size()];
            position.do_move(path[count]);
            evaluator.push();
            if (!check_delta(before, position))
                return false;
            // Leave some frames unevaluated, as TT hits and check nodes do in search.
            if (count % 3 != 1 && !matches(position))
                return false;
            if (!position.check()) {
                position.do_null_move();
                evaluator.push();
                if (!matches(position))
                    return false;
                evaluator.pop();
                position.undo_null_move();
            }
            const auto siblings = position.generate_all_moves(buffer);
            if (!siblings.empty()) {
                const auto move = siblings[random() % siblings.size()];
                evaluator.push();
                position.do_move(move);
                if (!matches(position))
                    return false;
                position.undo_move(move);
                evaluator.pop();
            }
        }
        while (count) {
            position.undo_move(path[--count]);
            evaluator.pop();
            if (!matches(position))
                return false;
        }
    }
    return true;
}

}

bool run_evaluator_tests() {
    evaluator_t evaluator{};
    if (!test_incremental_stack())
        return false;

    position_t position{};
    bool passed = test_support::check(evaluator.evaluate(position) == score_e{20},
                                      "evaluator initial position");

    position.do_move(move_t{e2, e4});
    passed &= test_support::check(evaluator.evaluate(position) == score_e{-1},
                                  "evaluator after 1. e4");

    position.do_move(move_t{e7, e5});
    passed &= test_support::check(evaluator.evaluate(position) == score_e{34},
                                  "evaluator after 1... e5");

    position = position_t{"1k6/8/8/8/3r4/2P5/8/K7 w - -"};
    passed &= test_support::check(evaluator.evaluate(position) == score_e{-1538},
                                  "evaluator custom position white to move");

    position = position_t{"1k6/8/8/8/3r4/2P5/8/K7 b - -"};
    passed &= test_support::check(evaluator.evaluate(position) == score_e{4554},
                                  "evaluator custom position black to move");

    // Test king moves and cache consistency compared to fresh evaluation
    position = position_t{};
    for (const move_t move : {move_t{e2, e4}, move_t{e7, e5},
                              move_t{e1, e2}, move_t{e8, e7}, // kings move
                              move_t{g1, f3}, move_t{b8, c6},
                              move_t{e2, e1}, move_t{e7, e8}}) { // kings move back
        position.do_move(move);
        const auto cached_eval = evaluator.evaluate(position);
        evaluator_t fresh_evaluator{};
        const auto fresh_eval = fresh_evaluator.evaluate(position);
        passed &= test_support::check(cached_eval == fresh_eval,
                                      "cached evaluator matches fresh evaluator after king moves");
    }

    // Check consistency across tactical positions and king moves
    for (const auto fen : {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
                          "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",
                          "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",
                          "4K3/1P1P1P1P/8/8/8/8/p1p1p1p1/4k3 w - -",
                          "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -"}) {
        position_t pos{fen};
        const auto cached_eval = evaluator.evaluate(pos);
        evaluator_t fresh_evaluator{};
        passed &= test_support::check(cached_eval == fresh_evaluator.evaluate(pos),
                                      "cached evaluator matches fresh evaluator on diverse FEN");
    }

    for (const auto& test : nnue_reference::cases) {
        position.setup(test.fen);
        passed &= test_support::check(evaluator.evaluate(position) == test.score,
                                      "accumulator cache matches Stockfish across positions and queen buckets");
        passed &= test_support::check(evaluator.evaluate(position) == test.score,
                                      "unchanged position leaves cached features unchanged");
    }

    struct move_case_t {
        std::string_view fen;
        move_t move;
    };
    const std::array moves{
        move_case_t{"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", {e1, g1}},
        move_case_t{"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", {e8, c8}},
        move_case_t{"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", {e5, d6}},
        move_case_t{"4k3/8/8/8/8/8/3q4/3RK3 w - - 0 1", {d1, d2}},
        move_case_t{"4k3/P7/8/8/8/8/8/4K3 w - - 0 1", {a7, a8, Q}},
        move_case_t{"4k3/P7/8/8/8/8/8/4K3 w - - 0 1", {a7, a8, N}},
        move_case_t{"4k3/P7/8/8/8/8/8/4K3 w - - 0 1", {a7, a8, B}},
        move_case_t{"4k3/P7/8/8/8/8/8/4K3 w - - 0 1", {a7, a8, R}},
        move_case_t{"1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1", {a7, b8, N}},
        move_case_t{"r3k3/8/8/4n3/3P4/8/4P3/4K3 w - - 0 1", {d4, e5}},
        move_case_t{"4k3/8/8/3p4/8/8/8/3RK3 w - - 0 1", {d1, d5}},
        move_case_t{"4k3/8/8/8/8/8/p7/4K3 b - - 0 1", {a2, a1, Q}}
    };
    for (const auto& test : moves) {
        position.setup(test.fen);
        const auto before = evaluator.evaluate(position);
        evaluator.reset_stack();
        passed &= test_support::check(evaluator.evaluate_incremental(position) == before,
                                      "search-stack root matches the reference evaluator");
        std::array<move_t, 256> legal_buffer;
        const auto legal = position.generate_all_moves(legal_buffer);
        const bool valid = std::ranges::find(legal, test.move) != legal.end();
        passed &= test_support::check(valid, "cache transition fixture uses a legal move");
        if (!valid)
            continue;
        position.do_move(test.move);
        evaluator.push();
        evaluator_t fresh;
        passed &= test_support::check(evaluator.evaluate(position) == fresh.evaluate(position),
                                      "cache refresh handles castling, en passant, queen capture, and promotion");
        passed &= test_support::check(evaluator.evaluate_incremental(position) == fresh.evaluate(position),
                                      "search-stack deltas handle special moves and refresh boundaries");
        position.undo_move(test.move);
        evaluator.pop();
        passed &= test_support::check(evaluator.evaluate_incremental(position) == before,
                                      "search-stack pop restores the parent accumulator");
        passed &= test_support::check(evaluator.evaluate(position) == before,
                                      "cache refresh restores all three feature families after undo");
    }
    position.setup("4k3/8/8/3pP3/8/8/8/3QK3 w - d6 0 1");
    const auto before_null = evaluator.evaluate(position);
    position.do_null_move();
    evaluator_t fresh;
    passed &= test_support::check(evaluator.evaluate(position) == fresh.evaluate(position),
                                  "null move selects the opposite perspective and queen bucket");
    position.undo_null_move();
    passed &= test_support::check(evaluator.evaluate(position) == before_null,
                                  "null-move undo preserves the cached evaluation");
    passed &= test_allocation_free_evaluation();
    passed &= test_random_cache_transitions();
    return passed;
}
