module;

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <immintrin.h>
#include <limits>
#include <span>
#include <type_traits>

module engine.ordering;

namespace chess {

void sort_scored_moves(const std::span<scored_move_t> moves) noexcept {
    if (moves.empty())
        return;

    static_assert(sizeof(move_t) == sizeof(uint16_t));
    static_assert(std::is_standard_layout_v<scored_move_t>);
    static_assert(std::is_trivially_copyable_v<scored_move_t>);
    static_assert(sizeof(scored_move_t) == 8);
    static_assert(offsetof(scored_move_t, move) == 0);
    static_assert(offsetof(scored_move_t, score) == 4);

    constexpr size_t max_elements = 16;
    const size_t count = std::min(moves.size(), max_elements);
    __m512i sorted_moves = _mm512_set1_epi32(std::bit_cast<uint16_t>(moves.front().move));
    __m512i sorted_scores = _mm512_mask_set1_epi32(
        _mm512_set1_epi32(moves.front().score), 0xfffe, std::numeric_limits<int>::min());

    for (size_t i = 1; i < count; ++i) {
        const __m512i move = _mm512_set1_epi32(std::bit_cast<uint16_t>(moves[i].move));
        const __m512i score = _mm512_set1_epi32(moves[i].score);
        // Inactive lanes must compare as smaller even when the score is INT_MIN.
        const __mmask16 inactive = static_cast<__mmask16>(0xffffu << i);
        const __mmask16 less = _mm512_cmplt_epi32_mask(sorted_scores, score) | inactive;
        const __mmask16 expand = static_cast<__mmask16>(less - 1);
        sorted_scores = _mm512_mask_expand_epi32(score, expand, sorted_scores);
        sorted_moves = _mm512_mask_expand_epi32(move, expand, sorted_moves);
    }

    // Interleave the 16-bit move (with zero padding) and its 32-bit score.
    const auto write = [&](const size_t offset, const __m512i indices) {
        if (offset >= count)
            return;
        const size_t store_count = std::min(count - offset, size_t{8});
        const __m512i packed = _mm512_permutex2var_epi32(sorted_moves, indices, sorted_scores);
        _mm512_mask_storeu_epi64(moves.data() + offset,
                                static_cast<__mmask8>((1u << store_count) - 1), packed);
    };
    write(0, _mm512_setr_epi32(0, 16, 1, 17, 2, 18, 3, 19, 4, 20, 5, 21, 6, 22, 7, 23));
    write(8, _mm512_setr_epi32(8, 24, 9, 25, 10, 26, 11, 27, 12, 28, 13, 29, 14, 30, 15, 31));

    for (size_t i = count; i < moves.size(); ++i) {
        const scored_move_t move = moves[i];
        size_t j = i;
        while (j > 0 && moves[j - 1].score < move.score) {
            moves[j] = moves[j - 1];
            --j;
        }
        moves[j] = move;
    }
}

} // namespace chess
