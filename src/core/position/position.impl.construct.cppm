module;

#include <cassert>
#include <string_view>
#include <ranges>

export module core.position:impl.construct;

import :decl;
import :impl.query;
import :impl.attack;
import types.hash;
import core.hashes.lookup;

namespace chess {

constexpr position_t::position_t() noexcept : position_t("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {}

constexpr piece_t piece_lookup(const char ch) noexcept {
    switch (ch) {
        case 'P': return WP; case 'p': return BP;
        case 'N': return WN; case 'n': return BN;
        case 'B': return WB; case 'b': return BB;
        case 'R': return WR; case 'r': return BR;
        case 'Q': return WQ; case 'q': return BQ;
        case 'K': return WK; case 'k': return BK;
        default: return NO_PIECE;
    }
}

constexpr squares_t castle_lookup(const char ch) noexcept {
    switch (ch) {
        case 'K': return squares_t{h1}; case 'k': return squares_t{h8};
        case 'Q': return squares_t{a1}; case 'q': return squares_t{a8};
        default: return {};
    }
}

constexpr position_t::position_t(const std::string_view fen) noexcept :
    _piece_at_square{},
    _occupied_by_type{},
    _occupied_by_side{},
    _checkers{},
    _special{},
    _side{},
    _hash{},
    _stack{} {

    auto fen_parts = fen | std::views::split(' ');
    auto fen_part = fen_parts.begin();

    std::string_view board_part {*fen_part++};
    for (auto [rank_index, rank_part] : std::views::enumerate(board_part | std::views::split('/'))) {
        long file_index = 0;
        for (const char ch : rank_part) {
            if (ch >= '0' && ch <= '9') {
                file_index += ch - '0';
            } else {
                const square_t square{file_e(file_index++), rank_e(7 - rank_index)};
                const piece_t piece{piece_lookup(ch)};
                _hash ^= hashes::lookup::piece(piece, square);
                _piece_at_square[square] = piece;
                _occupied_by_type[piece.type()].set(squares_t{square});
                _occupied_by_side[piece.side()].set(squares_t{square});
            }
        }
    }

    std::string_view side_part {*fen_part++};
    if (side_part[0] == 'w') {
        _side = WHITE;
    } else {
        _side = BLACK;
    }

    std::string_view castle_part {*fen_part++};
    if (castle_part[0] != '-') {
        for (auto ch : castle_part)
            _special.set(castle_lookup(ch));
    }

    std::string_view en_passant_part {*fen_part++};
    if (en_passant_part[0] != '-')
        _special.set(square_t{file_e(en_passant_part[0] - 'a'), rank_e(en_passant_part[1] - '1')});

    if (_side == BLACK)
        _hash ^= hashes::lookup::side();
    for (const square_t square : castling())
        _hash ^= hashes::lookup::castle(square);
    for (const square_t square : en_passant())
        _hash ^= hashes::lookup::ep(square.file());

    _checkers = attackers(by(_side, K).front()) & by(~_side);
}

constexpr position_t::stack_t::stack_t() noexcept : _data{}, _size{} {}

constexpr void position_t::stack_t::clear() noexcept {
    _size = 0;
}

constexpr void position_t::stack_t::push(const state_t& state) noexcept {
    assert(_size < _data.size());
    _data[_size++] = state;
}

constexpr void position_t::stack_t::pop() noexcept {
    assert(_size != 0);
    --_size;
}

constexpr const position_t::state_t& position_t::stack_t::top() const noexcept {
    assert(_size != 0);
    return _data[_size - 1];
}

} // namespace chess
