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
    bool passed = test_support::check(evaluator.evaluate(position) == score_e{4},
                                      "evaluator initial position");

    position.do_move(move_t{e2, e4});
    passed &= test_support::check(evaluator.evaluate(position) == score_e{-24},
                                  "evaluator after 1. e4");

    position.do_move(move_t{e7, e5});
    passed &= test_support::check(evaluator.evaluate(position) == score_e{8},
                                  "evaluator after 1... e5");

    position = position_t{"1k6/8/8/8/3r4/2P5/8/K7 w - -"};
    passed &= test_support::check(evaluator.evaluate(position) == score_e{-420},
                                  "evaluator custom position white to move");

    position = position_t{"1k6/8/8/8/3r4/2P5/8/K7 b - -"};
    passed &= test_support::check(evaluator.evaluate(position) == score_e{560},
                                  "evaluator custom position black to move");

    return passed;
}
