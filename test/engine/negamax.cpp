// #include <array>
// #include <cstdint>
// #include <iostream>
// #include <limits>
// #include <type_traits>
// #include <algorithm>
// #include <ranges>
// #include <span>
// #include "test_support.hpp"

// import engine.negamax;
// import engine.ordering;
// import engine.history;
// import engine.killer;
// import engine.transposition;
// import core.position;
// import types.piece;
// import types.hash;
// import types.ply;

// bool test_tt_mate_scores() {
//     using namespace chess;
//     static_assert(score_to_tt(score_e(MAX - 12), 7) == MAX - 5);
//     static_assert(score_to_tt(score_e(MIN + 12), 7) == MIN + 5);
//     static_assert(score_from_tt(score_e(MAX - 5), 3) == MAX - 8);
//     static_assert(score_from_tt(score_e(MIN + 5), 3) == MIN + 8);
//     constexpr std::array unchanged{
//         score_t{DRAW}, score_t{PAWN}, score_t{score_e(-QUEEN)},
//         score_t{score_e(WIN - 1)}, score_t{score_e(LOSS + 1)},
//         score_t{IGNORE}, score_t{score_e(-IGNORE)}
//     };
//     static_assert([&] {
//         for (const score_t score : unchanged)
//             if (score_to_tt(score, 17) != score || score_from_tt(score, 17) != score)
//                 return false;
//         return true;
//     }());

//     transposition_t table{0};
//     const move_t move{e7, e8, Q};
//     bool passed = true;
//     for (const flag_e flag : {EXACT, LOWER, UPPER}) {
//         for (const ply_t stored_height : {ply_t{0}, ply_t{7}, std::numeric_limits<ply_t>::max()}) {
//             for (const ply_t probed_height : {ply_t{0}, ply_t{3}, std::numeric_limits<ply_t>::max()}) {
//                 for (const bool winning : {false, true}) {
//                     const score_t stored_score = winning
//                         ? score_e(MAX - 5 - static_cast<int>(stored_height))
//                         : score_e(MIN + 5 + static_cast<int>(stored_height));
//                     const score_t expected_score = winning
//                         ? score_e(MAX - 5 - static_cast<int>(probed_height))
//                         : score_e(MIN + 5 + static_cast<int>(probed_height));
//                     constexpr ply_t depth = std::numeric_limits<ply_t>::max();
//                     table.put(hash_t{1}, move, stored_score, flag, depth, stored_height);
//                     const auto entry = table.get(hash_t{1}, probed_height);
//                     static_assert(std::is_same_v<decltype(entry->depth), ply_t>);
//                     passed &= test_support::check(
//                         entry && entry->score == expected_score && entry->move == move &&
//                             entry->flag == flag && entry->depth == depth,
//                         "TT mate distance adjusts to probe ply for exact and bound entries");
//                     const auto same_ply = table.get(hash_t{1}, stored_height);
//                     passed &= test_support::check(same_ply && same_ply->score == stored_score,
//                                                   "TT mate score round trip");
//                 }
//             }
//         }
//         for (const score_t score : unchanged) {
//             table.put(hash_t{1}, move, score, flag, 9, 7);
//             const auto entry = table.get(hash_t{1}, 3);
//             passed &= test_support::check(entry && entry->score == score,
//                                           "TT preserves non-mate scores and ignore sentinels");
//         }
//     }
//     return passed;
// }

