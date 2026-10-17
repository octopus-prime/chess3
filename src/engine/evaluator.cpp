module;

#include <cstdint>
#include <algorithm>
#include <iterator>
#include <memory>

module engine.evaluator;

import types.square;
import types.score;
import types.side;
import types.piece;
import types.squares;
import nnue.nnue;

namespace chess {

evaluator_t::evaluator_t()
    : _nnue(nnue::load()), _entries(std::make_unique<nnue::entry_t[]>(MAX_SIDE * MAX_SQUARE)) {
    for (size_t index = 0; index < MAX_SIDE * MAX_SQUARE; ++index)
        _nnue->initialize(_entries[index]);
}

score_t evaluator_t::evaluate(const position_t& position) {
    const nnue::entry_t& t = refresh(position, position.side());
    const nnue::entry_t& o = refresh(position, ~position.side());
    const auto side = position.side();
    auto score = _nnue->evaluate(t, o, position.by().size(),
                                !position.by(side, Q).empty(),
                                !position.by(~side, Q).empty());
    // score -= score * position.halfmove() / 212;
    return score_e(score);
}

const nnue::entry_t& evaluator_t::refresh(const position_t& position, const side_e side) {
    auto king = position.by(side, K).front();
    auto& entry = _entries[static_cast<size_t>(side) * MAX_SQUARE + static_cast<size_t>(king)];
    nnue::active_features(position, side, _features);
    _removed.clear();
    _added.clear();
    std::ranges::set_difference(entry.features, _features, std::back_inserter(_removed));
    std::ranges::set_difference(_features, entry.features, std::back_inserter(_added));
    _nnue->update(entry, _removed, _added);
    entry.features.swap(_features);

    return entry;
}

} // namespace chess
