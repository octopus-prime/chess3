module;

export module core.position:impl.query;

import :decl;
import core.attacks.lookup;

namespace chess {

constexpr piece_t position_t::at(const square_t square) const noexcept {
    return _piece_at_square[square];
}

constexpr squares_t position_t::by() const noexcept {
    return _occupied_by_side[WHITE] | _occupied_by_side[BLACK];
}

constexpr squares_t position_t::by(const side_t side) const noexcept {
    return _occupied_by_side[side];
}

constexpr squares_t position_t::by(const type_t type) const noexcept {
    return _occupied_by_type[type];
}

constexpr squares_t position_t::by(const piece_t piece) const noexcept {
    return by(piece.side()) & by(piece.type());
}

constexpr squares_t position_t::by(const side_t side, const type_t type) const noexcept {
    return by(side) & by(type);
}

constexpr squares_t position_t::by(const type_t type1, const type_t type2) const noexcept {
    return by(type1) | by(type2);
}

constexpr squares_t position_t::by(const side_t side, const type_t type1, const type_t type2) const noexcept {
    return by(side) & by(type1, type2);
}

constexpr side_t position_t::side() const noexcept {
    return _side;
}

constexpr hash_t position_t::hash() const noexcept {
    return _hash;
}

constexpr squares_t position_t::checkers() const noexcept {
    return _checkers;
}

constexpr squares_t position_t::en_passant() const noexcept {
    return _special & squares_t{_3, _6};
}

constexpr squares_t position_t::castling() const noexcept {
    return _special & squares_t{_1, _8};
}

constexpr bool position_t::tactical(const move_t move) const noexcept {
    return move.promotion() != NO_TYPE || at(move.to()).type() != NO_TYPE || (at(move.from()).type() == P && en_passant()[move.to()]);
}

} // namespace chess
