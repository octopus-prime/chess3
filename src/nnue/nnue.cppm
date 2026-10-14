module; 

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <span>

export module nnue.nnue;

import types.square;
import types.side;
import types.piece;

export namespace chess::nnue {

constexpr size_t L0 = 22528;
constexpr size_t L1 = 3072;
constexpr size_t L2 = 16;
constexpr size_t L3 = 32;
constexpr size_t SLOTS = 8;

// uint16_t make_index(const uint16_t king_square, const uint16_t piece_square, const uint16_t piece_type, const int Perspective) noexcept;
uint16_t make_index(const square_t king_square, const square_t piece_square, const piece_t piece_type, const side_t side) noexcept;

struct entry_t final {
    alignas(64) int16_t accumulation[L1]{};
    int32_t psqrt_accumulation[8]{};
    uint64_t pieces[7]{};
    uint64_t colors[2]{};
};

struct nnue_t final {
    void initialize(entry_t& entry) const noexcept;
    void update(entry_t& entry, const std::span<const uint16_t> removed_features, const std::span<const uint16_t> added_features) const noexcept;

    int16_t evaluate(const entry_t& t, const entry_t& o, const size_t piece_count) const noexcept;

private:
    nnue_t(const std::string_view filename);

    struct feature_layer_t final {
        alignas(64) int16_t biases0[L1];
        alignas(64) int16_t weights0[L0][L1];
        alignas(64) int32_t psqrt_weights0[L0][8];
    };

    struct hidden_layer_t final {
        alignas(64) int32_t biases1[L2];
        alignas(64) int8_t weights1[L1][L2];

        alignas(64) int32_t biases2[L3];
        alignas(64) int8_t weights2[2 * L2][L3];

        alignas(64) int32_t biases3[1];
        alignas(64) int8_t weights3[1][L3];
    };

    feature_layer_t feature_layer;
    hidden_layer_t hidden_layers[SLOTS];

    friend std::unique_ptr<nnue_t> load();
};

std::unique_ptr<nnue_t> load();

}
