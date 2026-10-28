module;

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>

#include <immintrin.h>

module nnue.nnue;

import core.attacks.lookup;
import types.squares;

namespace chess::nnue {

namespace {

constexpr int target_map[5][6] = {
    {-1, 0, -1, 1, -1, -1},
    {0, 1, 2, 3, 4, -1},
    {0, 1, 2, 3, -1, -1},
    {0, 1, 2, 3, -1, -1},
    {0, 1, 2, 3, 4, -1}
};
constexpr uint32_t target_counts[] = {0, 4, 10, 8, 8, 10};
constexpr uint32_t attack_counts[] = {0, 84, 336, 560, 896, 1456};
constexpr uint32_t attack_offsets[] = {0, 0, 336, 3696, 8176, 15344};

uint8_t orientation(const square_t king, const side_t perspective) noexcept {
    return (king.file() < e ? 7 : 0) ^ (56 * perspective);
}

squares_t attacks(const type_t type, const square_t from, const squares_t occupied,
                  const side_t color) noexcept {
    using namespace attacks::lookup;
    switch (type) {
        case P: return pawn(from, color);
        case N: return knight(from);
        case B: return bishop(from, occupied);
        case R: return rook(from, occupied);
        case Q: return queen(from, occupied);
        default: return {};
    }
}

struct threat_table_t {
    uint16_t index[6][64][64]{};