// bool test_lazy_move_ordering() {
//     using namespace chess;
//     const position_t position{};
//     const move_t low{a1, a2};
//     const move_t middle{b1, b2};
//     const move_t high{c1, c2};
//     const std::array<move_t, 2> no_killers{};
//     bool passed = true;
//     for (const move_t best : {low, middle, high, move_t{}}) {
//         std::array buffer{middle, low, high};
//         size_t projections{};
//         const auto get_score = [&](const move_t move) {
//             ++projections;
//             return uint16_t(move);
//         };
//         auto ordered = detail::ordered_moves(position, buffer, best, no_killers, get_score, get_score);
//         passed &= test_support::check(projections == 0, "move ordering is lazy at construction");
//         auto it = ordered.begin();
//         if (best != move_t{}) {
//             passed &= test_support::check(*it == best && projections == 0,
//                                           "legal TT move is first without sorting");
//             ++it;
//         }
//         passed &= test_support::check(projections > 0, "remaining moves sort only when reached");
//         std::array<move_t, 3> actual;
//         size_t count{};
//         if (best != move_t{})
//             actual[count++] = best;
//         for (; it != ordered.end(); ++it)
//             actual[count++] = *it;
//         std::array expected{high, middle, low};
//         if (best != move_t{}) {
//             const auto found = std::ranges::find(expected, best);
//             std::rotate(expected.begin(), found, found + 1);
//         }
//         passed &= test_support::check(actual == expected && count == 3,
//                                       "TT move and sorted remainder contain each move once");
//     }
//     std::array singleton{low};
//     size_t projections{};
//     const auto get_score = [&](const move_t move) {
//         ++projections;
//         return uint16_t(move);
//     };
//     auto one = detail::ordered_moves(position, singleton, low, no_killers, get_score, get_score);
//     passed &= test_support::check(std::ranges::distance(one) == 1 && projections == 0,
//                                   "singleton TT move needs no sorting");
//     auto empty = detail::ordered_moves(position, std::span<move_t>{}, low, no_killers,
//                                        get_score, get_score);
//     passed &= test_support::check(empty.begin() == empty.end(), "empty move ordering");
//     return passed;
// }

// bool test_phased_move_ordering() {
//     using namespace chess;
//     const position_t position{"4k3/8/8/8/8/ppp5/8/RRRRRR1K w - - 0 1"};
//     const std::array input{
//         move_t{a1, a3}, move_t{b1, b3}, move_t{c1, c3},
//         move_t{d1, d2}, move_t{e1, e2}, move_t{f1, f2}
//     };
//     constexpr std::array scores{-1, 0, 1, 99, 300, 900};
//     constexpr std::array history_scores{0, 0, 0, 900, 300, 99};
//     const auto get_score = [&](const move_t move) {
//         return scores[move.from().file()];
//     };
//     std::array<move_t, 7> candidates;
//     std::ranges::copy(input, candidates.begin());
//     candidates.back() = move_t{};
//     bool passed = true;
//     for (const move_t best : candidates) {
//         for (const move_t killer0 : candidates) {
//             for (const move_t killer1 : candidates) {
//                 auto buffer = input;
//                 const std::array killers{killer0, killer1};
//                 size_t projections{};
//                 const auto score = [&](const move_t move) {
//                     ++projections;
//                     return get_score(move);
//                 };
//                 const auto history_score = [&](const move_t move) {
//                     ++projections;
//                     return history_scores[move.from().file()];
//                 };
//                 auto ordered = detail::ordered_moves(position, buffer, best, killers, score, history_score);
//                 passed &= test_support::check(projections == 0, "five-phase ordering is lazy");
//                 std::array<move_t, 6> expected;
//                 size_t expected_count{};
//                 const auto append = [&](const move_t move) {
//                     if (std::ranges::find(input, move) != input.end() &&
//                         std::ranges::find(expected.begin(), expected.begin() + expected_count, move) ==
//                             expected.begin() + expected_count)
//                         expected[expected_count++] = move;
//                 };
//                 append(best);
//                 for (size_t index = input.size(); index-- > 0;)
//                     if (position.tactical(input[index]) && scores[index] >= 0)
//                         append(input[index]);
//                 append(killer0);
//                 append(killer1);
//                 for (size_t index = 0; index < input.size(); ++index)
//                     if (!position.tactical(input[index]))
//                         append(input[index]);
//                 for (size_t index = input.size(); index-- > 0;)
//                     append(input[index]);

//                 std::array<move_t, 6> actual;
//                 size_t count{};
//                 for (const move_t move : ordered) {
//                     if (count < actual.size()) {
//                         actual[count] = move;
//                         passed &= test_support::check(buffer[count] == move,
//                                                       "buffer prefix matches searched order at cutoff");
//                     }
//                     ++count;
//                 }
//                 passed &= test_support::check(count == input.size() && actual == expected,
//                                               "five-phase order emits each legal move exactly once");
//             }
//         }
//     }
//     const std::array killers{input[0], input[0]};
//     size_t projections{};
//     std::array singleton{input[0]};
//     const auto score = [&](const move_t move) {
//         ++projections;
//         return get_score(move);
//     };
//     auto one = detail::ordered_moves(position, singleton, input[0], killers, score, score);
//     auto it = one.begin();
//     passed &= test_support::check(*it == input[0] && projections == 0,
//                                   "TT cutoff avoids partitioning and sorting");
//     ++it;
//     passed &= test_support::check(it == one.end() && projections == 0,
//                                   "singleton with duplicate TT killers is emitted once");
//     auto empty = detail::ordered_moves(position, std::span<move_t>{}, input[0], killers,
//                                        get_score, get_score);
//     passed &= test_support::check(empty.begin() == empty.end(), "empty five-phase ordering");
//     return passed;
// }

