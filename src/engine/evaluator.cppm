module;

#include <memory>
#include <cstdint>
#include <array>
#include <span>

export module engine.evaluator;

import types.square;
import types.score;
import types.side;
import core.position;
import nnue.nnue;

export namespace chess {

struct evaluator_t final {
    evaluator_t();
    score_t evaluate(const position_t& position) noexcept;

private:
    const nnue::entry_t& refresh(const position_t& position, side_e side,
                                std::span<const uint32_t> features) noexcept;

    std::unique_ptr<nnue::nnue_t> _nnue;
    std::unique_ptr<nnue::entry_t[]> _entries;
    std::array<nnue::feature_buffer_t, 2> _features;
    nnue::feature_buffer_t _removed;
    nnue::feature_buffer_t _added;
    std::array<uint8_t, nnue::L0> _membership{};
};

} // namespace chess
