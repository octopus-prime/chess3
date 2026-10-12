module;

#include <cstddef>
#include <string_view>

export module core.perft;

import core.position;

export namespace chess {

struct perft_t {
    explicit perft_t(const std::string_view fen) noexcept;

    size_t operator()(size_t depth) noexcept;

private:
    position_t position;
};

} // namespace chess