// constexpr bool test_history_updates() {
//     using namespace chess;
//     history_t history;
//     if (history.get(WN, f3) != 0)
//         return false;
//     history.update(WN, f3, 100);
//     if (history.get(WN, f3) != 100 || history.get(BN, f3) != 0 ||
//         history.get(WB, f3) != 0 || history.get(WN, e3) != 0)
//         return false;
//     history.update(WN, f3, 100);
//     if (history.get(WN, f3) != 200)
//         return false;
//     history.update(WN, f3, -100);
//     if (history.get(WN, f3) != 99)
//         return false;
//     for (int iteration = 0; iteration < 1000; ++iteration) {
//         history.update(WN, f3, 2048);
//         history.update(BN, f6, -2048);
//         if (history.get(WN, f3) > history_t::LIMIT ||
//             history.get(BN, f6) < -history_t::LIMIT)
//             return false;
//     }
//     if (history.get(WN, f3) <= 0 || history.get(BN, f6) >= 0)
//         return false;
//     history.update(WN, f3, 1000000);
//     history.update(BN, f6, -1000000);
//     if (history.get(WN, f3) != history_t::LIMIT ||
//         history.get(BN, f6) != -history_t::LIMIT)
//         return false;
//     history.update(WN, f3, -history_t::LIMIT);
//     history.update(BN, f6, history_t::LIMIT);
//     return history.get(WN, f3) == -history_t::LIMIT &&
//         history.get(BN, f6) == history_t::LIMIT &&
//         history_t::bonus(0) == 0 && history_t::bonus(1) == 16 &&
//         history_t::bonus(2) == 64 && history_t::bonus(11) == 1936 &&
//         history_t::bonus(12) == 2048 &&
//         history_t::bonus(std::numeric_limits<ply_t>::max()) == 2048;
// }

// static_assert(test_history_updates());

// constexpr bool test_killer_heights() {
//     using namespace chess;
//     killer_t killer;
//     constexpr ply_t last = std::numeric_limits<ply_t>::max();
//     const move_t first{e2, e4};
//     const move_t second{d2, d4};
//     killer.put(first, last);
//     killer.put(second, last);
//     killer.put(second, last);
//     return killer.get(last)[0] == second && killer.get(last)[1] == first &&
//         killer.get(0)[0] == move_t{} && killer.get(last - 1)[0] == move_t{};
// }

// static_assert(test_killer_heights());
// static_assert(std::is_same_v<decltype(&chess::negamax_t::operator()),
//                             chess::score_t (chess::negamax_t::*)(chess::ply_t) noexcept>);
// static_assert(std::is_same_v<decltype(&chess::negamax_t::extract_pv),
//                             std::span<chess::move_t> (chess::negamax_t::*)(
//                                 std::span<chess::move_t, 256>, chess::ply_t) noexcept>);

// bool test_history_ordering() {
//     using namespace chess;
//     const position_t position{"4k3/P7/8/3pP3/8/8/8/4K1N1 w - d6 0 1"};
//     const move_t quiet_high{e1, f1};
//     const move_t quiet_low{g1, f3};
//     const move_t ep{e5, d6};
//     const move_t promotion{a7, a8, Q};
//     history_t history;
//     history.update(WK, f1, history_t::LIMIT);
//     history.update(WN, f3, -history_t::LIMIT);
//     history.update(WP, d6, history_t::LIMIT);
//     std::array buffer{quiet_low, ep, quiet_high, promotion};
//     const std::array<move_t, 2> no_killers{};
//     size_t history_reads{};
//     const auto capture_score = [&](const move_t move) {
//         return int(score_e(score_t(position.at(move.to()).type()) +
//                           score_t(move.promotion()) -
//                           score_t(position.at(move.from()).type()) / 100));
//     };
//     auto ordered = detail::ordered_moves(position, buffer, move_t{}, no_killers, capture_score,
//         [&](const move_t move) {
//             if (position.tactical(move))
//                 return capture_score(move);
//             ++history_reads;
//             return history.get(position.at(move.from()), move.to());
//         });
//     bool passed = test_support::check(history_reads == 0, "history ordering is lazy");
//     auto it = ordered.begin();
//     passed &= test_support::check(*it == promotion && history_reads == 0,
//                                   "positive history does not enter capture phase");
//     const std::array expected{promotion, quiet_high, quiet_low, ep};
//     size_t count{};
//     for (; it != ordered.end(); ++it) {
//         passed &= test_support::check(count < expected.size() && *it == expected[count],
//                                       "quiet history and tactical capture scores order remainder");
//         ++count;
//     }
//     passed &= test_support::check(count == expected.size() && history_reads > 0,
//                                   "history ordering visits every move once");
//     return passed;
// }

