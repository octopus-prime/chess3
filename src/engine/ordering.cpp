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
    if (moves.size() <= 1)
        return;

    static_assert(sizeof(move_t) == sizeof(uint16_t));
    static_assert(std::is_standard_layout_v<scored_move_t>);
    static_assert(std::is_trivially_copyable_v<scored_move_t>);
    static_assert(sizeof(scored_move_t) == sizeof(int32_t));
    static_assert(offsetof(scored_move_t, move) == 0);
    static_assert(offsetof(scored_move_t, score) == 2);
    static_assert(std::endian::native == std::endian::little);

    constexpr size_t max_elements = 16;
    const size_t count = std::min(moves.size(), max_elements);
    __m512i sorted = _mm512_set1_epi32(std::bit_cast<int32_t>(moves.front()));
    for (size_t i = 1; i < count; ++i) {
        const __m512i move = _mm512_set1_epi32(std::bit_cast<int32_t>(moves[i]));
        // Compare only the signed score; move bits must not break ties.
        const __m512i scores = _mm512_srai_epi32(sorted, 16);
        const __mmask16 inactive = static_cast<__mmask16>(0xffffu << i);
        const __mmask16 less = _mm512_cmplt_epi32_mask(
            scores, _mm512_set1_epi32(moves[i].score)) | inactive;
        const __mmask16 expand = static_cast<__mmask16>(less - 1);
        sorted = _mm512_mask_expand_epi32(move, expand, sorted);
    }
    _mm512_mask_storeu_epi32(moves.data(),
                            static_cast<__mmask16>((1u << count) - 1), sorted);

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
