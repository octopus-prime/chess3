module;

#include <memory>
#include <cstdint>
#include <array>
#include <span>
#include <optional>

export module engine.evaluator;

import types.square;
import types.score;
import types.side;
import core.position;
import nnue.nnue;
import types.ply;

export namespace chess {

struct evaluator_t final {
    evaluator_t();
    score_t evaluate(const position_t& position) noexcept;
    score_t evaluate_incremental(const position_t& position) noexcept;
    void reset_stack() noexcept;
    void push() noexcept;
    void pop() noexcept;

private:
    struct frame_t final {
        std::array<nnue::accumulator_t, 2> entries;
        std::optional<nnue::feature_state_t> state;
    };
    const nnue::entry_t& refresh(const position_t& position, side_e side,
                                std::span<const uint32_t> features) noexcept;

    std::unique_ptr<nnue::nnue_t> _nnue;
    std::unique_ptr<nnue::entry_t[]> _entries;
    std::array<nnue::feature_buffer_t, 2> _features;
    std::array<nnue::feature_buffer_t, 2> _removed;
    std::array<nnue::feature_buffer_t, 2> _added;
    std::array<uint8_t, nnue::L0> _membership{};
    std::unique_ptr<frame_t[]> _stack;
    size_t _height{};
};

} // namespace chess
