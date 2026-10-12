module;

#include <functional>
#include <tuple>

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

constexpr std::tuple<squares_t, squares_t> position_t::find_snipers_and_blockers(const side_t side) const noexcept {
    using namespace attacks;
    using namespace attacks::lookup;

    const square_t ksq = by(side, K).front();

    squares_t snipers{};
    squares_t blockers{};

    auto scan = [&](direction_e direction, squares_t candidates, auto&& find_nearest) {
        const squares_t ray = lookup::ray(ksq, direction);
        const squares_t sniper_candidates = ray & candidates;
        if (!sniper_candidates.empty()) {
            square_t sniper = std::invoke(find_nearest, sniper_candidates);
            const squares_t line = lookup::line(ksq, sniper);
            const squares_t between = line & by() & ~squares_t{ksq} & ~squares_t{sniper};
            const squares_t blocker_candidates = between & by(side);
            if (between.size() == 1 && blocker_candidates.size() == 1) {
                square_t blocker = blocker_candidates.front();
                snipers.set(sniper);
                blockers.set(blocker);
            }
        }
    };

    scan(NORTH, by(~side, R, Q), &squares_t::front);
    scan(SOUTH, by(~side, R, Q), &squares_t::back);
    scan(EAST, by(~side, R, Q), &squares_t::front);
    scan(WEST, by(~side, R, Q), &squares_t::back);
    scan(NORTH_EAST, by(~side, B, Q), &squares_t::front);
    scan(NORTH_WEST, by(~side, B, Q), &squares_t::front);
    scan(SOUTH_EAST, by(~side, B, Q), &squares_t::back);
    scan(SOUTH_WEST, by(~side, B, Q), &squares_t::back);

    return {snipers, blockers};
}

} // namespace chess
