module;

#include <algorithm>
#include <type_traits>
#include <initializer_list>

export module core.position:impl.attack;

import :decl;
import core.attacks.generator;
import core.attacks.lookup;

namespace chess {

constexpr squares_t position_t::attacked_by(const side_t side) const noexcept {
    using namespace attacks::generator;
    std::underlying_type_t<squares_e> attackers{};
    attackers |= leapers(by(side, K), by(side, N));
    attackers |= sliders(by(side, R, Q), by(side, B, Q), by() & ~by(~side, K));
    attackers |= pawns(by(side, P), side);
    return squares_e(attackers);
}

constexpr squares_t position_t::attackers_of(const square_t square) const noexcept {
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

constexpr score_t position_t::see(const move_t move) const noexcept {
    using namespace attacks::lookup;

    squares_t rook_sliders = rook(move.to(), {});
    squares_t bishop_sliders = bishop(move.to(), {});
    squares_t occupied = by();
    
    // En Passant: geschlagener Bauer steht auf Nachbarfeld
    if (at(move.from()).type() == P && move.to() == this->en_passant().front()) {
        square_t ep_pawn_sq = square_t{move.to().file(), move.from().rank()};
        occupied.reset(ep_pawn_sq);
    }

    occupied.reset(move.from());
    squares_t attackers = this->attackers_of(move.to()) & occupied;

    auto update_xray = [&]() {
        attackers |= (rook(move.to(), occupied) & rook_sliders & by(R, Q))
                   | (bishop(move.to(), occupied) & bishop_sliders & by(B, Q));
        attackers &= occupied;
    };

    update_xray();

    int gain[32];
    int depth = 0;

    gain[depth] = score_t{at(move.to()).type()};
    type_t next_victim = at(move.from()).type();

    if (move.promotion() != NO_TYPE) {
        gain[depth] += score_t{move.promotion()} - score_t{P};
        next_victim = move.promotion();
    } else if (at(move.from()).type() == P && move.to() == this->en_passant().front()) {
        gain[depth] = score_t{P};
    }

    side_t side = ~this->side();

    while (true) {
        squares_t current = attackers & by(side);
        if (current.empty()) break;

        // Finde kleinsten Angreifer
        type_t least_type = NO_TYPE;
        square_t least_sq = NO_SQUARE;
        for (type_t t : {P, N, B, R, Q, K}) {
            squares_t subset = current & by(t);
            if (!subset.empty()) {
                least_type = t;
                least_sq = subset.front();
                break;
            }
        }

        // KÖNIGS-REGEL: Wenn der König schlägt, darf das Zielfeld 
        // nicht mehr vom Gegner verteidigt sein!
        if (least_type == K) {
            if (attackers & by(~side)) break; // König darf nicht ins Schach
        }

        depth++;
        gain[depth] = score_t{next_victim} - gain[depth - 1];
        next_victim = least_type;

        occupied.reset(least_sq);
        attackers.reset(least_sq);
        update_xray();

        if (least_type == P && (squares_t{move.to()} & squares_t{_1, _8})) {
            gain[depth] += score_t{Q} - score_t{P};
            next_victim = Q;
        }

        side = ~side;
    }

    while (depth > 0) {
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
        --depth;
    }

    return score_e(gain[0]);
}

// constexpr score_t position_t::see(const move_t move) const noexcept {
//     using namespace attacks::lookup;

//     auto find_least_piece = [this](squares_t board) -> square_t {
//         for (type_t type : {P, N, B, R, Q, K}) {
//             squares_t pieces = board & by(type);
//             if (!pieces.empty()) {
//                 return pieces.front();
//             }
//         }
//         return NO_SQUARE;
//     };

//     squares_t rook_sliders = rook(move.to(), {});
//     squares_t bishop_sliders = bishop(move.to(), {});
//     squares_t occupied = by();
//     squares_t attackers = this->attackers_of(move.to());
//     squares_t blockers = {}; // this->states.back().blockers[WHITE] | this->states.back().blockers[BLACK];
//     attackers.reset(move.from());
//     occupied.reset(move.from());

//     auto expand_attackers = [&](square_t from) {
//         switch (this->at(from).type()) {
//             case P:
//                 if (from.file() == move.to().file()) {
//                     attackers |= rook(from, occupied) & rook_sliders & by(R, Q) & occupied;
//                 } else {
//                     attackers |= bishop(from, occupied) & bishop_sliders & by(B, Q) & occupied;
//                 }
//                 break;
//             case B:
//                 attackers |= bishop(from, occupied) & bishop_sliders & by(B, Q) & occupied;
//                 break;
//             case R:
//                 attackers |= rook(from, occupied) & rook_sliders & by(R, Q) & occupied;
//                 break;
//             case Q:
//                 attackers |= bishop(from, occupied) & bishop_sliders & by(B, Q) & occupied;
//                 attackers |= rook(from, occupied) & rook_sliders & by(R, Q) & occupied;
//                 break;
//             default:
//                 break;
//         }

//         attackers &= ~blockers;
//     };

//     // std::println("attackers: {}", attackers);
//     expand_attackers(move.from());

//     // std::println("attackers: {}", attackers);

//     int gain[32];
//     int depth = 1;

//     gain[0] = score_t{at(move.to()).type()};

//     type_e attacked_piece_type = at(move.from()).type();

//     if (move.promotion() != NO_TYPE) {
//         gain[0] += score_t{move.promotion()} - score_t{P};
//         attacked_piece_type = move.promotion();
//     }
    
//     squares_t en_passant = this->en_passant();
//     if (!en_passant.empty() && en_passant.front() == move.to()) {
//         gain[0] += score_t{P};
//         attacked_piece_type = P;
//     }

//     side_t side = ~this->side();
    
//     // std::println("in");

//     while (!attackers.empty()) {
//         squares_t current = attackers & by(side);

//         if (current.empty()) break;

//         square_t least_sq = find_least_piece(current);

//         gain[depth] = score_t{attacked_piece_type} - gain[depth-1];

//         attacked_piece_type = at(least_sq).type();
//         attackers.reset(least_sq);
//         occupied.reset(least_sq);

//         if (attacked_piece_type == P && squares_t{move.to()} & squares_t{_1, _8}) {
//             gain[depth] += score_t{Q} - score_t{P};
//             attacked_piece_type = Q;
//         }

//         expand_attackers(least_sq);

//         side = ~side;
//         depth++;
//     }

//     while (--depth)
//         gain[depth-1]= -std::max(-gain[depth-1], gain[depth]);

//     // std::println("out");

//     return score_e(gain[0]);

// }

} // namespace chess
