#include <array>
#include <cstddef>
#include <cstdint>

#include "test_support.hpp"

import types.piece;
import types.squares;
import types.score;

namespace {

constexpr bool test_piece_and_side() {
    using namespace chess;
    static_assert(sizeof(piece_t) == sizeof(uint8_t));
    static_assert(sizeof(side_t) == sizeof(uint8_t));
    for (const side_t side : {WHITE, BLACK}) {
        if (~~side != side || ~side == side)
            return false;
        for (const type_t type : {P, N, B, R, Q, K}) {
            const piece_t piece{side, type};
            if (piece.side() != side || piece.type() != type)
                return false;
        }
    }
    return piece_t{WHITE, K} == WK && piece_t{BLACK, N} == BN &&
        piece_t{}.type() == NO_TYPE;
}

constexpr bool test_coordinates() {
    using namespace chess;
    static_assert(sizeof(square_t) == sizeof(uint8_t));
    for (square_t square = a1; square <= h8; ++square)
        if (square_t{square.file(), square.rank()} != square)
            return false;
    return square_t{a, _1} == a1 && square_t{h, _8} == h8;
}

constexpr bool test_square_sets() {
    using namespace chess;
    static_assert(sizeof(squares_t) == sizeof(uint64_t));
    squares_t squares{a1, h8};
    if (squares.front() != a1 || squares.back() != h8 ||
        squares.size() != 2 || squares.empty())
        return false;
    const std::array<square_t, 2> expected{a1, h8};
    size_t index{};
    for (const square_t square : squares) {
        if (index >= expected.size() || square != expected[index++])
            return false;
    }
    if (index != expected.size())
        return false;
    squares.reset(a1);
    squares.set(e4);
    squares.flip(h8);
    if (squares != squares_t{e4} || !squares[e4] || squares[h8])
        return false;
    squares.reset(e4);
    return squares.empty() && squares_t{a}.size() == 8 &&
        squares_t{_1}.size() == 8 &&
        (squares_t{a} & squares_t{_1}) == squares_t{a1} &&
        (~squares_t{}).size() == 64 &&
        (squares_t{a1} << 8) == squares_t{a2} &&
        (squares_t{h8} >> 8) == squares_t{h7} &&
        (squares_t{a1, h8} ^ squares_t{h8}) == squares_t{a1};
}

constexpr bool test_scores() {
    using namespace chess;
    static_assert(sizeof(score_t) == sizeof(int16_t));
    score_t score{P};
    score += score_t{N};
    score -= score_t{CP};
    return score == PAWN + KNIGHT - CP &&
        score_t{Q} == QUEEN && (-score_t{ROOK}) == -ROOK &&
        score_t{PAWN} * 3 == KNIGHT && score_t{ROOK} / 5 == PAWN &&
        score_t{}.draw() && score_t{WIN}.win() && score_t{LOSS}.loss() &&
        score_t{WIN}.decisive() && !score_t{PAWN}.decisive() &&
        score_t{IGNORE}.ignore() && (-score_t{IGNORE}).ignore();
}

static_assert(test_piece_and_side());
static_assert(test_coordinates());
static_assert(test_square_sets());
static_assert(test_scores());

} // namespace

int main() {
    bool passed = test_support::check(test_piece_and_side(), "piece and side encoding");
    passed &= test_support::check(test_coordinates(), "coordinate round trips");
    passed &= test_support::check(test_square_sets(), "square sets and iteration");
    passed &= test_support::check(test_scores(), "score arithmetic and classification");
    return passed ? 0 : 1;
}
