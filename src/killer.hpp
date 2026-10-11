#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include <ranges>

import hashes.hash;
// import types.score;
import core.move;
import types.score;

namespace chess {

struct killer_t {
    constexpr killer_t() noexcept : _buckets{} {}
    constexpr void put(const move_t move, const size_t height) noexcept {
        auto& bucket = _buckets[height];
        if (bucket[0] != move) {
            bucket[1] = bucket[0];
            bucket[0] = move;
        }
    }

    constexpr std::span<const move_t, 2> get(const size_t height) const noexcept {
        return _buckets[height];
    }

private:
    std::array<std::array<move_t, 2>, 1024> _buckets;
};

} // namespace chess
