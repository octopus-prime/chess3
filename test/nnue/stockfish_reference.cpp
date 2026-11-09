#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "attacks.h"
#include "position.h"
#include "nnue/network.h"
#include "nnue/nnue_accumulator.h"
#include "nnue/nnue_misc.h"

// Build against Stockfish, not chess3, to generate independent evaluation references.
int main(int argc, char** argv) {
    using namespace Stockfish;
    using namespace Stockfish::Eval::NNUE;
    if (argc != 2)
        return 2;
    Attacks::init();
    Position::init();
    auto network = std::make_unique<Network>();
    EvalFile file;
    const auto path = std::filesystem::absolute(argv[1]);
    network->load_external(path.parent_path(), path.filename(), file);
    if (!file.current) {
        std::cerr << "Stockfish could not load " << path << '\n';
        return 3;
    }
    auto cache = std::make_unique<AccumulatorCaches>(*network);
    auto stack = std::make_unique<AccumulatorStack>();
    std::string fen;
    while (std::getline(std::cin, fen)) {
        Position position;
        StateInfo state;
        position.set(fen, false, &state);
        stack->reset();
        const auto score = network->evaluate(position, *stack, *cache);
        const auto trace = network->trace_evaluate(position, *stack, *cache);
        std::cout << score << ' ' << trace.correctBucket;
        for (const Color side : {WHITE, BLACK}) {
            std::vector<uint32_t> indices;
            const auto king = position.square<KING>(side);
            for (Bitboard pieces = position.pieces(); pieces;) {
                const auto square = pop_lsb(pieces);
                indices.push_back(59808 + 4560 +
                    PSQFeatureSet::block_offset(PSQFeatureSet::block(position, side)) +
                    PSQFeatureSet::make_index(side, square, position.piece_on(square), king));
            }
            ThreatFeatureSet::IndexList threats;
            ThreatFeatureSet::append_active_indices(side, position, threats);
            indices.insert(indices.end(), threats.begin(), threats.end());
            PairFeatureSet::IndexList pairs;
            PairFeatureSet::append_active_indices(side, position, pairs);
            indices.insert(indices.end(), pairs.begin(), pairs.end());
            std::sort(indices.begin(), indices.end());
            uint64_t hash = 14695981039346656037ULL;
            for (const auto index : indices)
                hash = (hash ^ index) * 1099511628211ULL;
            std::cout << ' ' << hash;
        }
        std::cout << '\n';
    }
}