// bool test_search_draws() {
//     using namespace chess;
//     struct case_t {
//         std::string_view fen;
//         std::string_view description;
//     };
//     constexpr std::array cases{
//         case_t{"4k3/8/8/8/8/8/8/4K3 w - - 0 1", "bare kings draw"},
//         case_t{"4k3/8/8/8/8/8/8/2B1K3 w - - 0 1", "single bishop draw"},
//         case_t{"4k1n1/8/8/8/8/8/8/4K3 b - - 0 1", "single knight draw"},
//         case_t{"4k1n1/8/8/8/8/8/8/2B1K3 w - - 0 1",
//                "one minor per side draws under current material heuristic"},
//         case_t{"4k3/8/8/8/8/8/8/R3K3 w - - 100 1", "50-move draw for stronger side"},
//         case_t{"4k3/8/8/8/8/8/8/R3K3 b - - 100 1", "50-move draw for weaker side"},
//         case_t{"4k3/8/8/8/8/8/8/R3K3 w - - 101 1", "50-move draw above threshold"},
//         case_t{"k3R3/8/8/8/8/8/8/4K3 b - - 100 1",
//                "50-move draw in check with legal evasions"}
//     };
//     bool passed = true;
//     for (const auto& test : cases) {
//         negamax_t search{test.fen};
//         for (const ply_t depth : {ply_t{0}, ply_t{1}, ply_t{3}}) {
//             passed &= test_support::check(search(depth) == DRAW, test.description);
//             std::array<move_t, MAX_PLY> buffer;
//             passed &= test_support::check(search.extract_pv(buffer, 3).empty(),
//                                           "immediate draw does not create a TT principal variation");
//         }
//     }

//     for (const auto fen : {"4k3/8/8/8/8/8/8/R3K3 w - - 99 1",
//                           "4k3/8/8/8/8/8/8/R3K3 b - - 99 1"}) {
//         negamax_t search{fen};
//         passed &= test_support::check(search(1) == DRAW,
//                                       "quiet child reaches 100 halfmoves in quiescence");
//         passed &= test_support::check(search.nodes() > 1,
//                                       "99 halfmoves is not an immediate root draw");
//         passed &= test_support::check(search(2) == DRAW,
//                                       "quiet child reaches 100 halfmoves in full search");
//     }

//     negamax_t stalemate{"k7/8/1QK5/8/8/8/8/8 b - - 0 1"};
//     passed &= test_support::check(stalemate(1) == DRAW && stalemate(3) == DRAW,
//                                   "stalemate is a draw despite material advantage");

//     negamax_t mate_at_threshold{"k7/1Q6/2K5/8/8/8/8/8 b - - 100 1"};
//     for (const ply_t depth : {ply_t{0}, ply_t{1}, ply_t{3}})
//         passed &= test_support::check(mate_at_threshold(depth) == MIN,
//                                       "checkmate takes precedence over 50-move draw");

//     negamax_t mate_on_last_halfmove{"k7/8/1QK5/8/8/8/8/8 w - - 99 1"};
//     passed &= test_support::check(mate_on_last_halfmove(1) == MAX - 1,
//                                   "mating move at 100 halfmoves wins instead of drawing");
//     return passed;
// }

