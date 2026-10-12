module;

#include <array>
#include <span>

export module engine.killer;

import core.hashes.lookup;
import core.move;
import types.score;
import types.ply;

export namespace chess {

struct killer_t final {
    constexpr killer_t() noexcept : _buckets{} {}
    constexpr void put(const move_t move, const ply_t height) noexcept {
        auto& bucket = _buckets[height];
        if (bucket[0] != move) {
            bucket[1] = bucket[0];
            bucket[0] = move;
        }
    }

    constexpr std::span<const move_t, 2> get(const ply_t height) const noexcept {
        return _buckets[height];
    }

private:
    std::array<std::array<move_t, 2>, MAX_PLY> _buckets;
};

} // namespace chess
