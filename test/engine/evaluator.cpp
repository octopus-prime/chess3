#include <algorithm>
#include <array>
#include "../nnue/reference_positions.hpp"
#include "test_support.hpp"

import engine.evaluator;
import core.position;
import core.move;
import types.square;
import types.score;

using namespace chess;

bool run_evaluator_tests() {
    evaluator_t evaluator{};

    position_t position{};
    bool passed = test_support::check(evaluator.evaluate(position) == score_e{26},
                                      "evaluator initial position");

    position.do_move(move_t{e2, e4});
    passed &= test_support::check(evaluator.evaluate(position) == score_e{-12},
                                  "evaluator after 1. e4");

    position.do_move(move_t{e7, e5});
    passed &= test_support::check(evaluator.evaluate(position) == score_e{32},
                                  "evaluator after 1... e5");

    position = position_t{"1k6/8/8/8/3r4/2P5/8/K7 w - -"};
    passed &= test_support::check(evaluator.evaluate(position) == score_e{-1513},
                                  "evaluator custom position white to move");

    position = position_t{"1k6/8/8/8/3r4/2P5/8/K7 b - -"};
    passed &= test_support::check(evaluator.evaluate(position) == score_e{2193},
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
        move_case_t{"4k3/8/8/8/8/8/p7/4K3 b - - 0 1", {a2, a1, Q}}
    };
    for (const auto& test : moves) {
        position.setup(test.fen);
        const auto before = evaluator.evaluate(position);
        std::array<move_t, 256> legal_buffer;
        const auto legal = position.generate_all_moves(legal_buffer);
        const bool valid = std::ranges::find(legal, test.move) != legal.end();
        passed &= test_support::check(valid, "cache transition fixture uses a legal move");
        if (!valid)
            continue;
        position.do_move(test.move);
        evaluator_t fresh;
        passed &= test_support::check(evaluator.evaluate(position) == fresh.evaluate(position),
                                      "cache refresh handles castling, en passant, queen capture, and promotion");
        position.undo_move(test.move);
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
    return passed;
}
