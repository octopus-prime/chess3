module;

#include <cstdint>
#include <algorithm>
#include <memory>
#include <span>
#include <cassert>

module engine.evaluator;

import types.square;
import types.score;
import types.side;
import types.piece;
import types.squares;
import nnue.nnue;

namespace chess {

evaluator_t::evaluator_t()
    : _nnue(nnue::load()), _entries(std::make_unique<nnue::entry_t[]>(MAX_SIDE * MAX_SQUARE)),
      _stack(std::make_unique<frame_t[]>(MAX_PLY)) {
    for (size_t index = 0; index < MAX_SIDE * MAX_SQUARE; ++index)
        _nnue->initialize(_entries[index]);
}

void evaluator_t::reset_stack() noexcept {
    _height = 0;
    _stack[0].state.reset();
}

void evaluator_t::push() noexcept {
    assert(_height + 1 < MAX_PLY);
    _stack[++_height].state.reset();
}

void evaluator_t::pop() noexcept {
    assert(_height > 0);
    --_height;
}

score_t evaluator_t::evaluate_incremental(const position_t& position) noexcept {
    auto& current = _stack[_height];
    if (!current.state) {
        size_t ancestor = _height;
        while (ancestor > 0 && !_stack[--ancestor].state) {}
        const auto& previous = _stack[ancestor];
        current.state.emplace(position);
        const auto& state = *current.state;
        std::array<bool, 2> incremental{};
        for (const side_t side : {WHITE, BLACK})
            incremental[side] = ancestor < _height && previous.state &&
                nnue::compatible(*previous.state, state, side);
        std::array<std::span<uint32_t>, 2> features;
        if (!incremental[WHITE] || !incremental[BLACK])
            features = nnue::active_features(position, _features);
        std::array<std::array<std::span<uint32_t>, 2>, 2> changes;
        if (incremental[WHITE] || incremental[BLACK])
            changes = nnue::changed_features(*previous.state, state, _removed, _added);
        for (const side_t side : {WHITE, BLACK}) {
            auto& entry = current.entries[side];
            if (incremental[side]) {
                std::ranges::copy(previous.entries[side].accumulation, entry.accumulation);
                _nnue->update(entry, changes[side][0], changes[side][1]);
            } else {
                const auto& cached = refresh(position, side, features[side]);
                std::ranges::copy(cached.accumulation, entry.accumulation);
            }
        }
    }
    const auto side = position.side();
    return score_e(_nnue->evaluate(current.entries[side], current.entries[~side], position));
}

score_t evaluator_t::evaluate(const position_t& position) noexcept {
    const auto features = nnue::active_features(position, _features);
    const nnue::entry_t& t = refresh(position, position.side(), features[position.side()]);
    const nnue::entry_t& o = refresh(position, ~position.side(), features[~position.side()]);
    auto score = _nnue->evaluate(t, o, position);
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
    // Mark cached features, then clear those still present in the new position.
    for (const auto feature : previous)
        _membership[feature] = 1;
    for (const auto feature : features) {
        _added[side][added_count] = feature;
        added_count += !_membership[feature];
        _membership[feature] = 0;
    }
    for (const auto feature : previous) {
        _removed[side][removed_count] = feature;
        removed_count += _membership[feature];
        _membership[feature] = 0;
    }
    _nnue->update(entry, std::span{_removed[side]}.first(removed_count),
                  std::span{_added[side]}.first(added_count));
    std::ranges::copy(features, entry.features.begin());
    entry.feature_count = features.size();

    return entry;
}

} // namespace chess
