module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <ranges>
#include <cmath>
#include <iostream>
#include <chrono>

module engine.negamax;

import engine.formatting;
import core.attacks.generator;
import engine.ordering;
import types.side;
import types.type;

namespace chess {

negamax_t::negamax_t(const std::string_view fen) noexcept : position(fen), transposition(), killer(), history(), nodes_searched{}, end_time{} {}

void negamax_t::setup(const std::string_view fen, const std::string_view moves) noexcept {
    position.setup(fen);
    for (auto&& part : std::views::split(moves, ' ')) {
        const std::string_view move{part};
        position.do_move(move);
    }
}

void negamax_t::reset() noexcept {
    position.setup("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    clear();
}

void negamax_t::clear() noexcept {
    transposition.clear();
    killer.clear();
    history.clear();
    nodes_searched = 0;
}

size_t negamax_t::nodes() const noexcept {
    return nodes_searched;
}

size_t negamax_t::full() const noexcept {
    return transposition.full();
}

side_t negamax_t::side() const noexcept {
    return position.side();
}

std::span<move_t> negamax_t::extract_pv(const std::span<move_t, MAX_PLY> buffer, ply_t max_length) noexcept {
    ply_t length = 0;
    while (length < max_length) {
        const auto entry = transposition.get(position.hash(), length);
        if (!entry || entry->move == move_t{})
            break;

        buffer[length++] = entry->move;
        position.do_move(entry->move);
    }

    for (ply_t i = length; i > 0; --i)
        position.undo_move(buffer[i - 1]);

    return buffer.first(length);
}

score_t negamax_t::operator()(const ply_t max_depth, const std::chrono::steady_clock::duration duration) noexcept {
    using namespace std::chrono;
    std::array<chess::move_t, 256> buffer;
    char output[2048];
    const auto start = steady_clock::now();
    nodes_searched = 0;
    end_time = start + duration;
    score_t score = qsearch<true>(0, MIN, MAX);
    if (score.ignore())
        score = evaluate();
    move_t best_move{};
    const auto legal = position.generate_all_moves(buffer);
    if (!legal.empty())
        best_move = legal.front();
    for (unsigned depth = 1; depth <= max_depth; ++depth) {
        if (steady_clock::now() >= end_time)
            break;
        const score_t next = asearch(score, static_cast<ply_t>(depth));
        if (next.ignore())
            break;
        score = next;
        const auto end = steady_clock::now();
        const auto ns = std::max(duration_cast<nanoseconds>(end - start).count(), int64_t{1});
        const auto ms = duration_cast<milliseconds>(end - start).count();
        const auto pv = extract_pv(buffer, depth);
        const auto nodes = this->nodes();
        const auto full = this->full();
        const char* out = std::format_to(output, "info depth {} seldepth {} score {} nodes {} nps {} hashfull {} time {} pv {}\n",
            depth, 0, score, nodes, size_t(1e9 * nodes / ns), full, ms, pv);
        std::fwrite(output, sizeof(char), size_t(out - output), stdout);
        std::fflush(stdout);
        if (!pv.empty())
            best_move = pv.front();
    }
    const char* out_best = std::format_to(output, "bestmove {}\n", best_move);
    std::fwrite(output, sizeof(char), size_t(out_best - output), stdout);
    std::fflush(stdout);
    return score;
}

score_t negamax_t::asearch(const score_t eval, const ply_t depth) noexcept {
    int delta = 25 + 2 * depth;
    int alpha = depth < 5 || eval.decisive() ? MIN : std::max(int{MIN}, int(eval) - delta);
    int beta = depth < 5 || eval.decisive() ? MAX : std::min(int{MAX}, int(eval) + delta);
    for (;;) {
        const score_t score = search<true>(depth, 0, score_e(alpha), score_e(beta));
        if (score.ignore())
            return IGNORE;
        if (score > alpha && score < beta)
            return score;
        if (alpha == MIN && beta == MAX)
            return score;
        delta = std::min(delta * 2, int{MAX} * 2);
        if (score <= alpha)
            alpha = std::max(int{MIN}, int(score) - delta);
        else
            beta = std::min(int{MAX}, int(score) + delta);
    }
}

template <bool PV>  
score_t negamax_t::search(ply_t depth, ply_t height, score_t alpha, score_t beta) noexcept {
    if ((nodes_searched & 1023) == 0 && std::chrono::steady_clock::now() >= end_time)
        return IGNORE;

    if (depth == 0 || height == MAX_PLY - 1)
        return qsearch<PV>(height, alpha, beta);

    ++nodes_searched;

    const bool in_check = position.check();

    if (position.threefold_repetition() || position.fifty_move_rule() || position.insufficient_material()) {
        if (in_check) {
            std::array<move_t, 256> buffer;
            if (position.generate_all_moves(buffer).empty())
                return score_e(MIN + height);
        }
        return DRAW;
    }

    if (height > 0) {
        alpha = std::max(alpha, score_t{score_e(MIN + height)});
        beta = std::min(beta, score_t{score_e(MAX - height - 1)});
        if (alpha >= beta)
            return alpha;
    }

    move_t best_move{};
    if (const auto entry = transposition.get(position.hash(), height)) {
        best_move = entry->move;
        if (entry->depth >= depth) {
            if (entry->flag == EXACT)
                return entry->score;
            if (!PV && ((entry->flag == LOWER && entry->score >= beta) ||
                        (entry->flag == UPPER && entry->score <= alpha)))
                return entry->score;
        }
    }
    const score_t original_alpha = alpha;

    std::array<move_t, 256> buffer;
    const std::span<move_t> moves = position.generate_all_moves(buffer);

    if (moves.empty())
        return in_check ? score_e(MIN + height) : DRAW;

    const score_t static_eval = in_check ? score_t{DRAW} : evaluate();
    if (!PV && !in_check && depth <= 7 && !beta.decisive() &&
        static_eval - 80 * depth >= beta)
        return static_eval;

    // Null Move Pruning
    {
        if (!PV && depth >= 3 && !in_check && !beta.decisive() && position.last_move() != move_t{} && !(position.by(position.side()) & ~position.by(position.side(), P, K)).empty() && moves.size() > 8 && static_eval >= beta) {
            const int R = std::min(int{depth} - 1, 2 + depth / 3 +
                std::min((int(static_eval) - int(beta)) / 200, 3));
            position.do_null_move();
            const score_t score = -search<false>(depth - 1 - R, height + 1, score_e(-beta), score_e(-beta + 1));
            position.undo_null_move();
            if (score.ignore())
                return IGNORE;
            if (score >= beta)
                return beta;
        }
    }

    const auto killers = killer.get(height);
    auto ordered = detail::ordered_moves(position, moves, best_move, killers, history);

    size_t move_count = 0;
    size_t quiet_count = 0;
    std::array<move_t, 256> searched_quiets;
    size_t quiets_searched = 0;
    for (const move_t& move : ordered) {
        const bool is_tactical = position.tactical(move);
        const int history_score = history.get(position.at(move.from()), move.to());
        const bool advanced_pawn = position.at(move.from()).type() == P &&
            (position.side() == WHITE ? move.to().rank() >= _6 : move.to().rank() <= _3);
        if (!is_tactical)
            ++quiet_count;
        const bool prune = !PV && !in_check && move_count > 0 &&
            !alpha.decisive() && !advanced_pawn && move != killers[0] && move != killers[1];
        if (prune && !is_tactical && depth <= 5 &&
            (quiet_count > 3 + depth * depth ||
             (depth <= 3 && static_eval + 100 + 120 * depth <= alpha)) &&
            !position.check(move))
            continue;
        if (prune && is_tactical && depth <= 4 && move.promotion() == NO_TYPE &&
            position.see(move) < -100 * depth && !position.check(move))
            continue;
        ++move_count;
        if (!is_tactical)
            searched_quiets[quiets_searched++] = move;
        position.do_move(move);
        const bool gives_check = position.check();
        score_t score;
        if (move_count > 1) {
            int reduction = 0;
            if (depth >= 3 && move_count > (PV ? 3 : 2) && !is_tactical &&
                !in_check && !gives_check && !advanced_pawn && !alpha.decisive() &&
                move != killers[0] && move != killers[1]) {
                reduction = int(0.75f + std::log(float(depth)) * std::log(float(move_count)) / 2.25f);
                reduction += !PV;
                reduction -= history_score / 8192;
                reduction = std::clamp(reduction, 1, int{depth} - 2);
            }
            score = -search<false>(depth - 1 - reduction, height + 1, score_e(-alpha - 1), -alpha);
            if (reduction > 0 && score > alpha && !score.ignore())
                score = -search<false>(depth - 1, height + 1, score_e(-alpha - 1), -alpha);
            if (PV && score > alpha && score < beta && !score.ignore())
                score = -search<true>(depth - 1, height + 1, -beta, -alpha);
        } else {
            score = -search<PV>(depth - 1, height + 1, -beta, -alpha);
        }
        position.undo_move(move);

        if (score.ignore())
            return IGNORE;

        if (score >= beta) {
            transposition.put(position.hash(), move, beta, LOWER, depth, height);
            if (!position.tactical(move)) {
                killer.put(move, height);
                const int bonus = history_t::bonus(depth);
                history.update(position.at(move.from()), move.to(), bonus);
                for (const move_t previous : std::span{searched_quiets}.first(quiets_searched - 1))
                    history.update(position.at(previous.from()), previous.to(), -bonus);
            }
            return beta;
        }
        if (score > alpha) {
            best_move = move;
            alpha = score;
        }
    }

    if (alpha > original_alpha) {
        transposition.put(position.hash(), best_move, alpha, EXACT, depth, height);
    } else {
        transposition.put(position.hash(), best_move, alpha, UPPER, depth, height);
    }

    return alpha;
}

template <bool PV>
score_t negamax_t::qsearch(ply_t height, score_t alpha, score_t beta) noexcept {
    if ((nodes_searched & 1023) == 0 && std::chrono::steady_clock::now() >= end_time)
        return IGNORE;
    ++nodes_searched;

    const bool in_check = position.check();

    if (height == MAX_PLY - 1)
        return evaluate();

    if (position.threefold_repetition() || position.fifty_move_rule() || position.insufficient_material()) {
        if (in_check) {
            std::array<move_t, 256> buffer;
            if (position.generate_all_moves(buffer).empty())
                return score_e(MIN + height);
        }
        return DRAW;
    }

    score_t stand_pat{DRAW};
    if (!in_check) {
        stand_pat = evaluate();
        if (stand_pat >= beta)
            return beta;
        if (stand_pat > alpha)
            alpha = stand_pat;
    }

    std::array<move_t, 256> buffer;
    std::span<move_t> moves = in_check
        ? position.generate_all_moves(buffer)
        : position.generate_tactical_moves(buffer);

    if (in_check && moves.empty())
        return score_e(MIN + height);

    if (!in_check) {
        auto tail = std::ranges::partition(moves, [&](const move_t& move) {
            if (move.promotion() != NO_TYPE)
                return true;
            const int gain = position.at(move.to()).type() == NO_TYPE
                ? PAWN : int(score_t{position.at(move.to()).type()});
            if ((!alpha.decisive() && stand_pat + gain + 150 < alpha) || position.see(move) < 0)
                return position.check(move);
            return true;
        });
        moves = moves.first(tail.begin() - moves.begin());
    }

    std::ranges::sort(moves, std::greater<>(), [&](const move_t& move) {
        return 16 * int(score_t{position.at(move.to()).type()}) +
            int(score_t{move.promotion()}) - int(score_t{position.at(move.from()).type()});
    });

    for (const move_t& move : moves) {
        position.do_move(move);
        const score_t score = -qsearch<PV>(height + 1, -beta, -alpha);
        position.undo_move(move);

        if (score.ignore())
            return IGNORE;
        if (score >= beta)
            return score;
        if (score > alpha)
            alpha = score;
    }

    return alpha;
}

score_t negamax_t::evaluate() noexcept {
    return evaluator.evaluate(position);
    // using namespace attacks::generator;
    // const side_t side = position.side();
    // score_t score{};
    // // score += KING * (position.by(side, K).empty() - position.by(~side, K).empty());
    // score += score_t{QUEEN} * (int(position.by(side, Q).size()) - int(position.by(~side, Q).size()));
    // score += score_t{ROOK} * (int(position.by(side, R).size()) - int(position.by(~side, R).size()));
    // score += score_t{BISHOP} * (int(position.by(side, B).size()) - int(position.by(~side, B).size()));
    // score += score_t{KNIGHT} * (int(position.by(side, N).size()) - int(position.by(~side, N).size()));
    // score += score_t{PAWN} * (int(position.by(side, P).size()) - int(position.by(~side, P).size()));

    // score += score_t{CP} * int(leapers(position.by(side, K), position.by(side, N)).size());
    // score -= score_t{CP} * int(leapers(position.by(~side, K), position.by(~side, N)).size());

    // score += score_t{CP} * int(sliders(position.by(side, R, Q), position.by(side, B, Q), position.by()).size());
    // score -= score_t{CP} * int(sliders(position.by(~side, R, Q), position.by(~side, B, Q), position.by()).size());

    // score += score_t{CP} * int(pawns(position.by(side, P), side).size());
    // score -= score_t{CP} * int(pawns(position.by(~side, P), ~side).size());

    // return score_t(score);
}

} // namespace chess
