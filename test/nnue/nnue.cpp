#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include "reference_positions.hpp"
#include "test_support.hpp"

import nnue.nnue;
import core.position;

using namespace chess;

namespace {

uint64_t feature_hash(const std::vector<uint32_t>& features) {
    uint64_t hash = 14695981039346656037ULL;
    for (const auto feature : features)
        hash = (hash ^ feature) * 1099511628211ULL;
    return hash;
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
    {
        std::ofstream stream{path, std::ios::binary};
        const std::array<uint32_t, 4> header{0x6A448AFA, 0xE5CD8C39, 0, 0x86FEFD2F};
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
    std::filesystem::copy_file("../nnue.bin", path, std::filesystem::copy_options::overwrite_existing);
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
    bool passed = true;

    for (const auto& test : nnue_reference::cases) {
        const position_t position{test.fen};
        nnue::entry_t fresh[2];
        for (const side_t side : {WHITE, BLACK}) {
            auto& entry = fresh[side];
            network->initialize(entry);
            nnue::active_features(position, side, entry.features);
            passed &= test_support::check(
                feature_hash(entry.features) == test.feature_hashes[side],
                "PSQ, threat, and pawn-pair feature indices match Stockfish");
            passed &= test_support::check(
                std::ranges::adjacent_find(entry.features) == entry.features.end() &&
                    entry.features.back() < nnue::L0,
                "active features are unique and within the network dimensions");
            network->update(entry, {}, entry.features);

            removed.clear();
            added.clear();
            std::ranges::set_difference(cached[side].features, entry.features,
                                        std::back_inserter(removed));
            std::ranges::set_difference(entry.features, cached[side].features,
                                        std::back_inserter(added));
            network->update(cached[side], removed, added);
            cached[side].features = entry.features;
            passed &= test_support::check(
                std::ranges::equal(cached[side].accumulation, entry.accumulation),
                "feature deltas reproduce a fresh accumulator exactly");
        }
        const auto side = position.side();
        const bool our_queen = !position.by(side, Q).empty();
        const bool their_queen = !position.by(~side, Q).empty();
        const size_t count = position.by().size();
        buckets[(count - 1) / 4 * 4 + 2 * our_queen + their_queen] = true;
        const auto score = network->evaluate(fresh[side], fresh[~side], count,
                                             our_queen, their_queen);
        if (score != test.score)
            std::cerr << test.fen << ": expected " << test.score << ", got " << score << '\n';
        passed &= test_support::check(score == test.score, "NNUE output matches Stockfish exactly");
        passed &= test_support::check(
            network->evaluate(cached[side], cached[~side], count, our_queen, their_queen) == score,
            "updated accumulator evaluation matches full refresh");
    }
    passed &= test_support::check(std::ranges::all_of(buckets, [](const bool covered) { return covered; }),
                                  "reference positions cover all 32 network stacks");
    passed &= test_network_errors();
    return passed;
}
