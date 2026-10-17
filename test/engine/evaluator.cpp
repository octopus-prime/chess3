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
    bool passed = test_support::check(evaluator.evaluate(position) == score_e{10},
                                      "evaluator initial position");

    position.do_move(move_t{e2, e4});
    passed &= test_support::check(evaluator.evaluate(position) == score_e{-51},
                                  "evaluator after 1. e4");

    position.do_move(move_t{e7, e5});
    passed &= test_support::check(evaluator.evaluate(position) == score_e{16},
                                  "evaluator after 1... e5");

    position = position_t{"1k6/8/8/8/3r4/2P5/8/K7 w - -"};
    passed &= test_support::check(evaluator.evaluate(position) == score_e{-841},
                                  "evaluator custom position white to move");

    position = position_t{"1k6/8/8/8/3r4/2P5/8/K7 b - -"};
    passed &= test_support::check(evaluator.evaluate(position) == score_e{1125},
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

    return passed;
}
