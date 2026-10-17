module;

#include <memory>
#include <cstdint>
#include <vector>

export module engine.evaluator;

import types.square;
import types.score;
import types.side;
import core.position;
import nnue.nnue;

export namespace chess {

struct evaluator_t final {
    evaluator_t();
    score_t evaluate(const position_t& position);

private:
    const nnue::entry_t& refresh(const position_t& position, side_e side);

    std::unique_ptr<nnue::nnue_t> _nnue;
    std::unique_ptr<nnue::entry_t[]> _entries;
    std::vector<uint32_t> _features;
    std::vector<uint32_t> _removed;
    std::vector<uint32_t> _added;
};

} // namespace chess
