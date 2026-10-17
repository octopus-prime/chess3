#include <array>
#include <iostream>
#include <chrono>
#include <string_view>

import engine.engine;
import engine.uci;

using namespace std::string_view_literals;

void bench() {
    constexpr std::array fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", // startpos
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", // kiwipete
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -",
        "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -", // mate in 3
        "4r1k1/pp4pp/8/q1p1N3/5P2/1P4P1/P6P/3Q2K1 w - -", // mate in 5
        "n1N3br/2p1Bpkr/1pP2R1b/pP1p1PpR/Pp4P1/1P6/1K1P4/8 w - -", // mate in 6
        "r3k3/1pp1p1Kp/1p2P3/1p6/2N5/P2P2pr/5B1p/3Q1R2 w q -" // mate in 8

        "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -", // d6d1; id "BK.01";
        "3r1k2/4npp1/1ppr3p/p6P/P2PPPP1/1NR5/5K2/2R5 w - -", // d4d5; id "BK.02";
        "2q1rr1k/3bbnnp/p2p1pp1/2pPp3/PpP1P1P1/1P2BNNP/2BQ1PRK/7R b - -", // f8g8; id "BK.03"; f6f5 f8g8 e7d8
        "rnbqkb1r/p3pppp/1p6/2ppP3/3N4/2P5/PPP1QPPP/R1B1KB1R w KQkq -", // e5e6; id "BK.04";
        "r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - -", // c3d5; id "BK.05"; a4
        "2r3k1/pppR1pp1/4p3/4P1P1/5P2/1P4K1/P1P5/8 w - -", // g5g6; id "BK.06";
        "1nk1r1r1/pp2n1pp/4p3/q2pPp1N/b1pP1P2/B1P2R2/2P1B1PP/R2Q2K1 w - -", // h5f6; id "BK.07";
        "4b3/p3kp2/6p1/3pP2p/2pP1P2/4K1P1/P3N2P/8 w - -", // f4f5; id "BK.08";
        "2kr1bnr/pbpq4/2n1pp2/3p3p/3P1P1B/2N2N1Q/PPP3PP/2KR1B1R w - -", // f1b5; id "BK.09"; f4f5 f1b5
        "3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - -", // f6d7; id "BK.10"; c6e5
        "2r1nrk1/p2q1ppp/bp1p4/n1pPp3/P1P1P3/2PBB1N1/4QPPP/R4RK1 w - -", // f2f4; id "BK.11";
        "r3r1k1/ppqb1ppp/8/4p1NQ/8/2P5/PP3PPP/R3R1K1 b - -", // d7f5; id "BK.12";
        "r2q1rk1/4bppp/p2p4/2pP4/3pP3/3Q4/PP1B1PPP/R3R1K1 w - -", // b2b4; id "BK.13";
        "rnb2r1k/pp2p2p/2pp2p1/q2P1p2/8/1Pb2NP1/PB2PPBP/R2Q1RK1 w - -", // d1d2; id "BK.14"; d1e1 d1d2
        "2r3k1/1p2q1pp/2b1pr2/p1pp4/6Q1/1P1PP1R1/P1PN2PP/5RK1 w - -", // g4g7; id "BK.15";
        "r1bqkb1r/4npp1/p1p4p/1p1pP1B1/8/1B6/PPPN1PPP/R2Q1RK1 w kq -", // d2e4; id "BK.16";
        "r2q1rk1/1ppnbppp/p2p1nb1/3Pp3/2P1P1P1/2N2N1P/PPB1QP2/R1B2RK1 b - -", // c7c6; id "BK.17"; h7h5
        "r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - -", // f7f5; id "BK.18"; c5b3
        "3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - -", // e8e4; id "BK.19";
        "r4k2/pb2bp1r/1p1qp2p/3pNp2/3P1P2/2N3P1/PPP1Q2P/2KRR3 w - -", // g3g4; id "BK.20";
        "3rn2k/ppb2rpp/2ppqp2/5N2/2P1P3/1P5Q/PB3PPP/3RR1K1 w - -", // f5h6; id "BK.21";
        "2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - -", // b7e4; id "BK.22";
        "r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq -", // f7f6; id "BK.23"; c8f5
        "r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3QNPPP/5RK1 w - -", // e4f5; id "BK.24"; f2f4
    };
    constexpr chess::ply_t depth = 12;
    chess::engine_t engine{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};
    size_t nodes = 0;
    const auto start = std::chrono::steady_clock::now();
    for (const std::string_view fen : fens) {
        engine.setup(fen, "");
        engine(depth, std::chrono::hours{24});
        nodes += engine.nodes();
        engine.clear();
    }
    const auto end = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "time: " << duration.count() << " ms\n";
    std::cout << "nodes: " << nodes << "\n";
    std::cout << "perf: " << (1e-3 * nodes / duration.count()) << "MN/s\n";
}

int main(int argc, char* argv[]) {
    try {
        if (argc > 1 && "bench"sv == std::string_view{argv[1]}) {
            bench();
            return 0;
        }
        chess::uci_t uci;
        uci(std::cin);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
    }
    return 0;
}