// bool run_mate_tests() {
//     using namespace chess;
//     struct case_t {
//         std::string_view fen;
//         ply_t depth;
//         std::string_view description;
//     };
//     constexpr std::array cases{
//         case_t{"1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - -", 5,
//                "mate in 3 at depth 5"},
//         case_t{"4r1k1/pp4pp/8/q1p1N3/5P2/1P4P1/P6P/3Q2K1 w - -", 9,
//                "mate in 5 at depth 9"},
//         case_t{"n1N3br/2p1Bpkr/1pP2R1b/pP1p1PpR/Pp4P1/1P6/1K1P4/8 w - -", 11,
//                "mate in 6 at depth 11"},
//         case_t{"r3k3/1pp1p1Kp/1p2P3/1p6/2N5/P2P2pr/5B1p/3Q1R2 w q -", 15,
//                "mate in 8 at depth 15"}
//     };
//     bool passed = true;
//     for (const auto& test : cases) {
//         negamax_t search{test.fen};
//         score_t score{};
//         for (int depth = 1; depth <= test.depth; ++depth)
//             score = search(static_cast<ply_t>(depth));
//         const score_t expected{score_e(MAX - test.depth)};
//         if (score != expected)
//             std::cerr << test.description << ": expected " << int(score_e(expected))
//                       << ", got " << int(score_e(score)) << '\n';
//         passed &= test_support::check(score == expected, test.description);
//         std::array<move_t, MAX_PLY> pv_buffer;
//         const auto pv = search.extract_pv(pv_buffer, test.depth);
//         position_t position{test.fen};
//         std::array<move_t, 256> move_buffer;
//         for (const move_t move : pv) {
//             const auto legal = position.generate_all_moves(move_buffer);
//             const bool valid = std::ranges::find(legal, move) != legal.end();
//             passed &= test_support::check(valid, "mate PV contains only legal moves");
//             if (!valid)
//                 break;
//             position.do_move(move);
//         }
//         passed &= test_support::check(!pv.empty(), "mate search produces a principal variation");
//         passed &= test_support::check(search(test.depth) == expected,
//                                       "mate PV extraction restores the search position");
//     }
//     return passed;
// }

// bool run_negamax_tests() {
//     bool passed = test_tt_mate_scores();
//     passed &= test_lazy_move_ordering();
//     passed &= test_phased_move_ordering();
//     passed &= test_support::check(test_history_updates(), "piece-to history gravity updates");
//     passed &= test_support::check(test_killer_heights(), "killers support the maximum ply height");
//     passed &= test_history_ordering();
//     passed &= test_search_draws();
//     for (const auto fen : {"k7/4P3/8/8/8/8/8/7K w - - 0 1",
//                           "7k/8/8/8/8/8/4p3/K7 b - - 0 1"}) {
//         chess::negamax_t search{fen};
//         const auto score = search(0);
//         passed &= test_support::check(score > chess::PAWN,
//                                       "quiescence searches quiet promotions");
//         passed &= test_support::check(search.nodes() > 1,
//                                       "quiescence expands promotion moves");
//         passed &= test_support::check(search(0) == score,
//                                       "quiescence restores the position");
//     }
//     chess::negamax_t mate{"k7/1Q6/2K5/8/8/8/8/8 b - - 0 1"};
//     passed &= test_support::check(mate(0) == chess::MIN,
//                                   "quiescence detects checkmate without stand pat");
//     passed &= test_support::check(mate(std::numeric_limits<chess::ply_t>::max()) == chess::MIN,
//                                   "search accepts the maximum ply depth");
//     chess::negamax_t mate_in_one{"k7/8/1QK5/8/8/8/8/8 w - - 0 1"};
//     passed &= test_support::check(mate_in_one(1) == chess::MAX - 1,
//                                   "search scores mate in one at the correct distance");
//     std::array<chess::move_t, 256> pv_buffer;
//     passed &= test_support::check(mate_in_one.extract_pv(pv_buffer, 0).empty(),
//                                   "zero-length PV extraction leaves the position unchanged");
//     const auto pv = mate_in_one.extract_pv(pv_buffer, std::numeric_limits<chess::ply_t>::max());
//     passed &= test_support::check(pv.size() == 1,
//                                   "PV extraction accepts the maximum ply length");
//     passed &= test_support::check(mate_in_one(1) == chess::MAX - 1,
//                                   "PV extraction restores the root position");
//     chess::negamax_t evasion{"k3r3/8/8/8/8/8/8/4K3 w - - 0 1"};
//     const auto evasion_score = evasion(0);
//     passed &= test_support::check(evasion.nodes() > 1 && evasion_score > chess::LOSS,
//                                   "quiescence searches quiet check evasions");
//     passed &= test_support::check(evasion(0) == evasion_score,
//                                   "quiescence restores the position after evasions");

//     return passed;
// }
