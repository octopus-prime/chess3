module;

#include <fstream>
#include <stdexcept>
#include <span>
#include <ranges>
#include <algorithm>
#include <memory>

#include <immintrin.h>

module nnue.nnue;

import types.square;
import types.side;
import types.piece;

namespace chess::nnue {

enum {
    PS_NONE     = 0,
    PS_W_PAWN   = 0,
    PS_B_PAWN   = 1 * MAX_SQUARE,
    PS_W_KNIGHT = 2 * MAX_SQUARE,
    PS_B_KNIGHT = 3 * MAX_SQUARE,
    PS_W_BISHOP = 4 * MAX_SQUARE,
    PS_B_BISHOP = 5 * MAX_SQUARE,
    PS_W_ROOK   = 6 * MAX_SQUARE,
    PS_B_ROOK   = 7 * MAX_SQUARE,
    PS_W_QUEEN  = 8 * MAX_SQUARE,
    PS_B_QUEEN  = 9 * MAX_SQUARE,
    PS_KING     = 10 * MAX_SQUARE,
    PS_NB       = 11 * MAX_SQUARE
};

constexpr uint16_t PieceSquareIndex[MAX_SIDE][MAX_PIECE] = {
    {PS_NONE, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, PS_KING, PS_NONE, PS_NONE, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, PS_KING, PS_NONE},
    {PS_NONE, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_B_ROOK, PS_B_QUEEN, PS_KING, PS_NONE, PS_NONE, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_W_ROOK, PS_W_QUEEN, PS_KING, PS_NONE}
};

constexpr uint16_t OrientTBL[MAX_SIDE][MAX_SQUARE] = {
    { h1, h1, h1, h1, a1, a1, a1, a1,
    h1, h1, h1, h1, a1, a1, a1, a1,
    h1, h1, h1, h1, a1, a1, a1, a1,
    h1, h1, h1, h1, a1, a1, a1, a1,
    h1, h1, h1, h1, a1, a1, a1, a1,
    h1, h1, h1, h1, a1, a1, a1, a1,
    h1, h1, h1, h1, a1, a1, a1, a1,
    h1, h1, h1, h1, a1, a1, a1, a1 },
    { h8, h8, h8, h8, a8, a8, a8, a8,
    h8, h8, h8, h8, a8, a8, a8, a8,
    h8, h8, h8, h8, a8, a8, a8, a8,
    h8, h8, h8, h8, a8, a8, a8, a8,
    h8, h8, h8, h8, a8, a8, a8, a8,
    h8, h8, h8, h8, a8, a8, a8, a8,
    h8, h8, h8, h8, a8, a8, a8, a8,
    h8, h8, h8, h8, a8, a8, a8, a8 }
};

#define B(v) (v * PS_NB)
constexpr uint16_t KingBuckets[MAX_SIDE][MAX_SQUARE] = {
    { B(28), B(29), B(30), B(31), B(31), B(30), B(29), B(28),
    B(24), B(25), B(26), B(27), B(27), B(26), B(25), B(24),
    B(20), B(21), B(22), B(23), B(23), B(22), B(21), B(20),
    B(16), B(17), B(18), B(19), B(19), B(18), B(17), B(16),
    B(12), B(13), B(14), B(15), B(15), B(14), B(13), B(12),
    B( 8), B( 9), B(10), B(11), B(11), B(10), B( 9), B( 8),
    B( 4), B( 5), B( 6), B( 7), B( 7), B( 6), B( 5), B( 4),
    B( 0), B( 1), B( 2), B( 3), B( 3), B( 2), B( 1), B( 0) },
    { B( 0), B( 1), B( 2), B( 3), B( 3), B( 2), B( 1), B( 0),
    B( 4), B( 5), B( 6), B( 7), B( 7), B( 6), B( 5), B( 4),
    B( 8), B( 9), B(10), B(11), B(11), B(10), B( 9), B( 8),
    B(12), B(13), B(14), B(15), B(15), B(14), B(13), B(12),
    B(16), B(17), B(18), B(19), B(19), B(18), B(17), B(16),
    B(20), B(21), B(22), B(23), B(23), B(22), B(21), B(20),
    B(24), B(25), B(26), B(27), B(27), B(26), B(25), B(24),
    B(28), B(29), B(30), B(31), B(31), B(30), B(29), B(28) }
};
#undef B

// uint16_t make_index(const uint16_t king_square, const uint16_t piece_square, const uint16_t piece_type, const int Perspective) noexcept {
//     return (piece_square ^ OrientTBL[Perspective][king_square]) + PieceSquareIndex[Perspective][piece_type] + KingBuckets[Perspective][king_square];
// }

uint16_t make_index(const square_t king_square, const square_t piece_square, const piece_t piece_type, const side_t side) noexcept {
    return (uint16_t(piece_square) ^ OrientTBL[side][king_square]) + PieceSquareIndex[side][piece_type] + KingBuckets[side][king_square];
}

template <typename T, typename U, std::size_t I>
    requires(sizeof(T) % sizeof(U) == 0)
auto span_cast(const std::span<U, I> span) noexcept {
    constexpr auto O = I / (sizeof(T) / sizeof(U));
    return std::span<T, O>{reinterpret_cast<T*>(span.data()), O};
}

void mul_clipped_relu(const std::span<const int16_t, L1> input, const std::span<uint8_t, L1 / 2> output) noexcept {
    const auto in0 = span_cast<const __m512i>(input.template first<L1 / 2>());
    const auto in1 = span_cast<const __m512i>(input.template last<L1 / 2>());
    const auto out = span_cast<__m256i>(output);
    const __m512i min = _mm512_setzero_si512();
    const __m512i max = _mm512_set1_epi16(127 * 2);
    for (size_t i = 0; i < out.size(); ++i) {
        const __m512i sum0 = _mm512_slli_epi16(_mm512_max_epi16(_mm512_min_epi16(in0[i], max), min), 7);
        const __m512i sum1 = _mm512_min_epi16(in1[i], max);
        const __m512i prod = _mm512_mulhi_epi16(sum0, sum1);
        out[i] = _mm256_packus_epi16(_mm512_castsi512_si256(prod), _mm512_extracti32x8_epi32(prod, 1));
    }
}

void sqr_clipped_relu(const std::span<const int32_t, L2> input, const std::span<uint8_t, L2> output) noexcept {
    std::ranges::transform(span_cast<const __m128i>(input) | std::views::chunk(4), span_cast<__m128i>(output).begin(), [](auto&& chunk) {
        __m128i words0 = _mm_packs_epi32(chunk[0], chunk[1]);
        __m128i words1 = _mm_packs_epi32(chunk[2], chunk[3]);
        words0 = _mm_srli_epi16(_mm_mulhi_epi16(words0, words0), 3);
        words1 = _mm_srli_epi16(_mm_mulhi_epi16(words1, words1), 3);
        return _mm_packs_epi16(words0, words1);
    });
}

void clipped_relu(const std::span<const int32_t, L2> input, const std::span<uint8_t, L2> output) noexcept {
    std::ranges::transform(span_cast<const __m128i>(input) | std::views::chunk(4), span_cast<__m128i>(output).begin(), [](auto&& chunk) {
        const __m128i words0 = _mm_srli_epi16(_mm_packus_epi32(chunk[0], chunk[1]), 6);
        const __m128i words1 = _mm_srli_epi16(_mm_packus_epi32(chunk[2], chunk[3]), 6);
        return _mm_packs_epi16(words0, words1);
    });
}

void clipped_relu(const std::span<const int32_t, L3> input, const std::span<uint8_t, L3> output) noexcept {
    std::ranges::transform(span_cast<const __m256i>(input) | std::views::chunk(4), span_cast<__m256i>(output).begin(), [](auto&& chunk) {
        const __m256i words0 = _mm256_srli_epi16(_mm256_packus_epi32(chunk[0], chunk[1]), 6);
        const __m256i words1 = _mm256_srli_epi16(_mm256_packus_epi32(chunk[2], chunk[3]), 6);
        return _mm256_permutevar8x32_epi32(_mm256_packs_epi16(words0, words1), _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0));
    });
}

