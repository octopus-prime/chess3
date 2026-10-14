module;

#include <cstdint>
#include <memory>
#include <span>

module engine.evaluator;

import types.square;
import types.score;
import types.side;
import types.piece;
import types.squares;
import nnue.nnue;

namespace chess {

evaluator_t::evaluator_t() : _nnue(nnue::load()), _entries{} {
    for (const side_e side : {WHITE, BLACK})
        for (nnue::entry_t& entry : _entries[side])
            _nnue->initialize(entry);
}

score_t evaluator_t::evaluate(const position_t& position) noexcept {
    const nnue::entry_t& t = refresh(position, position.side());
    const nnue::entry_t& o = refresh(position, ~position.side());
    return score_e(_nnue->evaluate(t, o, position.by().size()));
}

const nnue::entry_t& evaluator_t::refresh(const position_t& position, const side_e side) noexcept {
    auto king = position.by(side, K).front();
    auto& entry = _entries[side][king];
    uint16_t added_features[32];
    uint16_t removed_features[32];
    size_t added_count = 0;
    size_t removed_count = 0;

    for (piece_t piece : {WP, WN, WB, WR, WQ, WK, BP, BN, BB, BR, BQ, BK}) {
        squares_t oldBB = squares_e(entry.pieces[piece.type()] & entry.colors[piece.side()]);
        squares_t newBB = position.by(piece);
        for (square_t square : oldBB & ~newBB)
            removed_features[removed_count++] = nnue::make_index(king, square, piece, side);
        for (square_t square : newBB & ~oldBB)
            added_features[added_count++] = nnue::make_index(king, square, piece, side);
    }

    _nnue->update(entry, std::span{removed_features}.first(removed_count), std::span{added_features}.first(added_count));

    for (side_e c : {WHITE, BLACK})
        entry.colors[c] = position.by(c);
    for (type_e p : {P, N, B, R, Q, K})
        entry.pieces[p] = position.by(p);

    return entry;
}

} // namespace chess
