module;

#include <array>
#include <algorithm>
#include <array>
#include <cstdint>

export module engine.history;

import types.piece;
import types.square;
import types.ply;

// decl

export namespace chess {

struct history_t final {
    static constexpr int LIMIT = 16384;

    constexpr int get(const piece_t piece, const square_t to) const noexcept;
    constexpr void update(const piece_t piece, const square_t to, int bonus) noexcept;
    constexpr void clear() noexcept;
    static constexpr int bonus(const ply_t depth) noexcept;

private:
    std::array<std::array<int16_t, MAX_SQUARE>, MAX_PIECE> _entries{};
};

} // namespace chess

// impl

namespace chess {

constexpr int history_t::get(const piece_t piece, const square_t to) const noexcept {
    return _entries[piece][to];
}

constexpr void history_t::update(const piece_t piece, const square_t to, int bonus) noexcept {
    bonus = std::clamp(bonus, -LIMIT, LIMIT);
    const int magnitude = bonus < 0 ? -bonus : bonus;
    auto& entry = _entries[piece][to];
    const int value = entry;
    entry = static_cast<int16_t>(value + bonus - value * magnitude / LIMIT);
}

constexpr void history_t::clear() noexcept {
    for (auto& entry : _entries)
        entry.fill(0);
}

constexpr int history_t::bonus(const ply_t depth) noexcept {
    const int bounded_depth = std::min(int{depth}, 12);
    return std::min(16 * bounded_depth * bounded_depth, 2048);
}

} // namespace chess
