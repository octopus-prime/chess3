#include <array>
#include <cstddef>

#include "test_support.hpp"

import attacks.lookup;
import attacks.generator;
import types.squares;

namespace {

constexpr bool test_ray_table() {
    using namespace chess;
    using namespace attacks;
    static_assert(generator::leapers({e1}, {}) == squares_t{d1, f1, d2, e2, f2});
    static_assert(generator::leapers({e8}, {}) == squares_t{d8, f8, d7, e7, f7});
    static_assert(generator::leapers({}, {b1, g1}) == squares_t{a3, c3, d2, e2, f3, h3});
    static_assert(generator::leapers({}, {b8, g8}) == squares_t{a6, c6, d7, e7, f6, h6});
    static_assert(generator::leapers({e1}, {b1, g1}) == (squares_t{d1, f1, d2, e2, f2, a3, c3, d2, e2, f3, h3}));
    static_assert(generator::leapers({e8}, {b8, g8}) == (squares_t{d8, f8, d7, e7, f7, a6, c6, d7, e7, f6, h6}));
    static_assert(generator::sliders({e4}, {}, {}) == squares_t{a4, b4, c4, d4, f4, g4, h4, e1, e2, e3, e5, e6, e7, e8});
    static_assert(generator::sliders({}, {e4}, {}) == squares_t{b1, c2, d3, f5, g6, h7, a8, b7, c6, d5, f3, g2, h1});
    static_assert(generator::pawns({e2}, WHITE) == squares_t{d3, f3});
    static_assert(generator::pawns({e7}, BLACK) == squares_t{d6, f6});
    static_assert(lookup::king(e4) == squares_t{d3, e3, f3, d4, f4, d5, e5, f5});
    static_assert(lookup::knight(e4) == squares_t{c3, d2, f2, g3, g5, f6, d6, c5});
    static_assert(lookup::rook(e4, {}) == squares_t{e1, e2, e3, e5, e6, e7, e8, a4, b4, c4, d4, f4, g4, h4});
    static_assert(lookup::bishop(e4, {}) == squares_t{b1, c2, d3, f5, g6, h7, a8, b7, c6, d5, f3, g2, h1});
    static_assert(lookup::queen(e4, {}) == squares_t{b1, c2, d3, f5, g6, h7, a8, b7, c6, d5, f3, g2, h1, e1, e2, e3, e5, e6, e7, e8, a4, b4, c4, d4, f4, g4, h4});
    static_assert(lookup::pawn(e4, WHITE) == squares_t{d5, f5});
    static_assert(lookup::pawn(e4, BLACK) == squares_t{d3, f3});
    using namespace attacks::lookup;
    constexpr std::array<int, MAX_DIRECTION> file_steps{0, 1, 1, 1, 0, -1, -1, -1};
    constexpr std::array<int, MAX_DIRECTION> rank_steps{1, 1, 0, -1, -1, -1, 0, 1};
    for (square_t from = a1; from <= h8; ++from) {
        squares_t all{};
        for (size_t index = 0; index < MAX_DIRECTION; ++index) {
            squares_t expected{};
            int file = from.file() + file_steps[index];
            int rank = from.rank() + rank_steps[index];
            while (file >= 0 && file < 8 && rank >= 0 && rank < 8) {
                expected |= squares_t{square_t{file_e(file), rank_e(rank)}};
                file += file_steps[index];
                rank += rank_steps[index];
            }
            const squares_t actual = ray(from, direction_e(index));
            if (actual != expected || !(actual & squares_t{from}).empty() ||
                !(actual & all).empty())
                return false;
            all |= actual;
        }
        if (all != queen(from, {}))
            return false;
    }
    return true;
}

static_assert(test_ray_table());
using namespace chess;
static_assert(attacks::lookup::ray(e4, attacks::lookup::NORTH_WEST) ==
              squares_t{d5, c6, b7, a8});
static_assert(attacks::lookup::ray(e4, attacks::lookup::SOUTH_EAST) ==
              squares_t{f3, g2, h1});

}

bool run_lookup_tests() {
    return test_support::check(test_ray_table(), "all 512 directional rays");
}
