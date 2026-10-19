module;

#include <array>
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>

export module engine.history;

import types.piece;
import types.square;
import types.ply;
import types.hash;
import types.side;

// decl

export namespace chess {

struct history_t final {
    static constexpr int LIMIT = 16384;

    constexpr int get(const piece_t piece, const square_t to) const noexcept;
    constexpr int score(piece_t piece, square_t to, const history_t* continuation) const noexcept;
    constexpr void update(const piece_t piece, const square_t to, int bonus) noexcept;
    constexpr void clear() noexcept;
    static constexpr int bonus(const ply_t depth) noexcept;

private:
    std::array<std::array<int16_t, MAX_SQUARE>, MAX_PIECE> _entries{};
};

struct continuation_history_t final {
    history_t& get(const piece_t previous_piece, const square_t previous_to) noexcept;
    void clear() noexcept;

private:
    std::unique_ptr<std::array<history_t, MAX_PIECE * MAX_SQUARE>> _entries =
        std::make_unique<std::array<history_t, MAX_PIECE * MAX_SQUARE>>();
};

struct correction_history_t final {
    static constexpr size_t ENTRY_COUNT = 16384;
    static constexpr int LIMIT = 512;

    std::optional<int> get(hash_t key, side_t side) const noexcept;
    void update(hash_t key, side_t side, int error, ply_t depth) noexcept;
    void clear() noexcept;

private:
    struct entry_t final {
        hash_t key{};
        std::array<int16_t, MAX_SIDE> values{};
        std::array<bool, MAX_SIDE> initialized{};
        bool occupied{};
    };
    std::unique_ptr<std::array<entry_t, ENTRY_COUNT>> _entries =
        std::make_unique<std::array<entry_t, ENTRY_COUNT>>();
};

} // namespace chess

// impl

namespace chess {

constexpr int history_t::get(const piece_t piece, const square_t to) const noexcept {
    return _entries[piece][to];
}

constexpr int history_t::score(const piece_t piece, const square_t to,
                               const history_t* continuation) const noexcept {
    return get(piece, to) + (continuation ? continuation->get(piece, to) : 0);
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

history_t& continuation_history_t::get(const piece_t previous_piece,
                                      const square_t previous_to) noexcept {
    return (*_entries)[previous_piece * MAX_SQUARE + previous_to];
}

void continuation_history_t::clear() noexcept {
    for (auto& entry : *_entries)
        entry.clear();
}

std::optional<int> correction_history_t::get(const hash_t key, const side_t side) const noexcept {
    const auto& entry = (*_entries)[key % ENTRY_COUNT];
    return entry.occupied && entry.key == key && entry.initialized[side]
        ? std::optional<int>{entry.values[side]}
        : std::nullopt;
}

void correction_history_t::update(const hash_t key, const side_t side,
                                  int error, const ply_t depth) noexcept {
    auto& entry = (*_entries)[key % ENTRY_COUNT];
    if (!entry.occupied || entry.key != key)
        entry = {key, {}, {}, true};
    error = std::clamp(error, -LIMIT, LIMIT);
    auto& value = entry.values[side];
    value = static_cast<int16_t>(value +
        (error - value) * int{depth} / (int{depth} + 8));
    entry.initialized[side] = true;
}

void correction_history_t::clear() noexcept {
    std::ranges::fill(*_entries, entry_t{});
}

} // namespace chess
