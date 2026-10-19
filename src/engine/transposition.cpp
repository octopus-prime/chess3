module;

#include <array>
#include <span>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <optional>
#include <algorithm>

module engine.transposition;

import types.hash;
import types.score;
import types.ply;
import core.move;

namespace chess {

constexpr transposition_t::entry_t::key_t make_key(const hash_t hash) noexcept {
    return {uint16_t(hash >> 43), uint16_t(hash >> 23)};
}

transposition_t::transposition_t(size_t mb)
    : _buckets(mb * BUCKETS_PER_MB + 137), _evaluations(EVALUATION_COUNT), _used() {}

void transposition_t::clear() noexcept {
    std::ranges::fill(_buckets, bucket_t{});
    std::ranges::fill(_evaluations, evaluation_t{});
    _used = 0;
}

void transposition_t::put(const hash_t hash, const move_t move, const score_t score, const flag_e flag,
                          const ply_t depth, const ply_t height) noexcept {
    const entry_t::key_t key = make_key(hash);
    bucket_t& bucket = _buckets[hash % _buckets.size()];
    entry_t* entry = std::ranges::find(bucket.entries, key, &entry_t::key);
    if (entry == std::end(bucket.entries)) {
        entry = std::ranges::min_element(bucket.entries, {}, &entry_t::depth);
    }
    _used += entry->flag == UNKNOWN;
    *entry = {key, move, score_to_tt(score, height), flag, depth};
}

std::optional<transposition_t::entry_t> transposition_t::get(const hash_t hash, const ply_t height) const noexcept {
    const entry_t::key_t key = make_key(hash);
    const bucket_t& bucket = _buckets[hash % _buckets.size()];
    const entry_t* entry = std::ranges::find(bucket.entries, key, &entry_t::key);
    if (entry == std::end(bucket.entries) || entry->flag == UNKNOWN)
        return std::nullopt;
    entry_t result = *entry;
    result.score = score_from_tt(result.score, height);
    return result;
}

size_t transposition_t::full() const noexcept {
    return size_t(1000.f * _used / (_buckets.size() * BUCKET_SIZE));
}

std::optional<score_t> transposition_t::get_eval(const hash_t hash) const noexcept {
    const auto& entry = _evaluations[hash & (EVALUATION_COUNT - 1)];
    if (entry.key != hash || entry.value.ignore())
        return std::nullopt;
    return entry.value;
}

void transposition_t::put_eval(const hash_t hash, const score_t evaluation) noexcept {
    _evaluations[hash & (EVALUATION_COUNT - 1)] = {hash, evaluation};
}

} // namespace chess
