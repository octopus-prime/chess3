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

std::ostream& operator<<(std::ostream& os, const chess::square_t square) {
    return os << char('a' + square.file()) << (char)('1' + square.rank());
}

std::ostream& operator<<(std::ostream& os, const chess::move_t move) {
    return os << move.from() << move.to();
}

void test_negamax(std::string_view fen, size_t max_depth) {
    using namespace std::chrono;
    chess::negamax_t negamax{fen};
    std::array<chess::move_t, 256> buffer;
    const auto start = steady_clock::now();
    for (size_t depth = 1; depth <= max_depth; ++depth) {
        const auto score = negamax(depth);
        const auto end = steady_clock::now();
        const auto ns = duration_cast<nanoseconds>(end - start).count();
        const auto ms = duration_cast<milliseconds>(end - start).count();
        const auto pv = negamax.extract_pv(buffer, depth);
        std::cout << "depth: " << depth << ", score: " << score << ", time: " << ms << " ms, nodes: " << negamax.nodes() << " perf: " << negamax.nodes() * 1e3 / ns << " MN/s, full: " << negamax.full() << ", pv: ";
        for (const auto& move : pv)
            std::cout << move << " ";
        std::cout << "\n";
    }
}

int main() {
    constexpr std::array fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", // startpos
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", // kiwipete
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",
        "4K3/1P1P1P1P/8/8/8/8/p1p1p1p1/4k3 w - -",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",
        "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -", // mate in 3
        "4r1k1/pp4pp/8/q1p1N3/5P2/1P4P1/P6P/3Q2K1 w - -", // mate in 5
        "r3k3/1pp1p1Kp/1p2P3/1p6/2N5/P2P2pr/5B1p/3Q1R2 w q -", // mate in 8
        "n1N3br/2p1Bpkr/1pP2R1b/pP1p1PpR/Pp4P1/1P6/1K1P4/8 w - -" // mate in 6
    };
    constexpr size_t depth = 6;
    for (const std::string_view fen : fens) {
        std::cout << "FEN: " << fen << "\n";
        test_perft(fen, depth);
        test_negamax(fen, depth + 4);
        std::cout << "\n";
    }
}
