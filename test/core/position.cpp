#include <cstddef>
#include <cstdint>
#include <array>
#include <span>
#include <iostream>
#include <cstdlib>
#include <string_view>
#include <algorithm>
#include <tuple>
#include "test_support.hpp"

import types.squares;
import types.piece;
import core.move;
import core.position;

using namespace chess;

constexpr bool test_pin_detection_after_moves();
constexpr bool test_checkers_and_pinned();
constexpr bool test_castling_moves();
constexpr bool test_en_passant_moves();
bool test_en_passant_generation();
bool test_pinned_pawn_generation();
bool test_promotion_generation();
bool test_tactical_generation();
constexpr bool test_tactical_query();
bool test_king_slider_evasions();
constexpr bool test_position_hash();
bool test_hash_tree();

bool run_position_tests() {
    bool passed = test_support::check(test_pin_detection_after_moves(), "pin detection after move and undo");
    passed &= test_support::check(test_checkers_and_pinned(), "checkers and pinned lifecycle");
    passed &= test_support::check(test_castling_moves(), "castling execution and rights lifecycle");
    passed &= test_support::check(test_en_passant_moves(), "en-passant execution and target lifecycle");
    passed &= test_support::check(test_en_passant_generation(), "legal en-passant generation");
    passed &= test_support::check(test_pinned_pawn_generation(), "pinned-pawn generation");
    passed &= test_support::check(test_promotion_generation(), "SIMD promotion generation");
    passed &= test_support::check(test_tactical_generation(), "captures and promotions generation");
    passed &= test_support::check(test_tactical_query(), "tactical move classification");
    passed &= test_support::check(test_king_slider_evasions(), "king slider evasions");
    passed &= test_support::check(test_position_hash(), "position hash identity");
    passed &= test_support::check(test_hash_tree(), "incremental hash tree");
    static_assert(sizeof(move_t) == sizeof(uint16_t));
    static_assert(move_t(a1, b2, Q) == 21056);
    static_assert(move_t(a1, b2, Q).from() == a1);
    static_assert(move_t(a1, b2, Q).to() == b2);
    static_assert(move_t(a1, b2, Q).promotion() == Q);

    constexpr position_t position{};
    static_assert(sizeof(position_t::stack_t) ==
                  1024 * sizeof(position_t::state_t) + sizeof(size_t));
    static_assert(sizeof(position_t::state_t) == 32);
    static_assert(position.by() == squares_t{_1, _2, _7, _8});
    static_assert(position.by(WHITE) == squares_t{_1, _2});
    static_assert(position.by(BLACK) == squares_t{_7, _8});
    static_assert(position.by(K) == squares_t{e1, e8});
    static_assert(position.by(Q) == squares_t{d1, d8});
    static_assert(position.by(R) == squares_t{a1, h1, a8, h8});
    static_assert(position.by(B) == squares_t{c1, f1, c8, f8});
    static_assert(position.by(N) == squares_t{b1, g1, b8, g8});
    static_assert(position.by(P) == squares_t{_2, _7});
    static_assert(position.by(WK) == squares_t{e1});
    static_assert(position.by(WQ) == squares_t{d1});
    static_assert(position.by(WR) == squares_t{a1, h1});
    static_assert(position.by(WB) == squares_t{c1, f1});
    static_assert(position.by(WN) == squares_t{b1, g1});
    static_assert(position.by(WP) == squares_t{_2});
    static_assert(position.by(BK) == squares_t{e8});
    static_assert(position.by(BQ) == squares_t{d8});
    static_assert(position.by(BR) == squares_t{a8, h8});
    static_assert(position.by(BB) == squares_t{c8, f8});
    static_assert(position.by(BN) == squares_t{b8, g8});
    static_assert(position.by(BP) == squares_t{_7});

    static_assert(position.attackers(WHITE) == (squares_t{_1, _2, _3} & ~squares_t{a1, h1}));
    static_assert(position.attackers(BLACK) == (squares_t{_6, _7, _8} & ~squares_t{a8, h8}));
    static_assert(position.attackers(f3) == squares_t{g1, e2, g2});
    static_assert(position.attackers(f6) == squares_t{g8, e7, g7});

    return passed;
}