void affine_tranform(const std::span<const uint8_t, L1> input, const std::span<const int8_t[L2], L1> weights, const std::span<const int32_t, L2> biases, const std::span<int32_t, L2> output) noexcept {
    // column major
    const auto input32 = span_cast<const int32_t>(input); // 768
    alignas(64) uint16_t buf[input32.size()];

    const __m512i       increment    = _mm512_set1_epi16(32);
    __m512i base = _mm512_set_epi16(31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
                                    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
    auto count = 0;
    for (auto&& chunk : span_cast<const __m512i>(input32) | std::views::chunk(2)) {
        const __mmask16 mask0 = _mm512_test_epi32_mask(chunk[0], chunk[0]);
        const __mmask16 mask1 = _mm512_test_epi32_mask(chunk[1], chunk[1]);
        const __mmask32 nnzMask = _mm512_kunpackw(mask1, mask0);
        _mm512_mask_compressstoreu_epi16(buf + count, nnzMask, base);
        count += std::popcount(nnzMask);
        base = _mm512_add_epi16(base, increment);
    }

    __m512i acc = _mm512_load_si512(biases.data());
    for (auto i : std::span{buf}.first(count)) {
        const __m512i in = _mm512_set1_epi32(input32[i]);
        acc = _mm512_dpbusd_epi32(acc, in, _mm512_load_si512(weights[i * 4]));
    }

    _mm512_store_si512(output.data(), acc);
}

void affine_tranform(const std::span<const uint8_t, 2 * L2> input, const std::span<const int8_t[L3], 2 * L2> weights, const std::span<const int32_t, L3> biases, const std::span<int32_t, L3> output) noexcept {
    // column major
    const auto input32 = span_cast<const int32_t>(input); // 8
    __m512i acc0 = _mm512_load_si512(biases.first<16>().data());
    __m512i acc1 = _mm512_load_si512(biases.last<16>().data());
    for (auto i = 0ul; i < input32.size(); ++i) {
        const __m512i in = _mm512_set1_epi32(input32[i]);
        acc0 = _mm512_dpbusd_epi32(acc0, in, _mm512_load_si512(weights[i * 4 + 0]));
        acc1 = _mm512_dpbusd_epi32(acc1, in, _mm512_load_si512(weights[i * 4 + 2]));
    }
    _mm512_store_si512(output.first<16>().data(), acc0);
    _mm512_store_si512(output.last<16>().data(), acc1);
}

void affine_tranform(const std::span<const uint8_t, L3> input, const std::span<const int8_t[L3], 1> weights, const std::span<const int32_t, 1> biases, const std::span<int32_t, 1> output) noexcept {
    // row major
    const __m256i in = _mm256_load_si256(reinterpret_cast<const __m256i*>(input.data()));
    const __m256i row = _mm256_load_si256(reinterpret_cast<const __m256i*>(weights.data()));
    const __m256i sum = _mm256_madd_epi16(_mm256_maddubs_epi16(in, row), _mm256_set1_epi16(1));
    output[0] = biases[0] + __builtin_reduce_add((__v8si) sum);
}

std::unique_ptr<nnue_t> load() {
    return std::unique_ptr<nnue_t>(new nnue_t("../nnue.bin"));
}

nnue_t::nnue_t(const std::string_view filename) {
    std::ifstream stream{std::string{filename}, std::ios::binary};
    if (!stream)
        throw std::runtime_error{"unable to open NNUE network: " + std::string{filename}};

    stream.read(reinterpret_cast<char*>(&feature_layer.biases0), sizeof(feature_layer.biases0));
    stream.read(reinterpret_cast<char*>(&feature_layer.weights0), sizeof(feature_layer.weights0));
    stream.read(reinterpret_cast<char*>(&feature_layer.psqrt_weights0), sizeof(feature_layer.psqrt_weights0));

    for (size_t i = 0; i < SLOTS; ++i) {
        stream.read(reinterpret_cast<char*>(&hidden_layers[i].biases1), sizeof(hidden_layers[i].biases1));
        stream.read(reinterpret_cast<char*>(&hidden_layers[i].weights1), sizeof(hidden_layers[i].weights1));

        stream.read(reinterpret_cast<char*>(&hidden_layers[i].biases2), sizeof(hidden_layers[i].biases2));
        stream.read(reinterpret_cast<char*>(&hidden_layers[i].weights2), sizeof(hidden_layers[i].weights2));

        stream.read(reinterpret_cast<char*>(&hidden_layers[i].biases3), sizeof(hidden_layers[i].biases3));
        stream.read(reinterpret_cast<char*>(&hidden_layers[i].weights3), sizeof(hidden_layers[i].weights3));
    }

    if (!stream || stream.fail() || stream.peek() != std::ios::traits_type::eof())
        throw std::runtime_error{"failed to read NNUE network: " + std::string{filename}};
}

void nnue_t::initialize(entry_t& entry) const noexcept {
    const auto accumulation = span_cast<__m512i>(std::span{entry.accumulation});
    const auto biases = span_cast<const __m512i>(std::span{feature_layer.biases0});
    std::ranges::copy(biases, accumulation.begin());
    _mm256_store_epi32(entry.psqrt_accumulation, _mm256_setzero_si256());
}

void nnue_t::update(entry_t& entry, const std::span<const uint16_t> removed_features, const std::span<const uint16_t> added_features) const noexcept {
    constexpr size_t chunk = 16;
    const auto accumulation = span_cast<__m512i>(std::span{entry.accumulation});
    const auto weights = [this](const auto feature){ return span_cast<const __m512i>(std::span{feature_layer.weights0[feature]}); };
    for (auto index = 0ul; index < accumulation.size(); index += chunk) {
        __m512i regs[chunk];
        std::ranges::copy(accumulation.subspan(index, chunk), regs);
        for (auto feature : removed_features)
            std::ranges::transform(regs, weights(feature).subspan(index, chunk), regs, _mm512_sub_epi16);
        for (auto feature : added_features)
            std::ranges::transform(regs, weights(feature).subspan(index, chunk), regs, _mm512_add_epi16);
        std::ranges::copy(regs, accumulation.subspan(index, chunk).begin());
    }

    const auto psqrt_weights = [this](const auto feature) { return _mm256_load_epi32(feature_layer.psqrt_weights0[feature]); };
    __m256i psqrt_accumulation = _mm256_load_epi32(entry.psqrt_accumulation);
    psqrt_accumulation = std::ranges::fold_left(removed_features | std::views::transform(psqrt_weights), psqrt_accumulation, _mm256_sub_epi32);
    psqrt_accumulation = std::ranges::fold_left(added_features | std::views::transform(psqrt_weights), psqrt_accumulation, _mm256_add_epi32);
    _mm256_store_epi32(entry.psqrt_accumulation, psqrt_accumulation);
}

int16_t nnue_t::evaluate(const entry_t& t, const entry_t& o, const size_t piece_count) const noexcept {
    const auto bucket = (piece_count - 1) / 4;

    alignas(64) uint8_t l1clipped[L1];
    alignas(64) int32_t l2transformed[L2];
    alignas(64) uint8_t l2clipped[2 * L2];
    alignas(64) int32_t l3transformed[L3];
    alignas(64) uint8_t l3clipped[L3];
    alignas(64) int32_t l4transformed[1];

    mul_clipped_relu(std::span{t.accumulation}, std::span{l1clipped}.template first<L1 / 2>());
    mul_clipped_relu(std::span{o.accumulation}, std::span{l1clipped}.template last<L1 / 2>());

    const auto& hidden_layer = hidden_layers[bucket];

    affine_tranform(std::span{l1clipped}, std::span{hidden_layer.weights1}, std::span{hidden_layer.biases1}, std::span{l2transformed});
    sqr_clipped_relu(std::span{l2transformed} | std::views::as_const, std::span{l2clipped}.template first<L2>());
    clipped_relu(std::span{l2transformed} | std::views::as_const, std::span{l2clipped}.template last<L2>());

    // ugly shit
    std::ranges::copy(std::span{l2clipped}.template subspan<L2, L2 - 1>(), l2clipped + L2 - 1);
    l2clipped[2 * L2 - 2] = 0;
    l2clipped[2 * L2 - 1] = 0;
    //

    affine_tranform(std::span{l2clipped} | std::views::as_const, std::span{hidden_layer.weights2}, std::span{hidden_layer.biases2}, std::span{l3transformed});
    clipped_relu(std::span{l3transformed} | std::views::as_const, std::span{l3clipped});
    affine_tranform(std::span{l3clipped} | std::views::as_const, std::span{hidden_layer.weights3}, std::span{hidden_layer.biases3}, std::span{l4transformed});

    const auto positional = l4transformed[0] + l2transformed[L2 - 1] * (600 * 16) / (127 * (1 << 6));
    const auto psqt = (t.psqrt_accumulation[bucket] - o.psqrt_accumulation[bucket]) / 2;

    return (positional + psqt) / 64;
}

}
