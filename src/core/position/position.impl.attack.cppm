module;

#include <type_traits>

export module core.position:impl.attack;

import :decl;
import attacks.generator;
import attacks.lookup;

namespace chess {

constexpr squares_t position_t::attackers(const side_t side) const noexcept {
    using namespace attacks::generator;
    std::underlying_type_t<squares_e> attackers{};
    attackers |= leapers(by(side, K), by(side, N));
    attackers |= sliders(by(side, R, Q), by(side, B, Q), by() & ~by(~side, K));
    attackers |= pawns(by(side, P), side);
    return squares_e(attackers);
}

constexpr squares_t position_t::attackers(const square_t square) const noexcept {
    using namespace attacks::lookup;
    std::underlying_type_t<squares_e> attackers{};
    attackers |= king(square) & by(K);
    attackers |= knight(square) & by(N);
    attackers |= rook(square, by()) & by(R, Q);
    attackers |= bishop(square, by()) & by(B, Q);
    attackers |= pawn(square, WHITE) & by(BP);
    attackers |= pawn(square, BLACK) & by(WP);
    return squares_e(attackers);
}

} // namespace chess
