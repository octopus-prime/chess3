module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <string_view>

module core.perft;

import core.move;

namespace chess {

perft_t::perft_t(const std::string_view fen) noexcept : position(fen) {}

size_t perft_t::operator()(size_t depth) noexcept {
    if (depth == 0)
        return 1;

    std::array<move_t, 256> buffer;
    std::span<move_t> moves = position.generate_all_moves(buffer);

    if (depth == 1)
        return moves.size();

    return std::ranges::fold_left(moves, size_t{}, [&](size_t acc, const move_t& move) {
        position.do_move(move);
        acc += (*this)(depth - 1);
        position.undo_move(move);
        return acc;
    });
}

} // namespace chess
