module;

#include <cstdint>
#include <algorithm>
#include <limits>
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

evaluator_t::evaluator_t()
    : _nnue(nnue::load()), _entries(std::make_unique<nnue::entry_t[]>(MAX_SIDE * MAX_SQUARE)) {
    for (size_t index = 0; index < MAX_SIDE * MAX_SQUARE; ++index)
        _nnue->initialize(_entries[index]);
}

score_t evaluator_t::evaluate(const position_t& position) noexcept {
    const auto features = nnue::active_features(position, _features);
    const nnue::entry_t& t = refresh(position, position.side(), features[position.side()]);
    const nnue::entry_t& o = refresh(position, ~position.side(), features[~position.side()]);
    const auto side = position.side();
    auto score = _nnue->evaluate(t, o, position.by().size(),
                                !position.by(side, Q).empty(),
                                !position.by(~side, Q).empty());
    // score -= score * position.halfmove() / 212;
    return score_e(score);
}

const nnue::entry_t& evaluator_t::refresh(const position_t& position, const side_e side,
                                        const std::span<const uint32_t> features) noexcept {
    auto king = position.by(side, K).front();
    auto& entry = _entries[static_cast<size_t>(side) * MAX_SQUARE + static_cast<size_t>(king)];
    size_t removed_count = 0;
    size_t added_count = 0;
    const auto previous = std::span{entry.features}.first(entry.feature_count);
    auto& membership = _membership[side];
    auto& membership_entry = _membership_entries[side];
    auto& epoch = _membership_epochs[side];
    // Reserve fresh marks for both a king-cache switch and the current features.
    if (epoch >= std::numeric_limits<uint16_t>::max() - 1) {
        membership.fill(0);
        membership_entry = nullptr;
        epoch = 1;
    }
    if (membership_entry != &entry) {
        ++epoch;
        for (const auto feature : previous)
            membership[feature] = epoch;
        membership_entry = &entry;
    }
    const auto previous_epoch = epoch++;
    for (const auto feature : features) {
        _added[added_count] = feature;
        added_count += membership[feature] != previous_epoch;
        membership[feature] = epoch;
    }
    for (const auto feature : previous) {
        _removed[removed_count] = feature;
        removed_count += membership[feature] != epoch;
    }
    _nnue->update(entry, std::span{_removed}.first(removed_count),
                  std::span{_added}.first(added_count));
    std::ranges::copy(features, entry.features.begin());
    entry.feature_count = features.size();

    return entry;
}

} // namespace chess
