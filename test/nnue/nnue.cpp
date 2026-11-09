#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include "reference_positions.hpp"
#include "test_support.hpp"

import nnue.nnue;
import core.position;

using namespace chess;

namespace {

uint64_t feature_hash(const std::span<const uint32_t> features) {
    uint64_t hash = 14695981039346656037ULL;
    for (const auto feature : features)
        hash = (hash ^ feature) * 1099511628211ULL;
    return hash;
}

bool test_paired_updates(const nnue::nnue_t& network) {
    std::array<uint32_t, 33> removed;
    std::array<uint32_t, 33> added;
    for (size_t i = 0; i < removed.size(); ++i) {
        removed[i] = (i * 997) % nnue::L0;
        added[i] = (nnue::PSQ_OFFSET + i * 127) % nnue::L0;
    }
    for (const size_t remove_count : {0, 1, 2, 7, 16, 33})
        for (const size_t add_count : {0, 1, 2, 7, 16, 33}) {
            nnue::entry_t paired{};
            network.initialize(paired);
            for (size_t i = 0; i < nnue::L1; ++i)
                paired.accumulation[i] = i % 2 ? int16_t{32760} : int16_t{-32760};
            nnue::entry_t separate = paired;
            const auto remove = std::span{removed}.first(remove_count);
            const auto add = std::span{added}.first(add_count);
            network.update(paired, remove, add);
            network.update(separate, remove, {});
            network.update(separate, {}, add);
            if (!test_support::check(
                    std::ranges::equal(paired.accumulation, separate.accumulation),
                    "paired updates match separate updates for unequal counts and wrapping sums"))
                return false;
        }
    return true;
}

bool test_stack_keys() {
    struct case_t {
        std::string_view fen;
        size_t index;
    };
    const std::array cases{
        case_t{"4k3/8/8/8/8/8/8/4K3 w - - 0 1", 4},
        case_t{"4k3/8/8/8/8/8/P7/4K3 w - - 0 1", 2},
        case_t{"4k3/8/8/8/8/8/7P/4K3 w - - 0 1", 2},
        case_t{"4k3/8/8/8/8/8/P6P/4K3 w - - 0 1", 0},
        case_t{"4k3/8/8/8/8/8/PP6/4K3 w - - 0 1", 2},
        case_t{"4k3/8/8/8/8/8/8/1NN1K3 w - - 0 1", 5},
        case_t{"1n2k3/8/8/8/8/8/8/1N2K3 w - - 0 1", 4},
        case_t{"4k3/8/8/8/8/8/8/3QK3 w - - 0 1", 16},
        case_t{"4k3/8/8/8/8/8/8/3QK3 b - - 0 1", 10},
        case_t{"3qk3/8/8/8/8/8/8/3QK3 w - - 0 1", 22},
        case_t{"4k3/8/8/8/8/8/P7/1NN1K3 w - - 0 1", 27}
    };
    bool passed = true;
    for (const auto& test : cases)
        passed &= test_support::check(nnue::stack_index(position_t{test.fen}) == test.index,
                                      "raw stack keys use file halves, per-side officers, queens, and count bands");
    return passed;
}

bool test_network_errors() {
    const auto path = std::filesystem::temp_directory_path() /
        ("chess3-nnue-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto expect_error = [&](const std::string_view message) {
        try {
            const auto network = nnue::load(path.string());
        } catch (const std::runtime_error& error) {
            return test_support::check(std::string_view{error.what()}.contains(message),
                                        "NNUE loader reports the expected error");
        }
        return test_support::check(false, "NNUE loader rejects invalid network data");
    };
    bool passed = expect_error("unable to open");
    const auto write_table = [&](const std::array<uint16_t, nnue::SLOTS>& fallbacks,
                                  const uint32_t stored,
                                  const uint32_t table_hash = 0x5A7AB1E5,
                                  const uint32_t raw_count = nnue::SLOTS,
                                  const uint32_t transformer = 0xA3060B95) {
        std::ofstream stream{path, std::ios::binary};
        const std::array<uint32_t, 6> header{
            0x6A448AFA, 0x9A4FCB66, 0, table_hash, raw_count, stored};
        stream.write(reinterpret_cast<const char*>(header.data()), sizeof(header));
        stream.write(reinterpret_cast<const char*>(fallbacks.data()), sizeof(fallbacks));
        stream.write(reinterpret_cast<const char*>(&transformer), sizeof(transformer));
        if (!stream)
            throw std::runtime_error{"unable to write NNUE loader test fixture"};
    };
    {
        std::ofstream stream{path, std::ios::binary};
        const std::array<uint32_t, 3> header{0x6A448AFA, 0x9A4FCB66, 0};
        stream.write(reinterpret_cast<const char*>(header.data()), sizeof(header));
        if (!stream)
            throw std::runtime_error{"unable to write NNUE loader test fixture"};
    }
    passed &= expect_error("truncated");
    {
        std::ofstream stream{path, std::ios::binary};
        const uint32_t invalid_version = 0;
        stream.write(reinterpret_cast<const char*>(&invalid_version), sizeof(invalid_version));
        if (!stream)
            throw std::runtime_error{"unable to write NNUE loader test fixture"};
    }
    passed &= expect_error("unsupported NNUE network format");
    write_table(nnue_reference::fallbacks, 0);
    passed &= expect_error("invalid NNUE stack count");
    write_table(nnue_reference::fallbacks, nnue::SLOTS + 1);
    passed &= expect_error("invalid NNUE stack count");
    write_table(nnue_reference::fallbacks, nnue_reference::stored_stacks, 0);
    passed &= expect_error("unsupported NNUE stack table");
    write_table(nnue_reference::fallbacks, nnue_reference::stored_stacks, 0x5A7AB1E5, 32);
    passed &= expect_error("unsupported NNUE stack table");
    write_table(nnue_reference::fallbacks, nnue_reference::stored_stacks - 1);
    passed &= expect_error("inconsistent NNUE stack count");
    auto invalid = nnue_reference::fallbacks;
    invalid[1] = nnue::SLOTS;
    write_table(invalid, nnue_reference::stored_stacks);
    passed &= expect_error("invalid NNUE stack fallback");
    invalid = nnue_reference::fallbacks;
    invalid[1] = 3;
    write_table(invalid, nnue_reference::stored_stacks);
    passed &= expect_error("invalid NNUE stack fallback chain");
    invalid = nnue_reference::fallbacks;
    invalid[0] = 1;
    write_table(invalid, nnue_reference::stored_stacks - 1);
    passed &= expect_error("invalid NNUE stack fallback chain");
    write_table(nnue_reference::fallbacks, nnue_reference::stored_stacks,
                0x5A7AB1E5, nnue::SLOTS, 0);
    passed &= expect_error("unsupported NNUE feature transformer");
    write_table(nnue_reference::fallbacks, nnue_reference::stored_stacks);
    passed &= expect_error("truncated");
    {
        std::ofstream stream{path, std::ios::binary};
        const std::array<uint32_t, 3> old_header{0x6A448AFA, 0xC0357A83, 0};
        stream.write(reinterpret_cast<const char*>(old_header.data()), sizeof(old_header));
        if (!stream)
            throw std::runtime_error{"unable to write NNUE loader test fixture"};
    }
    passed &= expect_error("unsupported NNUE network format");
    std::filesystem::copy_file(nnue::NETWORK_PATH, path, std::filesystem::copy_options::overwrite_existing);
    {
        std::ofstream stream{path, std::ios::binary | std::ios::app};
        stream.put('\0');
        if (!stream)
            throw std::runtime_error{"unable to write NNUE loader test fixture"};
    }
    passed &= expect_error("unexpected trailing data");
    std::filesystem::remove(path);
    return passed;
}

}

