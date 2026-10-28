module;

#include <cstddef>
#include <cstdint>
#include <array>
#include <memory>
#include <span>
#include <string_view>

export module nnue.nnue;

import core.position;
import types.square;
import types.side;
import types.piece;

export namespace chess::nnue {

constexpr size_t THREAT_INPUTS = 59808;
constexpr size_t PAIR_INPUTS = 4560;
constexpr size_t PSQ_BLOCK_DIMENSIONS = 22528;
constexpr size_t PSQ_INPUTS = 3 * PSQ_BLOCK_DIMENSIONS;
constexpr size_t PSQ_OFFSET = THREAT_INPUTS + PAIR_INPUTS;
constexpr size_t L0 = PSQ_OFFSET + PSQ_INPUTS;
constexpr size_t L1 = 1024;
constexpr size_t L2 = 32;
constexpr size_t L3 = 32;
constexpr size_t SLOTS = 32;
// At most 32 PSQs, 32 * 31 directed threats, and 16 * 15 / 2 pawn pairs.
constexpr size_t MAX_ACTIVE = 32 + 32 * 31 + 16 * 15 / 2;
using feature_buffer_t = std::array<uint32_t, MAX_ACTIVE>;
constexpr std::string_view NETWORK_NAME = "nn-d1ee674a62ca.nnue";

uint32_t make_index(square_t king, square_t square, piece_t piece, side_t perspective,
                    size_t psq_block) noexcept;
uint32_t threat_index(square_t king, square_t from, square_t to, piece_t attacker,
                      piece_t target, side_t perspective) noexcept;
uint32_t pair_index(square_t king, square_t from, square_t to, side_t color,
                    side_t paired_color, side_t perspective) noexcept;
std::array<std::span<uint32_t>, 2> active_features(
    const position_t& position, std::span<feature_buffer_t, 2> buffers) noexcept;

struct entry_t final {
    alignas(64) int16_t accumulation[L1]{};
    feature_buffer_t features;
    size_t feature_count{};
};

struct nnue_t final {
    void initialize(entry_t& entry) const noexcept;
    void update(entry_t& entry, std::span<const uint32_t> removed,
                std::span<const uint32_t> added) const noexcept;
    int16_t evaluate(const entry_t& us, const entry_t& them, size_t piece_count,
                     bool our_queen, bool their_queen) const noexcept;

private:
    explicit nnue_t(std::string_view filename);

    struct feature_layer_t final {
        alignas(64) int16_t biases[L1];
        alignas(64) int8_t weights[L0][L1];
    };

    struct hidden_layer_t final {
        alignas(64) int32_t biases1[L2];
        alignas(64) int8_t weights1[L1 / 4][L2][4];
        alignas(64) int32_t biases2[L3];
        alignas(64) int8_t weights2[2 * L2 / 4][L3][4];
        alignas(64) int32_t bias3;
        alignas(64) int8_t weights3[2 * L2 + 2 * L3];
    };

    feature_layer_t feature_layer;
    hidden_layer_t hidden_layers[SLOTS];

    friend std::unique_ptr<nnue_t> load(std::string_view);
};

std::unique_ptr<nnue_t> load(std::string_view filename = "../nn-d1ee674a62ca.nnue");

}
