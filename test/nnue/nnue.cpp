#include <cstdint>
#include <memory>
#include <span>
#include "test_support.hpp"

import nnue.nnue;
import types.square;
import types.side;
import types.piece;

using namespace chess;

bool run_nnue_tests() {
    const auto nnue = nnue::nnue_t::load();

    nnue::entry_t entry[2];
    nnue->initialize(entry[WHITE]);
    nnue->initialize(entry[BLACK]);

    // evaluation with refresh
    const uint16_t active_features[2][32] = { {
            nnue::make_index(a1, a1, WK, WHITE),
            nnue::make_index(a1, b8, BK, WHITE),
            nnue::make_index(a1, c3, WP, WHITE),
            nnue::make_index(a1, d4, BR, WHITE)
        }, {
            nnue::make_index(b8, a1, WK, BLACK),
            nnue::make_index(b8, b8, BK, BLACK),
            nnue::make_index(b8, c3, WP, BLACK),
            nnue::make_index(b8, d4, BR, BLACK)
        }
    };

    nnue->update(entry[WHITE], {}, std::span{active_features[WHITE]}.first(4));
    nnue->update(entry[BLACK], {}, std::span{active_features[BLACK]}.first(4));

    bool passed = test_support::check(nnue->evaluate(entry[WHITE], entry[BLACK], 4) == -420,
                                      "nnue evaluate refresh white perspective");
    passed &= test_support::check(nnue->evaluate(entry[BLACK], entry[WHITE], 4) == 562,
                                  "nnue evaluate refresh black perspective");

    // evaluation with update
    const uint16_t removed_features[2][3] = { {
            nnue::make_index(a1, c3, WP, WHITE),
            nnue::make_index(a1, d4, BR, WHITE)
        }, {
            nnue::make_index(b8, c3, WP, BLACK),
            nnue::make_index(b8, d4, BR, BLACK)
    }};

    const uint16_t added_features[2][3] = { {
            nnue::make_index(a1, d4, WP, WHITE)
        }, {
            nnue::make_index(b8, d4, WP, BLACK)
    }};

    nnue->update(entry[WHITE], std::span{removed_features[WHITE]}.first(2), std::span{added_features[WHITE]}.first(1));
    nnue->update(entry[BLACK], std::span{removed_features[BLACK]}.first(2), std::span{added_features[BLACK]}.first(1));

    passed &= test_support::check(nnue->evaluate(entry[WHITE], entry[BLACK], 3) == 8,
                                  "nnue evaluate update white perspective");
    passed &= test_support::check(nnue->evaluate(entry[BLACK], entry[WHITE], 3) == 23,
                                  "nnue evaluate update black perspective");

    return passed;
}
