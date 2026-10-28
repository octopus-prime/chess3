module;

#include <cstdint>
#include <span>

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

constexpr move_t position_t::last_move() const noexcept {
    return _last_move;
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

constexpr bool position_t::check() const noexcept {
    return !checkers().empty();
}

constexpr bool position_t::tactical(const move_t move) const noexcept {
    return move.promotion() != NO_TYPE || at(move.to()).type() != NO_TYPE || (at(move.from()).type() == P && en_passant()[move.to()]);
}

constexpr int position_t::mvv_lva(const move_t move) const noexcept {
    const auto attacker = at(move.from()).type();
    auto victim = at(move.to()).type();
    if (attacker == P && en_passant()[move.to()])
        victim = P;
    return 16 * int(score_t{victim}) + int(score_t{move.promotion()}) -
        int(score_t{attacker});
}

constexpr bool position_t::fifty_move_rule() const noexcept {
    return _halfmove >= 100;
}

constexpr bool position_t::threefold_repetition() const noexcept {
    return _repetition >= 3;
}

constexpr uint8_t position_t::repetition() const noexcept {
    return _repetition;
}

constexpr uint8_t position_t::halfmove() const noexcept {
    return _halfmove;
}

constexpr bool position_t::insufficient_material() const noexcept {
    return (by(P) | by(R, Q)).empty() && by(WHITE, N, B).size() <= 1 && by(BLACK, N, B).size() <= 1;
    // if (!(by(P) | by(R, Q)).empty())
    //     return false;

    // const squares_t bishops = by(B);
    // const squares_t knights = by(N);
    // if ((bishops | knights).size() <= 1)
    //     return true;
    // if (!knights.empty())
    //     return false;

    // const square_t first = bishops.front();
    // const int color = (int(first.file()) + int(first.rank())) & 1;
    // for (const square_t square : bishops)
    //     if (((int(square.file()) + int(square.rank())) & 1) != color)
    //         return false;
    // return true;
}

// constexpr uint8_t position_t::find_repetitions(const hash_t hash, const uint8_t halfmove) const noexcept {
//     const std::span<const state_t> states = _stack.data();
//     int s = states.size();
//     int limit = std::max(0, s - halfmove);
//     for (int i = s - 2; i >= limit; i -= 2) {
//         if (states[i].hash == hash)
//             return states[i].repetition;
//     }
//     return 0;
// }

constexpr uint8_t position_t::find_repetitions(
    const hash_t hash, const uint8_t halfmove) const noexcept {
    const std::span<const state_t> states = _stack.data();
    const int s = static_cast<int>(states.size());
    const int limit = std::max(0, s - halfmove);

    for (int i = s - 1; i >= limit; --i) {
        if ((s - i) % 2 == 0 && states[i].hash == hash)
            return states[i].repetition;

        if (states[i].last_move == move_t{})
            break;
    }
    return 0;
}

} // namespace chess
