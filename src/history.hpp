#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

import types.piece;
import types.square;

namespace chess {

struct history_t {
    static constexpr int LIMIT = 16384;

    constexpr int get(const piece_t piece, const square_t to) const noexcept {
        return entries[piece][to];
    }

    constexpr void update(const piece_t piece, const square_t to, int bonus) noexcept {
        bonus = std::clamp(bonus, -LIMIT, LIMIT);
        const int magnitude = bonus < 0 ? -bonus : bonus;
        auto& entry = entries[piece][to];
        const int value = entry;
        entry = static_cast<int16_t>(value + bonus - value * magnitude / LIMIT);
    }

    static constexpr int bonus(const size_t depth) noexcept {
        const int bounded_depth = static_cast<int>(std::min(depth, size_t{12}));
        return std::min(16 * bounded_depth * bounded_depth, 2048);
    }

private:
    std::array<std::array<int16_t, MAX_SQUARE>, MAX_PIECE> entries{};
};

} // namespace chess