void test_sinpers_and_blockers() {
    static_assert(position_t{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"}.find_snipers_and_blockers(WHITE) == std::make_tuple(squares_t{}, squares_t{}));

    struct case_t {
        std::string_view fen;
        side_t side;
        squares_t snipers;
        squares_t blockers;
    };
    constexpr std::array cases{
        case_t{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1", BLACK, {}, {}},
        case_t{"k7/8/8/8/4K3/8/8/8 w - - 0 1", WHITE, {}, {}},

        // One friendly blocker on each of the eight king rays.
        case_t{"k3r3/8/8/4N3/4K3/8/8/8 w - - 0 1", WHITE, {e8}, {e5}},
        case_t{"k7/8/8/8/4K3/4N3/8/4r3 w - - 0 1", WHITE, {e1}, {e3}},
        case_t{"k7/8/8/8/4KN1r/8/8/8 w - - 0 1", WHITE, {h4}, {f4}},
        case_t{"k7/8/8/8/r2NK3/8/8/8 w - - 0 1", WHITE, {a4}, {d4}},
        case_t{"k7/7b/8/5N2/4K3/8/8/8 w - - 0 1", WHITE, {h7}, {f5}},
        case_t{"k7/1b6/8/3N4/4K3/8/8/8 w - - 0 1", WHITE, {b7}, {d5}},
        case_t{"k7/8/8/8/4K3/5N2/8/7b w - - 0 1", WHITE, {h1}, {f3}},
        case_t{"k7/8/8/8/4K3/3N4/8/1b6 w - - 0 1", WHITE, {b1}, {d3}},

        // Queens pin along both orthogonal and diagonal rays.
        case_t{"k3q3/8/8/4P3/4K3/8/8/8 w - - 0 1", WHITE, {e8}, {e5}},
        case_t{"k7/7q/8/5R2/4K3/8/8/8 w - - 0 1", WHITE, {h7}, {f5}},
        case_t{"4k3/4n3/8/8/8/8/8/K3R3 b - - 0 1", BLACK, {e1}, {e7}},
        case_t{"7k/6p1/8/8/8/8/8/B6K w - - 0 1", BLACK, {a1}, {g7}},

        // Checks without a blocker, and enemy-only blockers, are not pins.
        case_t{"k3r3/8/8/8/4K3/8/8/8 w - - 0 1", WHITE, {}, {}},
        case_t{"k7/8/8/4r3/4K3/8/8/8 w - - 0 1", WHITE, {}, {}},
        case_t{"k3r3/8/8/4n3/4K3/8/8/8 w - - 0 1", WHITE, {}, {}},

        // Two intervening pieces prevent a pin, regardless of their colors.
        case_t{"k3r3/8/4B3/4N3/4K3/8/8/8 w - - 0 1", WHITE, {}, {}},
        case_t{"k3r3/8/4b3/4N3/4K3/8/8/8 w - - 0 1", WHITE, {}, {}},
        case_t{"k3r3/8/4B3/4n3/4K3/8/8/8 w - - 0 1", WHITE, {}, {}},

        // Friendly sliders, wrong slider directions, and unaligned enemies.
        case_t{"k3R3/8/8/4N3/4K3/8/8/8 w - - 0 1", WHITE, {}, {}},
        case_t{"k3b3/8/8/4N3/4K3/8/8/8 w - - 0 1", WHITE, {}, {}},
        case_t{"k7/7r/8/5N2/4K3/8/8/8 w - - 0 1", WHITE, {}, {}},
        case_t{"k4q2/8/8/4N3/4K3/8/8/8 w - - 0 1", WHITE, {}, {}},

        // Only the nearest matching enemy slider on a ray is returned.
        case_t{"k3q3/8/4r3/4N3/4K3/8/8/8 w - - 0 1", WHITE, {e6}, {e5}},
        case_t{"k7/8/8/8/4K3/4N3/4r3/4q3 w - - 0 1", WHITE, {e2}, {e3}},
        case_t{"k7/8/8/8/qr1NK3/8/8/8 w - - 0 1", WHITE, {b4}, {d4}},
        case_t{"k7/8/8/8/4K3/3N4/2b5/1q6 w - - 0 1", WHITE, {c2}, {d3}},
        case_t{"k7/8/8/4r3/4K3/8/8/4q3 w - - 0 1", WHITE, {}, {}},

        // Multiple independent pins accumulate in both output bitboards.
        case_t{"k3r3/7b/8/4BN2/4KN1r/8/8/8 w - - 0 1",
               WHITE, {e8, h7, h4}, {e5, f5, f4}},
        case_t{"k7/8/8/8/r2NK3/3NN3/8/1b2r3 w - - 0 1",
               WHITE, {a4, b1, e1}, {d4, d3, e3}},

        // Board-edge kings and a blocker adjacent to the sniper.
        case_t{"r6k/8/8/8/8/P7/8/K7 w - - 0 1", WHITE, {a8}, {a3}},
        case_t{"k7/8/8/8/8/8/6P1/4r1RK w - - 0 1", WHITE, {e1}, {g1}},
        case_t{"k3r3/4N3/8/8/4K3/8/8/8 w - - 0 1", WHITE, {e8}, {e7}}
    };
    static_assert([&] {
        for (const auto& test : cases) {
            const position_t position{test.fen};
            if (position.find_snipers_and_blockers(test.side) !=
                std::make_tuple(test.snipers, test.blockers))
                return false;
        }
        return true;
    }());
}

constexpr bool test_pin_detection_after_moves() {
    if (!std::get<1>(position_t{}.find_snipers_and_blockers(WHITE)).empty())
        return false;
    position_t position{"3R4/8/8/4n3/4k3/8/8/K7 w - - 0 1"};
    const move_t pin{d8, e8};
    const move_t king_move{e4, d4};
    position.do_move(pin);
    if (position.side() != BLACK ||
        std::get<1>(position.find_snipers_and_blockers(position.side())) != squares_t{e5})
        return false;
    position.do_move(king_move);
    if (position.side() != WHITE ||
        !std::get<1>(position.find_snipers_and_blockers(position.side())).empty())
        return false;
    position.undo_move(king_move);
    if (position.side() != BLACK ||
        std::get<1>(position.find_snipers_and_blockers(position.side())) != squares_t{e5})
        return false;
    position.undo_move(pin);
    if (position.side() != WHITE ||
        !std::get<1>(position.find_snipers_and_blockers(position.side())).empty())
        return false;

    position_t capture{"4r2k/8/8/8/8/8/4R3/4K3 w - - 0 1"};
    if (std::get<1>(capture.find_snipers_and_blockers(capture.side())) != squares_t{e2})
        return false;
    const move_t take{e2, e8};
    capture.do_move(take);
    if (!std::get<1>(capture.find_snipers_and_blockers(capture.side())).empty())
        return false;
    capture.undo_move(take);
    if (std::get<1>(capture.find_snipers_and_blockers(capture.side())) != squares_t{e2})
        return false;

    position_t promotion{"8/4P3/8/4n3/4k3/8/8/K7 w - - 0 1"};
    const move_t promote{e7, e8, R};
    promotion.do_move(promote);
    if (std::get<1>(promotion.find_snipers_and_blockers(promotion.side())) != squares_t{e5})
        return false;
    promotion.undo_move(promote);
    return std::get<1>(promotion.find_snipers_and_blockers(promotion.side())).empty() &&
           promotion.at(e7) == WP;
}

static_assert(test_pin_detection_after_moves());

constexpr bool test_checkers_and_pinned() {
    struct fen_case_t {
        std::string_view fen;
        squares_t checkers;
        squares_t pinned;
    };
    constexpr std::array fen_cases{
        fen_case_t{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {}, {}},
        fen_case_t{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1", {}, {}},
        fen_case_t{"k3r3/8/8/8/8/8/8/4K3 w - - 0 1", {e8}, {}},
        fen_case_t{"4k3/8/8/8/8/8/8/K3R3 b - - 0 1", {e1}, {}},
        fen_case_t{"k7/8/8/8/7b/8/8/4K3 w - - 0 1", {h4}, {}},
        fen_case_t{"k7/8/8/8/8/8/8/q3K3 w - - 0 1", {a1}, {}},
        fen_case_t{"k7/8/8/8/8/5n2/8/4K3 w - - 0 1", {f3}, {}},
        fen_case_t{"k7/8/8/8/8/8/3p4/4K3 w - - 0 1", {d2}, {}},
        fen_case_t{"4k3/3P4/8/8/8/8/8/K7 b - - 0 1", {d7}, {}},
        fen_case_t{"k3r3/8/8/8/7b/8/8/4K3 w - - 0 1", {e8, h4}, {}},
        fen_case_t{"k3r3/8/8/8/8/8/4N3/4K3 w - - 0 1", {}, {e2}},
        fen_case_t{"4k3/4n3/8/8/8/8/8/K3R3 b - - 0 1", {}, {e7}},
        fen_case_t{"k3r3/8/8/8/8/8/4n3/4K3 w - - 0 1", {}, {}},
        fen_case_t{"k3r3/8/8/8/7b/8/4N3/4K3 w - - 0 1", {h4}, {e2}},
        fen_case_t{"k3r3/8/8/8/8/4P3/4N3/4K3 w - - 0 1", {}, {}}
    };
    for (const auto& test : fen_cases) {
        const position_t position{test.fen};
        if (position.checkers() != test.checkers ||
            std::get<1>(position.find_snipers_and_blockers(position.side())) != test.pinned)
            return false;
    }

    struct move_case_t {
        std::string_view fen;
        move_t move;
        squares_t checkers;
        squares_t pinned;
    };
    constexpr std::array move_cases{
        move_case_t{"4k3/8/8/8/8/8/8/K2R4 w - - 0 1", {d1, e1}, {e1}, {}},
        move_case_t{"k2r4/8/8/8/8/8/8/4K3 b - - 0 1", {d8, e8}, {e8}, {}},
        move_case_t{"4k3/8/8/4B3/8/8/8/K3R3 w - - 0 1", {e5, b2}, {e1}, {}},
        move_case_t{"4k3/8/8/4B3/8/8/8/K3R3 w - - 0 1", {e5, b5}, {e1, b5}, {}},
        move_case_t{"4k3/8/8/8/8/5N2/8/K7 w - - 0 1", {f3, d6}, {d6}, {}},
        move_case_t{"4k3/8/3P4/8/8/8/8/K7 w - - 0 1", {d6, d7}, {d7}, {}},
        move_case_t{"3R4/8/8/4n3/4k3/8/8/K7 w - - 0 1", {d8, e8}, {}, {e5}},
        move_case_t{"k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1", {e2, e8}, {e8}, {}},
        move_case_t{"k3r3/8/8/8/8/8/8/4K3 w - - 0 1", {e1, d1}, {}, {}},
        move_case_t{"k3r3/8/8/8/8/8/4B3/4K3 w - - 0 1", {e2, e3}, {}, {}},
        move_case_t{"7k/6P1/8/8/8/8/8/K7 w - - 0 1", {g7, g8, Q}, {g8}, {}},
        move_case_t{"8/4P3/8/4n3/4k3/8/8/K7 w - - 0 1", {e7, e8, R}, {}, {e5}}
    };
    for (const auto& test : move_cases) {
        position_t position{test.fen};
        const position_t before = position;
        position.do_move(test.move);
        if (position.checkers() != test.checkers ||
            std::get<1>(position.find_snipers_and_blockers(position.side())) != test.pinned ||
            position.side() != ~before.side())
            return false;
        position.undo_move(test.move);
        if (position.checkers() != before.checkers() ||
            position.find_snipers_and_blockers(position.side()) !=
                before.find_snipers_and_blockers(before.side()) ||
            position.side() != before.side())
            return false;
        for (square_t square = a1; square <= h8; ++square)
            if (position.at(square) != before.at(square))
                return false;
    }

    // Nested undo must restore a non-empty checker cache from the previous ply.
    position_t position{"4k3/8/8/8/8/8/8/K2R4 w - - 0 1"};
    const move_t check{d1, e1};
    const move_t escape{e8, d8};
    position.do_move(check);
    position.do_move(escape);
    if (!position.checkers().empty() ||
        !std::get<1>(position.find_snipers_and_blockers(position.side())).empty())
        return false;
    position.undo_move(escape);
    if (position.checkers() != squares_t{e1} ||
        !std::get<1>(position.find_snipers_and_blockers(position.side())).empty())
        return false;
    position.undo_move(check);
    return position.checkers().empty() &&
           std::get<1>(position.find_snipers_and_blockers(position.side())).empty() &&
           position.side() == WHITE;
}

static_assert(test_checkers_and_pinned());

constexpr hash_t recompute_hash(const position_t& position) {
    hash_t hash{};
    for (square_t square = a1; square <= h8; ++square)
        if (position.at(square) != NO_PIECE)
            hash ^= hashes::lookup::piece(position.at(square), square);
    if (position.side() == BLACK)
        hash ^= hashes::lookup::side();
    for (const square_t square : position.castling())
        hash ^= hashes::lookup::castle(square);
    for (const square_t square : position.en_passant())
        hash ^= hashes::lookup::ep(square.file());
    return hash;
}

constexpr bool same_position(const position_t& actual, const position_t& expected) {
        if (actual.hash() != expected.hash() || actual.hash() != recompute_hash(actual) ||
            expected.hash() != recompute_hash(expected))
            return false;
        if (actual.side() != expected.side() || actual.by() != expected.by() ||
            actual.checkers() != expected.checkers() || actual.castling() != expected.castling() ||
            actual.en_passant() != expected.en_passant())
            return false;
        for (const side_t side : {WHITE, BLACK}) {
            if (actual.by(side) != expected.by(side))
                return false;
            for (const type_t type : {P, N, B, R, Q, K})
                if (actual.by(type) != expected.by(type) ||
                    actual.by(side, type) != expected.by(side, type))
                    return false;
        }
        for (square_t square = a1; square <= h8; ++square)
            if (actual.at(square) != expected.at(square))
                return false;
        return true;
}

constexpr bool test_position_hash() {
    constexpr position_t initial{};
    constexpr position_t black{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1"};
    if (initial.hash() != recompute_hash(initial) ||
        (initial.hash() ^ black.hash()) != hashes::lookup::side())
        return false;
    constexpr position_t clocks{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 42 99"};
    if (initial.hash() != clocks.hash())
        return false;
    constexpr position_t no_rights{"r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1"};
    constexpr std::array rights{
        "r3k2r/8/8/8/8/8/8/R3K2R w K - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w Q - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w k - 0 1",
        "r3k2r/8/8/8/8/8/8/R3K2R w q - 0 1"
    };
    constexpr std::array<square_t, 4> rook_squares{h1, a1, h8, a8};
    for (size_t index = 0; index < rights.size(); ++index) {
        const position_t position{rights[index]};
        if (position.hash() != recompute_hash(position) ||
            (position.hash() ^ no_rights.hash()) != hashes::lookup::castle(rook_squares[index]))
            return false;
    }
    constexpr position_t no_ep{"4k3/8/8/4p3/8/8/8/4K3 w - - 0 1"};
    constexpr position_t ep{"4k3/8/8/4p3/8/8/8/4K3 w - e6 0 1"};
    if ((ep.hash() ^ no_ep.hash()) != hashes::lookup::ep(e) ||
        ep.hash() != recompute_hash(ep))
        return false;

    position_t first{};
    position_t second{};
    first.do_move({g1, f3});
    first.do_move({g8, f6});
    first.do_move({b1, c3});
    first.do_move({b8, c6});
    second.do_move({b1, c3});
    second.do_move({b8, c6});
    second.do_move({g1, f3});
    second.do_move({g8, f6});
    return same_position(first, second);
}

static_assert(test_position_hash());

bool test_hash_tree() {
    const auto visit = [](auto&& self, position_t& position, int depth) -> bool {
        if (position.hash() != recompute_hash(position))
            return false;
        if (depth == 0)
            return true;
        const hash_t before = position.hash();
        std::array<move_t, 256> buffer;
        const auto moves = position.generate_all_moves(buffer);
        bool passed = true;
        for (const move_t move : moves) {
            position.do_move(move);
            passed &= self(self, position, depth - 1);
            position.undo_move(move);
            passed &= position.hash() == before && position.hash() == recompute_hash(position);
        }
        return passed;
    };
    constexpr std::array positions{
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "4k3/8/8/2PpP3/8/8/8/4K3 w - d6 0 1",
        "4k3/8/8/8/2pPp3/8/8/4K3 b - d3 0 1",
        "k2r1r2/4P3/8/8/8/8/8/7K w - - 0 1",
        "7k/8/8/8/8/8/4p3/K2R1R2 b - - 0 1"
    };
    bool passed = true;
    for (const auto fen : positions) {
        position_t position{fen};
        const bool matches = visit(visit, position, 3);
        if (!matches)
            std::cerr << "Incremental hash mismatch: " << fen << '\n';
        passed &= test_support::check(matches, "hash matches recomputation after every move and undo");
    }
    return passed;
}

constexpr bool test_castling_moves() {
    struct castle_case_t {
        std::string_view before;
        move_t move;
        std::string_view after;
    };
    constexpr std::array castles{
        castle_case_t{"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", {e1, g1},
                      "r3k2r/8/8/8/8/8/8/R4RK1 b kq - 0 1"},
        castle_case_t{"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", {e1, c1},
                      "r3k2r/8/8/8/8/8/8/2KR3R b kq - 0 1"},
        castle_case_t{"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", {e8, g8},
                      "r4rk1/8/8/8/8/8/8/R3K2R w KQ - 0 1"},
        castle_case_t{"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", {e8, c8},
                      "2kr3r/8/8/8/8/8/8/R3K2R w KQ - 0 1"},
        castle_case_t{"5k2/8/8/8/8/8/8/4K2R w K - 0 1", {e1, g1},
                      "5k2/8/8/8/8/8/8/5RK1 b - - 0 1"},
        castle_case_t{"3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", {e1, c1},
                      "3k4/8/8/8/8/8/8/2KR4 b - - 0 1"},
        castle_case_t{"4k2r/8/8/8/8/8/8/5K2 b k - 0 1", {e8, g8},
                      "5rk1/8/8/8/8/8/8/5K2 w - - 0 1"},
        castle_case_t{"r3k3/8/8/8/8/8/8/3K4 b q - 0 1", {e8, c8},
                      "2kr4/8/8/8/8/8/8/3K4 w - - 0 1"},
        castle_case_t{"r3k2r/8/8/8/3pP3/8/8/R3K2R b KQkq e3 0 1", {e8, g8},
                      "r4rk1/8/8/8/3pP3/8/8/R3K2R w KQ - 0 1"}
    };
    for (const auto& test : castles) {
        position_t position{test.before};
        const position_t before = position;
        position.do_move(test.move);
        if (!same_position(position, position_t{test.after}))
            return false;
        position.undo_move(test.move);
        if (!same_position(position, before))
            return false;
    }

    struct rights_case_t {
        side_t side;
        move_t move;
        squares_t remaining;
    };
    constexpr std::array rights_cases{
        rights_case_t{WHITE, {e1, f1}, {a8, h8}},
        rights_case_t{BLACK, {e8, f8}, {a1, h1}},
        rights_case_t{WHITE, {a1, a2}, {h1, a8, h8}},
        rights_case_t{WHITE, {h1, h2}, {a1, a8, h8}},
        rights_case_t{BLACK, {a8, a7}, {a1, h1, h8}},
        rights_case_t{BLACK, {h8, h7}, {a1, h1, a8}},
        rights_case_t{WHITE, {a1, a8}, {h1, h8}},
        rights_case_t{WHITE, {h1, h8}, {a1, a8}},
        rights_case_t{BLACK, {a8, a1}, {h1, h8}},
        rights_case_t{BLACK, {h8, h1}, {a1, a8}}
    };
    for (const auto& test : rights_cases) {
        position_t position{test.side == WHITE
            ? "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"
            : "r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1"};
        const position_t before = position;
        position.do_move(test.move);
        if (position.castling() != test.remaining)
            return false;
        position.undo_move(test.move);
        if (!same_position(position, before))
            return false;
    }

    position_t position{"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"};
    const position_t before = position;
    const move_t white{e1, g1};
    const move_t black{e8, c8};
    position.do_move(white);
    const position_t after_white = position;
    position.do_move(black);
    if (!same_position(position, position_t{"2kr3r/8/8/8/8/8/8/R4RK1 w - - 0 1"}))
        return false;
    position.undo_move(black);
    if (!same_position(position, after_white))
        return false;
    position.undo_move(white);
    return same_position(position, before);
}

static_assert(test_castling_moves());

constexpr bool test_en_passant_moves() {
    struct case_t {
        std::string_view before;
        move_t move;
        std::string_view after;
    };
    constexpr std::array cases{
        case_t{"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", {e5, d6},
               "4k3/8/3P4/8/8/8/8/4K3 b - - 0 1"},
        case_t{"4k3/8/8/2Pp4/8/8/8/4K3 w - d6 0 1", {c5, d6},
               "4k3/8/3P4/8/8/8/8/4K3 b - - 0 1"},
        case_t{"4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1", {e4, d3},
               "4k3/8/8/8/8/3p4/8/4K3 w - - 0 1"},
        case_t{"4k3/8/8/8/2pP4/8/8/4K3 b - d3 0 1", {c4, d3},
               "4k3/8/8/8/8/3p4/8/4K3 w - - 0 1"},
        case_t{"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", {e2, e4},
               "4k3/8/8/8/4P3/8/8/4K3 b - e3 0 1"},
        case_t{"4k3/4p3/8/8/8/8/8/4K3 b - - 0 1", {e7, e5},
               "4k3/8/8/4p3/8/8/8/4K3 w - e6 0 1"},
        case_t{"r3k2r/8/8/3pP3/8/8/8/R3K2R w KQkq d6 0 1", {e1, f1},
               "r3k2r/8/8/3pP3/8/8/8/R4K1R b kq - 0 1"},
        case_t{"r3k2r/8/8/3pP3/8/8/8/R3K2R w KQkq d6 0 1", {a1, a2},
               "r3k2r/8/8/3pP3/8/8/R7/4K2R b Kkq - 0 1"},
        case_t{"4k3/8/8/3pP3/8/8/2P5/4K3 w - d6 0 1", {c2, c3},
               "4k3/8/8/3pP3/8/2P5/8/4K3 b - - 0 1"},
        case_t{"4k3/8/8/3pP3/8/8/2P5/4K3 w - d6 0 1", {c2, c4},
               "4k3/8/8/3pP3/2P5/8/8/4K3 b - c3 0 1"},
        case_t{"4k3/8/5n2/3pP3/8/8/8/4K3 w - d6 0 1", {e5, f6},
               "4k3/8/5P2/3p4/8/8/8/4K3 b - - 0 1"},
        case_t{"8/8/8/R2pP2k/8/8/8/4K3 w - d6 0 1", {e5, d6},
               "8/8/3P4/R6k/8/8/8/4K3 b - - 0 1"},
        case_t{"4k3/8/8/8/r2Pp2K/8/8/8 b - d3 0 1", {e4, d3},
               "4k3/8/8/8/r6K/3p4/8/8 w - - 0 1"}
    };
    for (const auto& test : cases) {
        position_t position{test.before};
        const position_t before = position;
        position.do_move(test.move);
        if (!same_position(position, position_t{test.after}))
            return false;
        position.undo_move(test.move);
        if (!same_position(position, before))
            return false;
    }

    position_t position{"4k3/3p4/8/4P3/8/8/8/4K3 b - - 0 1"};
    const position_t before = position;
    const move_t push{d7, d5};
    const move_t take{e5, d6};
    position.do_move(push);
    const position_t after_push = position;
    position.do_move(take);
    if (!same_position(position, position_t{"4k3/8/3P4/8/8/8/8/4K3 b - - 0 1"}))
        return false;
    position.undo_move(take);
    if (!same_position(position, after_push))
        return false;
    position.undo_move(push);
    return same_position(position, before);
}

static_assert(test_en_passant_moves());

bool test_king_slider_evasions() {
    struct case_t {
        std::string_view fen;
        square_t from;
        square_t forbidden;
        square_t allowed;
    };
    constexpr std::array cases{
        case_t{"k3r3/8/8/8/8/8/4K3/8 w - - 0 1", e2, e1, d2},
        case_t{"8/4k3/8/8/8/8/8/K3R3 b - - 0 1", e7, e8, d7},
        case_t{"k7/8/8/8/8/4b3/3K4/8 w - - 0 1", d2, c1, c2},
        case_t{"8/3k4/4B3/8/8/8/8/K7 b - - 0 1", d7, c8, c7}
    };
    bool passed = true;
    for (const auto& test : cases) {
        position_t position{test.fen};
        const position_t before = position;
        std::array<move_t, 256> buffer;
        const auto moves = position.generate_all_moves(buffer);
        passed &= test_support::check(
            std::ranges::count(moves, move_t{test.from, test.forbidden}) == 0,
            "king cannot retreat behind its old square along a checking ray");
        passed &= test_support::check(
            std::ranges::count(moves, move_t{test.from, test.allowed}) == 1,
            "king can leave the checking ray");
        for (const move_t move : moves) {
            position.do_move(move);
            const side_t side = ~position.side();
            passed &= test_support::check(
                (position.attackers(position.by(side, K).front()) & position.by(~side)).empty(),
                "king evasion preserves king safety");
            position.undo_move(move);
            passed &= test_support::check(same_position(position, before), "king evasion round trip");
        }
    }
    return passed;
}

bool test_promotion_generation() {
    struct case_t {
        std::string_view fen;
        square_t from;
        squares_t destinations;
    };
    constexpr std::array cases{
        case_t{"k7/4P3/8/8/8/8/8/7K w - - 0 1", e7, {e8}},
        case_t{"k2r1r2/4P3/8/8/8/8/8/7K w - - 0 1", e7, {d8, e8, f8}},
        case_t{"7k/8/8/8/8/8/4p3/K7 b - - 0 1", e2, {e1}},
        case_t{"7k/8/8/8/8/8/4p3/K2R1R2 b - - 0 1", e2, {d1, e1, f1}},
        case_t{"k5b1/5P2/4K3/8/8/8/8/8 w - - 0 1", f7, {g8}},
        case_t{"8/8/8/8/8/4k3/5p2/K5B1 b - - 0 1", f2, {g1}},
        case_t{"k3b3/5P2/6K1/8/8/8/8/8 w - - 0 1", f7, {e8}},
        case_t{"8/8/8/8/8/6k1/5p2/K3B3 b - - 0 1", f2, {e1}}
    };
    constexpr std::array promotions{Q, R, B, N};
    bool passed = true;
    for (const auto& test : cases) {
        position_t position{test.fen};
        const position_t before = position;
        for (const bool filtered : {false, true}) {
            std::array<move_t, 256> buffer;
            const auto moves = filtered
                ? position.generate_tactical_moves(buffer)
                : position.generate_all_moves(buffer);
            size_t count{};
            for (size_t index = 0; index < moves.size(); ++index) {
                const move_t move = moves[index];
                if (move.from() != test.from)
                    continue;
                ++count;
                passed &= test_support::check(test.destinations[move.to()],
                                              "promotion destination");
                passed &= test_support::check(move.promotion() == promotions[(count - 1) % 4],
                                              "promotion order Q R B N");
                passed &= test_support::check(
                    std::ranges::count(moves, move) == 1, "promotion generated once");
                position.do_move(move);
                passed &= test_support::check(
                    position.at(move.to()) == piece_t(before.side(), move.promotion()),
                    "promotion places selected piece");
                position.undo_move(move);
                passed &= test_support::check(same_position(position, before),
                                              "SIMD promotion round trip");
            }
            passed &= test_support::check(count == 4 * test.destinations.size(),
                                          "four moves per promotion destination");
        }
    }
    return passed;
}

constexpr bool test_tactical_query() {
    struct case_t {
        std::string_view fen;
        move_t move;
        bool expected;
    };
    constexpr std::array cases{
        case_t{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {e2, e4}, false},
        case_t{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1", {g8, f6}, false},
        case_t{"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", {e1, g1}, false},
        case_t{"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", {e8, c8}, false},
        case_t{"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", {a1, a8}, true},
        case_t{"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", {h8, h1}, true},
        case_t{"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", {e5, d6}, true},
        case_t{"4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1", {e4, d3}, true},
        case_t{"4k3/8/8/3pPN2/8/8/8/4K3 w - d6 0 1", {f5, d6}, false},
        case_t{"4k3/8/8/8/3Ppn2/8/8/4K3 b - d3 0 1", {f4, d3}, false},
        case_t{"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", {e5, e6}, false}
    };
    for (const auto& test : cases)
        if (position_t{test.fen}.tactical(test.move) != test.expected)
            return false;

    constexpr position_t white{"k2r4/4P3/8/8/8/8/8/7K w - - 0 1"};
    constexpr position_t black{"7k/8/8/8/8/8/4p3/K2R4 b - - 0 1"};
    for (const type_t promotion : {Q, R, B, N})
        if (!white.tactical({e7, e8, promotion}) ||
            !white.tactical({e7, d8, promotion}) ||
            !black.tactical({e2, e1, promotion}) ||
            !black.tactical({e2, d1, promotion}))
            return false;

    position_t position{"4k3/3p4/8/4P3/8/8/8/4K3 b - - 0 1"};
    const move_t double_push{d7, d5};
    const move_t ep{e5, d6};
    position.do_move(double_push);
    if (!position.tactical(ep))
        return false;
    const move_t quiet{e1, f1};
    position.do_move(quiet);
    if (position.tactical(ep))
        return false;
    position.undo_move(quiet);
    return position.tactical(ep);
}

static_assert(test_tactical_query());

bool test_tactical_generation() {
    struct case_t {
        std::string_view fen;
        size_t expected;
    };
    constexpr std::array cases{
        case_t{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 0},
        case_t{"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", 2},
        case_t{"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", 2},
        case_t{"k7/4P3/8/8/8/8/8/7K w - - 0 1", 4},
        case_t{"k2r1r2/4P3/8/8/8/8/8/7K w - - 0 1", 12},
        case_t{"7k/8/8/8/8/8/4p3/K7 b - - 0 1", 4},
        case_t{"7k/8/8/8/8/8/4p3/K2R1R2 b - - 0 1", 12},
        case_t{"8/R3P2K/2Q2N2/8/8/8/8/1k6 w - - 0 1", 4},
        case_t{"1K6/8/8/8/8/2q2n2/r3p2k/8 b - - 0 1", 4},
        case_t{"k5b1/5P2/4K3/8/8/8/8/8 w - - 0 1", 4},
        case_t{"8/8/8/8/8/4k3/5p2/K5B1 b - - 0 1", 4},
        case_t{"k3r3/4P3/4K3/8/8/8/8/8 w - - 0 1", 0},
        case_t{"8/8/8/8/8/4k3/4p3/K3R3 b - - 0 1", 0},
        case_t{"r6K/4P3/8/8/8/8/8/k7 w - - 0 1", 4},
        case_t{"K7/8/8/8/8/8/4p3/R6k b - - 0 1", 4},
        case_t{"r6K/4P3/8/8/8/8/8/k6r w - - 0 1", 0},
        case_t{"K6R/8/8/8/8/8/4p3/R6k b - - 0 1", 0},
        case_t{"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", 1},
        case_t{"4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 1", 1},
        case_t{"4k3/8/8/3pPN2/8/8/8/4K3 w - d6 0 1", 1},
        case_t{"4k3/8/8/8/3Ppn2/8/8/4K3 b - d3 0 1", 1}
    };
    bool passed = true;
    for (const auto& test : cases) {
        const position_t position{test.fen};
        std::array<move_t, 256> all_buffer;
        std::array<move_t, 256> tactical_buffer;
        const auto all = position.generate_all_moves(all_buffer);
        const auto tactical = position.generate_tactical_moves(tactical_buffer);
        const auto is_capture = [&](const move_t move) {
            return position.by(~position.side())[move.to()] ||
                (position.at(move.from()).type() == P && position.en_passant()[move.to()]);
        };
        if (tactical.size() != test.expected)
            std::cerr << "Tactical position " << test.fen << ": expected "
                      << test.expected << ", actual " << tactical.size() << '\n';
        passed &= test_support::check(tactical.size() == test.expected,
                                      "exact tactical move count");
        size_t tactical_index{};
        for (const move_t move : all) {
            passed &= test_support::check(
                position.tactical(move) == (is_capture(move) || move.promotion() != NO_TYPE),
                "tactical query agrees with independent legal move classification");
            if (is_capture(move) || move.promotion() != NO_TYPE) {
                passed &= test_support::check(
                    tactical_index < tactical.size() && tactical[tactical_index] == move,
                    "tactical moves preserve legal move order and all underpromotions");
                ++tactical_index;
            }
        }
        passed &= test_support::check(tactical_index == tactical.size(),
                                      "no extra quiet moves or castling in tactical generation");
    }
    return passed;
}

bool test_pinned_pawn_generation() {
    struct case_t {
        std::string_view fen;
        square_t from;
        squares_t destinations;
        bool promotions{};
    };
    constexpr std::array cases{
        case_t{"k3r3/8/8/8/8/8/4P3/4K3 w - - 0 1", e2, {e3, e4}},
        case_t{"4k3/4p3/8/8/8/8/8/K3R3 b - - 0 1", e7, {e6, e5}},
        case_t{"k7/8/8/8/8/4r3/4P3/4K3 w - - 0 1", e2, {}},
        case_t{"k7/8/8/8/4r3/8/4P3/4K3 w - - 0 1", e2, {e3}},
        case_t{"k3r3/8/8/8/8/3p4/4P3/4K3 w - - 0 1", e2, {e3, e4}},
        case_t{"k7/8/8/8/8/8/4KP1r/8 w - - 0 1", f2, {}},
        case_t{"k7/8/8/8/8/5b2/4P3/3K4 w - - 0 1", e2, {f3}},
        case_t{"3k4/4p3/5B2/8/8/8/8/K7 b - - 0 1", e7, {f6}},
        case_t{"k7/8/8/7b/8/5P2/4K3/8 w - - 0 1", f3, {}},
        case_t{"k3r3/8/8/8/7b/8/4P3/4K3 w - - 0 1", e2, {}},
        case_t{"k3r3/8/8/8/8/5n2/4P3/4K3 w - - 0 1", e2, {}},
        case_t{"k7/8/8/8/8/5K2/6P1/7b w - - 0 1", g2, {}},
        case_t{"k5b1/5P2/4K3/8/8/8/8/8 w - - 0 1", f7, {g8}, true},
        case_t{"8/8/8/8/8/4k3/5p2/K5B1 b - - 0 1", f2, {g1}, true},
        case_t{"k3b3/5P2/6K1/8/8/8/8/8 w - - 0 1", f7, {e8}, true},
        case_t{"8/8/8/8/8/6k1/5p2/K3B3 b - - 0 1", f2, {e1}, true},
        case_t{"k3r3/4P3/4K3/8/8/8/8/8 w - - 0 1", e7, {}},
        case_t{"8/8/8/8/8/4k3/4p3/K3R3 b - - 0 1", e2, {}}
    };
    bool passed = true;
    for (const auto& test : cases) {
        position_t position{test.fen};
        const position_t before = position;
        passed &= test_support::check(
            std::get<1>(position.find_snipers_and_blockers(position.side()))[test.from],
            "test pawn is pinned");
        std::array<move_t, 256> buffer;
        const auto moves = position.generate_all_moves(buffer);
        size_t count{};
        for (const move_t move : moves) {
            if (move.from() != test.from)
                continue;
            ++count;
            passed &= test_support::check(test.destinations[move.to()],
                                          "pinned pawn stays on pin and evasion lines");
            passed &= test_support::check(
                test.promotions ? move.promotion() != NO_TYPE : move.promotion() == NO_TYPE,
                "pinned-pawn promotion encoding");
            position.do_move(move);
            const side_t side = ~position.side();
            passed &= test_support::check(
                (position.attackers(position.by(side, K).front()) & position.by(~side)).empty(),
                "pinned-pawn move preserves king safety");
            position.undo_move(move);
            passed &= test_support::check(same_position(position, before), "pinned-pawn round trip");
        }
        passed &= test_support::check(
            count == test.destinations.size() * (test.promotions ? 4 : 1),
            "exact pinned-pawn move count");
        for (const square_t to : test.destinations) {
            if (test.promotions) {
                for (const type_t promotion : {N, B, R, Q})
                    passed &= test_support::check(
                        std::ranges::count(moves, move_t{test.from, to, promotion}) == 1,
                        "each pinned-pawn promotion generated once");
            } else {
                passed &= test_support::check(
                    std::ranges::count(moves, move_t{test.from, to}) == 1,
                    "each pinned-pawn destination generated once");
            }
        }
    }
    return passed;
}

bool test_en_passant_generation() {
    struct case_t {
        std::string_view fen;
        square_t target;
        squares_t sources;
    };
    constexpr std::array cases{
        case_t{"4k3/8/8/2PpP3/8/8/8/4K3 w - d6 0 1", d6, {c5, e5}},
        case_t{"4k3/8/8/8/2pPp3/8/8/4K3 b - d3 0 1", d3, {c4, e4}},
        case_t{"4k3/8/8/Pp6/8/8/8/4K3 w - b6 0 1", b6, {a5}},
        case_t{"4k3/8/8/6pP/8/8/8/4K3 w - g6 0 1", g6, {h5}},
        case_t{"4k3/8/8/8/pP6/8/8/4K3 b - b3 0 1", b3, {a4}},
        case_t{"4k3/8/8/8/6Pp/8/8/4K3 b - g3 0 1", g3, {h4}},
        // Capturing a checking pawn does not land on its square.
        case_t{"k7/8/8/3pP3/4K3/8/8/8 w - d6 0 1", d6, {e5}},
        case_t{"8/8/8/4k3/3Pp3/8/8/K7 b - d3 0 1", d3, {e4}},
        // Blocking a bishop check and remaining on a diagonal pin line.
        case_t{"k7/5b2/8/4pP2/2K5/8/8/8 w - e6 0 1", e6, {f5}},
        case_t{"k7/6b1/8/4Pp2/3K4/8/8/8 w - f6 0 1", f6, {e5}},
        case_t{"K7/8/8/2k5/4Pp2/8/5B2/8 b - e3 0 1", e3, {f4}},
        case_t{"K7/8/8/3k4/4pP2/8/6B1/8 b - f3 0 1", f3, {e4}},
        // Removing both pawns exposes a horizontal rook attack.
        case_t{"k7/8/8/r4pPK/8/8/8/8 w - f6 0 1", f6, {}},
        case_t{"8/8/8/8/R4Ppk/8/8/7K b - f3 0 1", f3, {}},
        // Removing the captured pawn exposes a diagonal attack.
        case_t{"k7/5b2/8/3pP3/2K5/8/8/8 w - d6 0 1", d6, {}},
        case_t{"K7/8/8/2k5/3Pp3/8/5B2/8 b - d3 0 1", d3, {}},
        case_t{"k3r3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", d6, {}},
        case_t{"k7/8/8/3pP3/8/5n2/8/4K3 w - d6 0 1", d6, {}},
        case_t{"k7/8/8/3pP3/4K3/8/5n2/8 w - d6 0 1", d6, {}},
        case_t{"4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1", d6, {}},
        case_t{"4k3/8/8/3p4/8/8/8/4K3 w - d6 0 1", d6, {}}
    };
    bool passed = true;
    for (const auto& test : cases) {
        position_t position{test.fen};
        const position_t before = position;
        std::array<move_t, 256> buffer;
        const auto moves = position.generate_all_moves(buffer);
        squares_t sources{};
        size_t count{};
        for (const move_t move : moves) {
            if (move.to() != test.target || position.at(move.from()).type() != P ||
                move.from().file() == move.to().file())
                continue;
            sources.set(move.from());
            ++count;
            position.do_move(move);
            const side_t moving_side = ~position.side();
            passed &= test_support::check(
                (position.attackers(position.by(moving_side, K).front()) &
                 position.by(~moving_side)).empty(), "en passant preserves king safety");
            position.undo_move(move);
            passed &= test_support::check(same_position(position, before),
                                          "generated en-passant round trip");
        }
        if (sources != test.sources || count != test.sources.size())
            std::cerr << "En-passant generation mismatch: " << test.fen << '\n';
        passed &= test_support::check(sources == test.sources && count == test.sources.size(),
                                      "exact en-passant source set");
    }
    position_t position{"4k3/3p4/8/4P3/8/8/8/4K3 b - - 0 1"};
    const position_t before = position;
    const move_t push{d7, d5};
    const move_t take{e5, d6};
    std::array<move_t, 256> buffer;
    position.do_move(push);
    const auto moves = position.generate_all_moves(buffer);
    passed &= test_support::check(std::ranges::count(moves, take) == 1,
                                  "double push generates en-passant reply");
    const position_t after_push = position;
    const move_t decline{e1, f1};
    position.do_move(decline);
    passed &= test_support::check(position.en_passant().empty(),
                                  "declining en passant expires target");
    position.undo_move(decline);
    passed &= test_support::check(same_position(position, after_push),
                                  "undo restores en-passant availability");
    position.undo_move(push);
    passed &= test_support::check(same_position(position, before),
                                  "undo removes newly created target");
    return passed;
}