    threat_table_t() noexcept {
        for (const type_t type : {P, N, B, R, Q}) {
            uint32_t offset = 0;
            for (square_t from = a1; from <= h8; ++from) {
                if (type == P && (from.rank() == _1 || from.rank() == _8))
                    continue;
                for (square_t to = a1; to <= h8; ++to) {
                    const auto destinations = attacks(type, from, {}, to < from ? BLACK : WHITE);
                    const auto preceding = (uint64_t{1} << to) - 1;
                    index[type][from][to] =
                        static_cast<uint16_t>(offset + std::popcount(uint64_t(destinations) & preceding));
                }
                offset += attacks(type, from, {}, WHITE).size();
            }
        }
    }
};

const threat_table_t threat_table;

constexpr squares_t pawn_pair_files[] = {
    {a, b}, {a, b, c}, {b, c, d}, {c, d, e},
    {d, e, f}, {e, f, g}, {f, g, h}, {g, h}
};

void read_bytes(std::istream& stream, void* destination, const size_t size) {
    if (!stream.read(static_cast<char*>(destination), static_cast<std::streamsize>(size)))
        throw std::runtime_error{"truncated NNUE network"};
}

template <typename T>
T read_integer(std::istream& stream) {
    static_assert(std::endian::native == std::endian::little);
    T value;
    read_bytes(stream, &value, sizeof(value));
    return value;
}

template <size_t Inputs>
void read_affine(std::istream& stream, int32_t (&biases)[32],
                 int8_t (&weights)[Inputs / 4][32][4]) {
    read_bytes(stream, biases, sizeof(biases));
    std::array<int8_t, Inputs> row;
    for (size_t out = 0; out < 32; ++out) {
        read_bytes(stream, row.data(), row.size());
        for (size_t in = 0; in < Inputs; ++in)
            weights[in / 4][out][in % 4] = row[in];
    }
}

template <size_t Inputs>
void affine(const uint8_t (&input)[Inputs], const int8_t (&weights)[Inputs / 4][32][4],
            const int32_t (&biases)[32], int32_t (&output)[32]) noexcept {
    auto low = _mm512_load_si512(biases);
    auto high = _mm512_load_si512(biases + 16);
    const auto accumulate = [&](const size_t i, const uint32_t word) {
        const auto value = _mm512_set1_epi32(std::bit_cast<int32_t>(word));
        low = _mm512_dpbusd_epi32(low, value, _mm512_load_si512(weights[i][0]));
        high = _mm512_dpbusd_epi32(high, value, _mm512_load_si512(weights[i][16]));
    };
    if constexpr (Inputs == L1) {
        // Each mask bit selects one nonzero four-byte VNNI input group.
        for (size_t base = 0; base < Inputs / 4; base += 16) {
            uint32_t nonzero = _mm512_cmpneq_epi32_mask(
                _mm512_load_si512(input + 4 * base), _mm512_setzero_si512());
            while (nonzero) {
                const size_t i = base + std::countr_zero(nonzero);
                nonzero &= nonzero - 1;
                uint32_t word;
                std::memcpy(&word, input + 4 * i, sizeof(word));
                accumulate(i, word);
            }
        }
    } else {
        for (size_t i = 0; i < Inputs / 4; ++i) {
            uint32_t word;
            std::memcpy(&word, input + 4 * i, sizeof(word));
            if (word != 0)
                accumulate(i, word);
        }
    }
    _mm512_store_si512(output, low);
    _mm512_store_si512(output + 16, high);
}

void activate(const int32_t (&input)[32], uint8_t* output, const int shift) noexcept {
    for (size_t i = 0; i < 32; ++i) {
        const int64_t value = input[i];
        output[i] = static_cast<uint8_t>(std::min((value * value) >> (2 * shift + 7), int64_t{127}));
        output[i + 32] = static_cast<uint8_t>(std::clamp(input[i] >> shift, 0, 127));
    }
}

}

uint32_t make_index(const square_t king, const square_t square, const piece_t piece,
                    const side_t perspective, const size_t psq_block) noexcept {
    const auto relative_king = uint8_t(king) ^ (56 * perspective);
    const uint32_t bucket = (7 - relative_king / 8) * 4 +
        std::min(relative_king % 8, 7 - relative_king % 8);
    const uint32_t piece_offset = piece.type() == K ? 640 :
        128 * (piece.type() - P) + 64 * (piece.side() != perspective);
    return PSQ_OFFSET + psq_block * PSQ_BLOCK_DIMENSIONS +
        (uint32_t(square) ^ (bucket * 704 + piece_offset + orientation(king, perspective)));
}

uint32_t threat_index(const square_t king, const square_t from, const square_t to,
                      const piece_t attacker, const piece_t target, const side_t perspective) noexcept {
    if (attacker.type() == K || target.type() == NO_TYPE)
        return THREAT_INPUTS;
    const int mapped = target_map[attacker.type() - 1][target.type() - 1];
    const auto flip = orientation(king, perspective);
    const auto oriented_from = uint8_t(from) ^ flip;
    const auto oriented_to = uint8_t(to) ^ flip;
    if (mapped < 0 || (oriented_from < oriented_to && attacker.type() == target.type() &&
                      (attacker.side() != target.side() || attacker.type() != P)))
        return THREAT_INPUTS;
    const uint32_t block = attack_offsets[attacker.type()] +
        29904 * (attacker.side() != perspective);
    const uint32_t target_block = target_counts[attacker.type()] / 2 *
        (target.side() != perspective) + mapped;
    return block + target_block * attack_counts[attacker.type()] +
        threat_table.index[attacker.type()][oriented_from][oriented_to];
}

uint32_t pair_index(const square_t king, const square_t from, const square_t to,
                    const side_t color, const side_t paired_color, const side_t perspective) noexcept {
    const auto flip = orientation(king, perspective);
    const uint32_t first = 48 * (color != perspective) + (uint8_t(from) ^ flip) - 8;
    const uint32_t second = 48 * (paired_color != perspective) + (uint8_t(to) ^ flip) - 8;
    const auto high = std::max(first, second);
    const auto low = std::min(first, second);
    return THREAT_INPUTS + high * (high - 1) / 2 + low;
}

std::array<std::span<uint32_t>, 2> active_features(
    const position_t& position, const std::span<feature_buffer_t, 2> buffers) noexcept {
    std::array<size_t, 2> counts{};
    const square_t kings[] = {position.by(WHITE, K).front(), position.by(BLACK, K).front()};
    const bool queens[] = {!position.by(WHITE, Q).empty(), !position.by(BLACK, Q).empty()};
    const bool any_heavy_pieces = !position.by(R).empty() || !position.by(Q).empty();
    const size_t psq_blocks[] = {
        queens[BLACK] ? size_t{2} : any_heavy_pieces ? size_t{1} : size_t{0},
        queens[WHITE] ? size_t{2} : any_heavy_pieces ? size_t{1} : size_t{0}
    };
    const auto occupied = position.by();
    assert(occupied.size() <= 32 && position.by(P).size() <= 16);
    for (const square_t from : occupied) {
        const auto piece = position.at(from);
        for (const side_t side : {WHITE, BLACK})
            buffers[side][counts[side]++] =
                make_index(kings[side], from, piece, side, psq_blocks[side]);
        if (piece.type() == K)
            continue;
        for (const square_t to : attacks(piece.type(), from, occupied, piece.side()) & occupied) {
            const auto target = position.at(to);
            for (const side_t side : {WHITE, BLACK}) {
                const auto index = threat_index(kings[side], from, to, piece, target, side);
                if (index < THREAT_INPUTS)
                    buffers[side][counts[side]++] = index;
            }
        }
    }

    auto pawns = position.by(P);
    for (const square_t from : position.by(P)) {
        pawns.reset(from);
        for (const square_t to : pawns & pawn_pair_files[from.file()])
            for (const side_t side : {WHITE, BLACK})
                buffers[side][counts[side]++] = pair_index(
                    kings[side], from, to, position.at(from).side(), position.at(to).side(), side);
    }
    return {std::span{buffers[WHITE]}.first(counts[WHITE]),
            std::span{buffers[BLACK]}.first(counts[BLACK])};
}

std::unique_ptr<nnue_t> load(const std::string_view filename) {
    return std::unique_ptr<nnue_t>(new nnue_t(filename));
}

nnue_t::nnue_t(const std::string_view filename) {
    std::ifstream stream{std::string{filename}, std::ios::binary};
    if (!stream)
        throw std::runtime_error{"unable to open NNUE network: " + std::string{filename}};
    if (read_integer<uint32_t>(stream) != 0x6A448AFA ||
        read_integer<uint32_t>(stream) != 0xC0357A83)
        throw std::runtime_error{"unsupported NNUE network format: " + std::string{filename}};
    const auto description_size = read_integer<uint32_t>(stream);
    if (description_size > 4096)
        throw std::runtime_error{"invalid NNUE network description length"};
    std::array<char, 4096> description;
    read_bytes(stream, description.data(), description_size);
    if (read_integer<uint32_t>(stream) != 0xA3060B95)
        throw std::runtime_error{"unsupported NNUE feature transformer"};
    read_bytes(stream, feature_layer.biases, sizeof(feature_layer.biases));
    read_bytes(stream, feature_layer.weights, sizeof(feature_layer.weights));
    for (auto& layer : hidden_layers) {
        if (read_integer<uint32_t>(stream) != 0x63337116)
            throw std::runtime_error{"unsupported NNUE layer stack"};
        read_affine<L1>(stream, layer.biases1, layer.weights1);
        read_affine<2 * L2>(stream, layer.biases2, layer.weights2);
        layer.bias3 = read_integer<int32_t>(stream);
        read_bytes(stream, layer.weights3, sizeof(layer.weights3));
    }
    if (stream.peek() != std::ios::traits_type::eof())
        throw std::runtime_error{"unexpected trailing data in NNUE network: " + std::string{filename}};
}

void nnue_t::initialize(entry_t& entry) const noexcept {
    std::ranges::copy(feature_layer.biases, entry.accumulation);
    entry.feature_count = 0;
}

void nnue_t::update(entry_t& entry, const std::span<const uint32_t> removed,
                    const std::span<const uint32_t> added) const noexcept {
    if (removed.empty() && added.empty())
        return;
    constexpr size_t registers = 16;
    for (size_t base = 0; base < L1; base += registers * 32) {
        __m512i accumulation[registers];
        for (size_t i = 0; i < registers; ++i)
            accumulation[i] = _mm512_load_si512(entry.accumulation + base + i * 32);
        const auto paired = std::min(removed.size(), added.size());
        for (size_t index = 0; index < paired; ++index)
            for (size_t i = 0; i < registers; ++i) {
                const auto remove = _mm512_cvtepi8_epi16(_mm256_load_si256(
                    reinterpret_cast<const __m256i*>(feature_layer.weights[removed[index]] + base + i * 32)));
                const auto add = _mm512_cvtepi8_epi16(_mm256_load_si256(
                    reinterpret_cast<const __m256i*>(feature_layer.weights[added[index]] + base + i * 32)));
                accumulation[i] = _mm512_add_epi16(accumulation[i], _mm512_sub_epi16(add, remove));
            }
        for (const auto feature : removed.subspan(paired))
            for (size_t i = 0; i < registers; ++i)
                accumulation[i] = _mm512_sub_epi16(accumulation[i],
                    _mm512_cvtepi8_epi16(_mm256_load_si256(
                        reinterpret_cast<const __m256i*>(feature_layer.weights[feature] + base + i * 32))));
        for (const auto feature : added.subspan(paired))
            for (size_t i = 0; i < registers; ++i)
                accumulation[i] = _mm512_add_epi16(accumulation[i],
                    _mm512_cvtepi8_epi16(_mm256_load_si256(
                        reinterpret_cast<const __m256i*>(feature_layer.weights[feature] + base + i * 32))));
        for (size_t i = 0; i < registers; ++i)
            _mm512_store_si512(entry.accumulation + base + i * 32, accumulation[i]);
    }
}

int16_t nnue_t::evaluate(const entry_t& us, const entry_t& them, const size_t piece_count,
                        const bool our_queen, const bool their_queen) const noexcept {
    const size_t bucket = (piece_count - 1) / 4 * 4 + 2 * our_queen + their_queen;
    alignas(64) uint8_t transformed[L1];
    const entry_t* perspectives[] = {&us, &them};
    for (size_t p = 0; p < 2; ++p)
        for (size_t i = 0; i < L1 / 2; ++i) {
            const int first = std::clamp(int(perspectives[p]->accumulation[i]), 0, 255);
            const int second = std::clamp(int(perspectives[p]->accumulation[i + L1 / 2]), 0, 255);
            transformed[p * (L1 / 2) + i] = static_cast<uint8_t>(first * second / 512);
        }

    const auto& layer = hidden_layers[bucket];
    alignas(64) int32_t first[32];
    alignas(64) int32_t second[32];
    alignas(64) uint8_t first_activations[64];
    alignas(64) uint8_t second_activations[128];
    affine(transformed, layer.weights1, layer.biases1, first);
    activate(first, first_activations, 7);
    affine(first_activations, layer.weights2, layer.biases2, second);
    std::ranges::copy(first_activations, second_activations);
    activate(second, second_activations + 64, 6);
    auto sum = _mm512_setzero_si512();
    for (size_t i = 0; i < 128; i += 64)
        sum = _mm512_dpbusd_epi32(sum, _mm512_load_si512(second_activations + i),
                                 _mm512_load_si512(layer.weights3 + i));
    const int64_t output = int64_t(layer.bias3) + _mm512_reduce_add_epi32(sum) +
        int64_t(first[30]) - first[31];
    return static_cast<int16_t>((output * 9600 / 16384) / 16);
}

}
