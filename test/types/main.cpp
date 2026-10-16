#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

#include "test_support.hpp"

import types.piece;
import types.squares;
import types.score;
import types.hash;
import types.ply;

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

constexpr bool test_string_view_pieces() {
    using namespace chess;
    constexpr std::string_view symbols{"pnbrqkPNBRQK"};
    constexpr std::array<type_t, 6> types{P, N, B, R, Q, K};
    constexpr std::array<piece_t, 12> pieces{BP, BN, BB, BR, BQ, BK, WP, WN, WB, WR, WQ, WK};
    for (size_t index = 0; index < symbols.size(); ++index) {
        const auto token = symbols.substr(index, 1);
        const piece_t piece{token};
        if (piece != pieces[index] || piece.type() != types[index % types.size()] ||
            piece.side() != (index < types.size() ? BLACK : WHITE))
            return false;
        if (index < types.size() && type_t{token} != types[index])
            return false;
    }
    for (const auto token : {std::string_view{}, std::string_view{"x"},
                             std::string_view{"Q"}, std::string_view{"-"}})
        if (type_t{token} != NO_TYPE)
            return false;
    return piece_t{std::string_view{"?"}} == NO_PIECE &&
        type_t{std::string_view{"queen"}} == Q &&
        piece_t{std::string_view{"Queen"}} == WQ;
}

constexpr bool test_string_view_coordinates() {
    using namespace chess;
    constexpr std::string_view files{"abcdefgh"};
    constexpr std::string_view ranks{"12345678"};
    for (size_t file = 0; file < MAX_FILE; ++file) {
        if (file_t{files.substr(file, 1)} != file_e(file))
            return false;
        for (size_t rank = 0; rank < MAX_RANK; ++rank) {
            if (rank_t{ranks.substr(rank, 1)} != rank_e(rank))
                return false;
            const std::array text{files[file], ranks[rank]};
            const square_t square{std::string_view{text.data(), text.size()}};
            if (square != square_t{file_e(file), rank_e(rank)} ||
                square.file() != file_e(file) || square.rank() != rank_e(rank))
                return false;
        }
    }
    return file_t{std::string_view{"e4"}} == e &&
        rank_t{std::string_view{"4suffix"}} == _4 &&
        square_t{std::string_view{"e4suffix"}} == e4;
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

constexpr bool test_hashes() {
    using namespace chess;
    static_assert(std::is_same_v<hash_t, uint64_t>);
    static_assert(std::numeric_limits<hash_t>::digits == 64);
    static_assert(std::numeric_limits<hash_t>::min() == 0);
    static_assert(std::numeric_limits<hash_t>::max() == UINT64_MAX);
    constexpr hash_t high_bit = hash_t{1} << 63;
    constexpr hash_t key = 0x0123456789abcdef;
    hash_t hash{};
    if (hash != 0 || (high_bit >> 63) != 1)
        return false;
    hash ^= key;
    hash ^= high_bit;
    if (hash != (key | high_bit))
        return false;
    hash ^= high_bit;
    if (hash != key)
        return false;
    hash ^= key;
    return hash == 0 && ~hash == std::numeric_limits<hash_t>::max();
}

constexpr bool test_plies() {
    using namespace chess;
    static_assert(std::is_same_v<ply_t, uint8_t>);
    static_assert(std::numeric_limits<ply_t>::digits == 8);
    static_assert(std::numeric_limits<ply_t>::min() == 0);
    static_assert(std::numeric_limits<ply_t>::max() == 255);
    ply_t height{};
    ply_t depth = std::numeric_limits<ply_t>::max();
    for (int step = 0; step < 255; ++step) {
        if (int{height} != step || int{depth} != 255 - step)
            return false;
        ++height;
        --depth;
    }
    return height == std::numeric_limits<ply_t>::max() && depth == 0 &&
        height + 1 == 256 && depth - 1 == -1;
}

static_assert(test_piece_and_side());
static_assert(test_coordinates());
static_assert(test_string_view_pieces());
static_assert(test_string_view_coordinates());
static_assert(test_square_sets());
static_assert(test_scores());
static_assert(test_hashes());
static_assert(test_plies());

} // namespace

int main() {
    bool passed = test_support::check(test_piece_and_side(), "piece and side encoding");
    passed &= test_support::check(test_coordinates(), "coordinate round trips");
    passed &= test_support::check(test_string_view_pieces(), "piece and type construction from string views");
    passed &= test_support::check(test_string_view_coordinates(), "file, rank and square construction from string views");
    passed &= test_support::check(test_square_sets(), "square sets and iteration");
    passed &= test_support::check(test_scores(), "score arithmetic and classification");
    passed &= test_support::check(test_hashes(), "hash width and reversible XOR updates");
    passed &= test_support::check(test_plies(), "ply range and depth/height arithmetic");
    return passed ? 0 : 1;
}
