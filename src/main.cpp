#include <chrono>
#include <iostream>

#include "perft.hpp"

void test_perft(std::string_view fen, size_t max_depth) {
    using namespace std::chrono;
    chess::perft_t perft{fen};
    for (size_t depth = 1; depth <= max_depth; ++depth) {
        const auto start = steady_clock::now();
        const auto nodes = perft(depth);
        const auto end = steady_clock::now();
        const auto ns = duration_cast<nanoseconds>(end - start).count();
        const auto ms = duration_cast<milliseconds>(end - start).count();
        std::cout << "perft(" << depth << "): " << nodes << ", time: " << ms << " ms, perf: " << nodes * 1e3 / ns << " MN/s\n";
    }
}

void test_negamax(std::string_view fen, size_t max_depth) {
    using namespace std::chrono;
    chess::negamax_t negamax{fen};
    for (size_t depth = 1; depth <= max_depth; ++depth) {
        const auto start = steady_clock::now();
        const auto score = negamax(depth);
        const auto end = steady_clock::now();
        const auto ns = duration_cast<nanoseconds>(end - start).count();
        const auto ms = duration_cast<milliseconds>(end - start).count();
        std::array<chess::move_t, 256> buffer;
        const auto pv = negamax.extract_pv(buffer, 10);
        std::cout << "depth: " << depth << ", score: " << score << ", time: " << ms << " ms, nodes: " << negamax.nodes() << " perf: " << negamax.nodes() * 1e3 / ns << " MN/s, full: " << negamax.full() << ", pv: ";
        for (const auto& move : pv)
            std::cout << char('a' + move.from().file()) << (char)('1' + move.from().rank()) << char('a' + move.to().file()) << (char)('1' + move.to().rank()) << " ";
        std::cout << "\n";
        negamax.reset();
    }
}

int main() {
    // constexpr std::string_view fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    // constexpr std::string_view fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -";
    // constexpr std::string_view fen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1";
    // constexpr std::string_view fen = "4K3/1P1P1P1P/8/8/8/8/p1p1p1p1/4k3 w - -";
    // constexpr std::string_view fen = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1";
    constexpr std::string_view fen = "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -";
    constexpr size_t depth = 6;
    test_perft(fen, depth);
    test_negamax(fen, depth + 4);
}