bool run_nnue_tests() {
    const auto network = nnue::load();
    nnue::entry_t cached[2];
    for (auto& entry : cached)
        network->initialize(entry);
    std::vector<uint32_t> removed;
    std::vector<uint32_t> added;
    std::array<bool, nnue::SLOTS> buckets{};
    std::array<bool, 3> psq_blocks{};
    std::array<nnue::feature_buffer_t, 2> buffers;
    bool passed = true;

    const auto check_position = [&](const nnue_reference::case_t& test) {
        const position_t position{test.fen};
        const auto active = nnue::active_features(position, buffers);
        nnue::entry_t fresh[2];
        for (const side_t side : {WHITE, BLACK}) {
            auto& entry = fresh[side];
            network->initialize(entry);
            auto features = active[side];
            entry.feature_count = features.size();
            std::ranges::sort(features);
            std::ranges::copy(features, entry.features.begin());
            const auto hash = feature_hash(features);
            if (hash != test.feature_hashes[side])
                std::cerr << test.fen << " side " << int(side) << ": feature hash "
                          << hash << '\n';
            passed &= test_support::check(
                hash == test.feature_hashes[side],
                "PSQ, threat, and pawn-pair feature indices match Stockfish");
            passed &= test_support::check(
                std::ranges::adjacent_find(features) == features.end() &&
                    features.back() < nnue::L0,
                "active features are unique and within the network dimensions");
            network->update(entry, {}, features);

            removed.clear();
            added.clear();
            const auto previous = std::span{cached[side].features}.first(cached[side].feature_count);
            std::ranges::set_difference(previous, features,
                                        std::back_inserter(removed));
            std::ranges::set_difference(features, previous,
                                        std::back_inserter(added));
            network->update(cached[side], removed, added);
            std::ranges::copy(features, cached[side].features.begin());
            cached[side].feature_count = entry.feature_count;
            passed &= test_support::check(
                std::ranges::equal(cached[side].accumulation, entry.accumulation),
                "feature deltas reproduce a fresh accumulator exactly");
        }
        const auto side = position.side();
        const bool their_queen = !position.by(~side, Q).empty();
        buckets[network->layer_stack(position)] = true;
        passed &= test_support::check(network->layer_stack(position) == test.stack,
                                      "stored layer-stack selection matches Stockfish");
        const auto raw = nnue::stack_index(position);
        const auto fallback = nnue_reference::fallbacks[raw];
        size_t expected_stack = 0;
        for (size_t i = 0; i < fallback; ++i)
            expected_stack += nnue_reference::fallbacks[i] == i;
        passed &= test_support::check(network->layer_stack(position) == expected_stack,
                                      "raw configurations resolve through the network fallback table");
        const bool any_heavy_pieces = !position.by(R).empty() || !position.by(Q).empty();
        psq_blocks[their_queen ? 2 : any_heavy_pieces ? 1 : 0] = true;
        const auto score = network->evaluate(fresh[side], fresh[~side], position);
        if (score != test.score)
            std::cerr << test.fen << ": expected " << test.score << ", got " << score << '\n';
        passed &= test_support::check(score == test.score, "NNUE output matches Stockfish exactly");
        passed &= test_support::check(
            network->evaluate(cached[side], cached[~side], position) == score,
            "updated accumulator evaluation matches full refresh");
    };
    for (const auto& test : nnue_reference::cases)
        check_position(test);
    for (const auto& test : nnue_reference::stack_cases)
        check_position(test);
    passed &= test_support::check(std::ranges::count(buckets, true) == nnue_reference::stored_stacks,
                                  "reference positions cover all 92 stored network stacks");
    passed &= test_support::check(std::ranges::all_of(psq_blocks, [](const bool covered) { return covered; }),
                                  "reference positions cover all three K32H3 blocks");
    passed &= test_paired_updates(*network);
    passed &= test_stack_keys();
    passed &= test_network_errors();
    return passed;
}
