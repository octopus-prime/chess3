module;

#include <cassert>

export module core.position:impl.execute;

import :decl;
import :impl.construct;
import :impl.query;
import :impl.attack;

namespace chess {

constexpr bool is_castling_move(const side_t side, const type_t type,
                               const square_t from, const square_t to) noexcept {
    const rank_t rank = side == WHITE ? _1 : _8;
    return type == K && from == square_t{e, rank} &&
           (to == square_t{g, rank} || to == square_t{c, rank});
}

constexpr void position_t::do_move(const move_t move) noexcept {
    const square_t from = move.from();
    const square_t to = move.to();
    const piece_t piece = _piece_at_square[from];
    const type_t promotion = move.promotion();
    const bool castle = is_castling_move(_side, piece.type(), from, to);
    const bool ep = piece.type() == P && en_passant()[to] &&
                    _piece_at_square[to] == NO_PIECE && from.file() != to.file();
    const square_t captured_square = ep ? square_t{to.file(), from.rank()} : to;
    const piece_t captured = _piece_at_square[captured_square];

    assert(from != to);
    assert(piece != NO_PIECE && piece.side() == _side);
    assert(captured == NO_PIECE || captured.side() != _side);
    assert(captured.type() != K);
    assert(promotion == NO_TYPE || promotion == N || promotion == B ||
           promotion == R || promotion == Q);
    assert(promotion == NO_TYPE || piece.type() == P);
    assert(piece.type() != P ||
           ((to.rank() == (_side == WHITE ? _8 : _1)) ==
            (promotion != NO_TYPE)));
    if (ep) {
        assert(to.rank() == (_side == WHITE ? _6 : _3));
        assert(from.rank() == (_side == WHITE ? _5 : _4));
        assert(int(from.file()) - int(to.file()) == 1 ||
               int(to.file()) - int(from.file()) == 1);
        assert(captured == piece_t(~_side, P));
    }

    if (castle) {
        const square_t rook_from{to.file() == g ? h : a, from.rank()};
        const squares_t path = to.file() == g
            ? squares_t{square_t{f, from.rank()}, to}
            : squares_t{square_t{b, from.rank()}, to, square_t{d, from.rank()}};
        assert(captured == NO_PIECE);
        assert(_piece_at_square[rook_from] == piece_t(_side, R));
        assert(castling()[rook_from]);
        assert((by() & path).empty());
        assert(_checkers.empty());
    }

    _stack.push(state_t{_checkers, _special, _hash, captured, captured_square});

    const squares_t source{from};
    const squares_t target{to};
    if (captured != NO_PIECE) {
        _hash ^= hashes::lookup::piece(captured, captured_square);
        _piece_at_square[captured_square] = NO_PIECE;
        _occupied_by_type[captured.type()].reset(captured_square);
        _occupied_by_side[captured.side()].reset(captured_square);
    }

    const piece_t placed = promotion == NO_TYPE ? piece : piece_t{_side, promotion};
    _hash ^= hashes::lookup::piece(piece, from) ^ hashes::lookup::piece(placed, to);
    _piece_at_square[from] = NO_PIECE;
    _piece_at_square[to] = placed;
    _occupied_by_type[piece.type()] &= ~source;
    _occupied_by_type[placed.type()] |= target;
    _occupied_by_side[_side] &= ~source;
    _occupied_by_side[_side] |= target;

    if (castle) {
        const square_t rook_from{to.file() == g ? h : a, from.rank()};
        const square_t rook_to{to.file() == g ? f : d, from.rank()};
        const piece_t rook{_side, R};
        _hash ^= hashes::lookup::piece(rook, rook_from) ^ hashes::lookup::piece(rook, rook_to);
        _piece_at_square[rook_from] = NO_PIECE;
        _piece_at_square[rook_to] = piece_t{_side, R};
        _occupied_by_type[R].reset(rook_from);
        _occupied_by_type[R].set(rook_to);
        _occupied_by_side[_side].reset(rook_from);
        _occupied_by_side[_side].set(rook_to);
    }

    _special.reset(en_passant());
    if (piece.type() == P && from.file() == to.file() &&
        from.rank() == (_side == WHITE ? _2 : _7) &&
        to.rank() == (_side == WHITE ? _4 : _5)) {
        _special.set(square_t{from.file(), _side == WHITE ? _3 : _6});
    }

    _special.reset((source | target) & squares_t{a1, h1, a8, h8});
    if (piece.type() == K) {
        const rank_t rank = _side == WHITE ? _1 : _8;
        _special.reset(squares_t{square_t{a, rank}, square_t{h, rank}});
    }

    for (const square_t square : _stack.top().special ^ _special) {
        _hash ^= square.rank() == _1 || square.rank() == _8
            ? hashes::lookup::castle(square)
            : hashes::lookup::ep(square.file());
    }
    _hash ^= hashes::lookup::side();
    _side = ~_side;
    _checkers = attackers_of(by(_side, K).front()) & by(~_side);
}

constexpr void position_t::undo_move(const move_t move) noexcept {
    const state_t& state = _stack.top();
    const square_t from = move.from();
    const square_t to = move.to();
    const side_t side = ~_side;
    const piece_t placed = _piece_at_square[to];
    const type_t promotion = move.promotion();
    const bool castle = is_castling_move(side, placed.type(), from, to);

    assert(from != to);
    assert(_piece_at_square[from] == NO_PIECE);
    assert(placed != NO_PIECE && placed.side() == side);
    assert(state.captured_square == to ||
           (placed.type() == P && state.captured == piece_t(~side, P) &&
            (state.special & squares_t{_3, _6})[to] &&
            state.captured_square == square_t(to.file(), from.rank())));
    assert(promotion == NO_TYPE || promotion == N || promotion == B ||
           promotion == R || promotion == Q);
    assert(promotion == NO_TYPE || placed.type() == promotion);

    const piece_t piece = promotion == NO_TYPE ? placed : piece_t{side, P};
    const squares_t source{from};
    const squares_t target{to};
    _piece_at_square[to] = NO_PIECE;
    _piece_at_square[from] = piece;
    _occupied_by_type[placed.type()] &= ~target;
    _occupied_by_type[piece.type()] |= source;
    _occupied_by_side[side] &= ~target;
    _occupied_by_side[side] |= source;

    if (castle) {
        const square_t rook_from{to.file() == g ? h : a, from.rank()};
        const square_t rook_to{to.file() == g ? f : d, from.rank()};
        assert(state.captured == NO_PIECE);
        assert(_piece_at_square[rook_from] == NO_PIECE);
        assert(_piece_at_square[rook_to] == piece_t(side, R));
        _piece_at_square[rook_to] = NO_PIECE;
        _piece_at_square[rook_from] = piece_t{side, R};
        _occupied_by_type[R].reset(rook_to);
        _occupied_by_type[R].set(rook_from);
        _occupied_by_side[side].reset(rook_to);
        _occupied_by_side[side].set(rook_from);
    }

    if (state.captured != NO_PIECE) {
        const squares_t captured_square{state.captured_square};
        _piece_at_square[state.captured_square] = state.captured;
        _occupied_by_type[state.captured.type()] |= captured_square;
        _occupied_by_side[state.captured.side()] |= captured_square;
    }

    _side = side;
    _checkers = state.checkers;
    _special = state.special;
    _hash = state.hash;
    _stack.pop();
}

}
