module;

#include <array>
#include <span>
#include <cstddef>

#include <cstdint>
#include <vector>
#include <optional>
#include <algorithm>
#include <tuple>
#include <type_traits>
#include <ranges>

export module engine.transposition;

import types.hash;
import types.score;
import types.ply;
import core.move;

export namespace chess {

enum flag_e : std::uint8_t {
	UNKNOWN,
	UPPER,
	LOWER,
	EXACT
};

using flag_t = flag_e;

struct transposition_t final {
	struct entry_t final {
		using key_t = std::tuple<uint16_t, uint16_t>;
		key_t key;		//2*2
		move_t move;	//2
		score_t score;	//2
		flag_t flag;	//1
		ply_t depth;	//1
	};
    transposition_t(size_t mb = 16);
    void clear() noexcept;
    void put(const hash_t hash, const move_t move, const score_t score, const flag_e flag,
             const ply_t depth, const ply_t height) noexcept;
    std::optional<entry_t> get(const hash_t hash, const ply_t height) const noexcept;
    size_t full() const noexcept;

private:
	static_assert(sizeof(entry_t) == 10);

	constexpr static size_t BUCKET_SIZE = 64 / sizeof(entry_t);

	static_assert(BUCKET_SIZE == 6);

	struct alignas(64) bucket_t {
		entry_t entries[BUCKET_SIZE]{};
	};

	static_assert(sizeof(bucket_t) == 64);

   	constexpr static size_t BUCKETS_PER_MB = 1024 * 1024 / sizeof(bucket_t);

    std::vector<bucket_t> _buckets;
	size_t _used;
};

constexpr score_t score_to_tt(const score_t score, const ply_t height) noexcept {
    if (score.ignore())
        return score;
    // Store mate distance relative to this position, not the search root.
    const int ply = static_cast<int>(height);
    return score.win() ? score_t(score_e(score + ply))
        : score.loss() ? score_t(score_e(score - ply)) : score;
}

constexpr score_t score_from_tt(const score_t score, const ply_t height) noexcept {
    if (score.ignore())
        return score;
    const int ply = static_cast<int>(height);
    return score.win() ? score_t(score_e(score - ply))
        : score.loss() ? score_t(score_e(score + ply)) : score;
}

} // namespace chess
