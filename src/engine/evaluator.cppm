module;

#include <memory>

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
    const nnue::entry_t& refresh(const position_t& position, const side_e side) noexcept;

    std::unique_ptr<nnue::nnue_t> _nnue;
    nnue::entry_t _entries[MAX_SIDE][MAX_SQUARE];
};

} // namespace chess
