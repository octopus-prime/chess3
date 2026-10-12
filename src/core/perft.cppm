module;

#include <cstddef>
#include <string_view>

export module core.perft;

import types.ply;
import core.position;

export namespace chess {

struct perft_t final {
    explicit perft_t(const std::string_view fen) noexcept;

    size_t operator()(ply_t depth) noexcept;

private:
    position_t position;
};

} // namespace chess
