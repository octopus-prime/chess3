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

constexpr score_t score_to_tt(const score_t score, const size_t height) noexcept {
    if (score.ignore())
        return score;
    // Store mate distance relative to this position, not the search root.
    const int ply = static_cast<int>(height);
    return score.win() ? score_t(score_e(score + ply))
        : score.loss() ? score_t(score_e(score - ply)) : score;
}

constexpr score_t score_from_tt(const score_t score, const size_t height) noexcept {
    if (score.ignore())
        return score;
    const int ply = static_cast<int>(height);
    return score.win() ? score_t(score_e(score - ply))
        : score.loss() ? score_t(score_e(score + ply)) : score;
}

enum flag_e : std::uint8_t {
	UNKNOWN,
	UPPER,
	LOWER,
	EXACT
};

// struct flag_t {
// 	using enum_t = flag_e;
// 	using value_t = std::underlying_type_t<enum_t>;

// 	constexpr flag_t() noexcept = default;
// 	constexpr flag_t(flag_e flag, bool replace = false) noexcept : value(flag), replace(replace) {}
// 	constexpr operator flag_e() const noexcept { return flag_e(value); }
// 	constexpr operator bool() const noexcept { return replace; }

// private:
// 	value_t value:7;
// 	value_t replace:1;
// };

class transposition_t {
	struct entry_t {
		using key_t = std::tuple<uint16_t, uint16_t>;
		key_t key;		//2*2
		move_t move;	//2
		score_t score;	//2
		// flag_t flag;	//1
		flag_e flag;	//1
		uint8_t	depth;	//1
	};

	static_assert(sizeof(entry_t) == 10);

	constexpr static size_t BUCKET_SIZE = 64 / sizeof(entry_t);

	static_assert(BUCKET_SIZE == 6);

	struct alignas(64) bucket_t {
		entry_t entries[BUCKET_SIZE]{};
	};

	static_assert(sizeof(bucket_t) == 64);

    std::vector<bucket_t> buckets;
	std::size_t used{0};

    constexpr static size_t BUCKETS_PER_MB = 1024 * 1024 / sizeof(bucket_t);

public:
    transposition_t(size_t mb = 16) : buckets(mb * BUCKETS_PER_MB + 137) {}
    // transposition_t() : buckets(1'000'037) {}
    // transposition_t() : buckets(499'999) {}
    // transposition_t() : buckets(250'007) {}
    // transposition_t() : buckets(125'003) {}

    void clear() noexcept {
        std::ranges::fill(buckets, bucket_t{});
		used = 0;
    }

    void put(const hash_t hash, const move_t move, const score_t score, const flag_e flag,
             const uint8_t depth, const size_t height) noexcept {
		const entry_t::key_t key = {uint16_t(hash >> 43), uint16_t(hash >> 23)};
        bucket_t& bucket = buckets[hash % buckets.size()];
		entry_t* entry = std::ranges::find(bucket.entries, key, &entry_t::key);
		if (entry == std::end(bucket.entries)) {
			entry = std::ranges::min_element(bucket.entries, {}, &entry_t::depth);
		}
		used += entry->flag == UNKNOWN;
		*entry = {key, move, score_to_tt(score, height), flag, depth};
    }

    std::optional<entry_t> get(const hash_t hash, const size_t height) const noexcept {
		const entry_t::key_t key = {uint16_t(hash >> 43), uint16_t(hash >> 23)};
        const bucket_t& bucket = buckets[hash % buckets.size()];
		const entry_t* entry = std::ranges::find(bucket.entries, key, &entry_t::key);
		if (entry == std::end(bucket.entries) || entry->flag == UNKNOWN)
			return std::nullopt;
        entry_t result = *entry;
        result.score = score_from_tt(result.score, height);
        return result;
    }

    size_t full() const noexcept {
		return size_t(1000.0 * double(used) / double(buckets.size() * BUCKET_SIZE));
    }
};

} // namespace chess
