module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <span>
#include <string_view>
#include <ranges>

module engine.negamax;

import core.attacks.generator;
import engine.ordering;
import types.side;
import types.type;

namespace chess {

negamax_t::negamax_t(const std::string_view fen) noexcept : position(fen), transposition(96), killer(), history(), nodes_searched{} {}

score_t negamax_t::operator()(ply_t depth) noexcept {
    return search(depth, 0, MIN, MAX);
}

size_t negamax_t::nodes() const noexcept {
    return nodes_searched;
}

size_t negamax_t::full() const noexcept {
    return transposition.full();
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

score_t negamax_t::search(ply_t depth, ply_t height, score_t alpha, score_t beta) noexcept {
    if (depth == 0 || height == MAX_PLY - 1)
        return qsearch(height, alpha, beta);

    ++nodes_searched;

    move_t best_move{};
    if (const auto entry = transposition.get(position.hash(), height)) {
        best_move = entry->move;
        if (entry->depth >= depth) {
            switch (entry->flag) {
            case EXACT:
                return entry->score;
            case LOWER:
                if (entry->score > alpha)
                    alpha = entry->score;
                break;
            case UPPER:
                if (entry->score < beta)
                    beta = entry->score;
                break;
            default:
                break;
            }
            if (alpha >= beta)
                return beta;
        }
    }
    const score_t original_alpha = alpha;

    std::array<move_t, 256> buffer;
    const std::span<move_t> moves = position.generate_all_moves(buffer);

    if (moves.empty())
        return position.checkers() ? score_e(MIN + height) : DRAW;

    const auto get_score = [&](const move_t& move) {
        const score_t win = score_t(position.at(move.to()).type()) + score_t(move.promotion());
        const score_t loss = score_t(position.at(move.from()).type()) / 100;
        return win - loss;
    };
    const auto get_history_score = [&](const move_t move) {
        return history.get(position.at(move.from()), move.to());
    };
    auto ordered = detail::ordered_moves(position, moves, best_move, killer.get(height), get_score, get_history_score);

    for (const move_t& move : ordered) {
        position.do_move(move);
        score_t score;
        if (alpha > original_alpha) {
            score = -search(depth - 1, height + 1, score_e(-alpha - 1), -alpha);
            if (score > alpha)
                score = -search(depth - 1, height + 1, -beta, -alpha);
        } else {
            score = -search(depth - 1, height + 1, -beta, -alpha);
        }
        position.undo_move(move);

        if (score >= beta) {
            transposition.put(position.hash(), move, beta, LOWER, depth, height);
            if (!position.tactical(move)) {
                killer.put(move, height);
                const int bonus = history_t::bonus(depth);
                history.update(position.at(move.from()), move.to(), bonus);
                for (const move_t previous : moves) {
                    if (previous == move)
                        break;
                    if (!position.tactical(previous))
                        history.update(position.at(previous.from()), previous.to(), -bonus);
                }
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

score_t negamax_t::qsearch(ply_t height, score_t alpha, score_t beta) noexcept {
    ++nodes_searched;

    const bool in_check = !position.checkers().empty();

    if (height == MAX_PLY - 1)
        return in_check ? evaluate() : alpha;

    if (!in_check) {
        const score_t stand_pat = evaluate();
        if (stand_pat >= beta)
            return beta;
        if (stand_pat > alpha)
            alpha = stand_pat;
    }

    std::array<move_t, 256> buffer;
    const std::span<move_t> moves = in_check
        ? position.generate_all_moves(buffer)
        : position.generate_tactical_moves(buffer);

    if (in_check && moves.empty())
        return score_e(MIN + height);

    const auto get_score = [&](const move_t& move) {
        const score_t win = score_t(position.at(move.to()).type()) + score_t(move.promotion());
        const score_t loss = score_t(position.at(move.from()).type()) / 100;
        return win - loss;
    };
    std::ranges::sort(moves, std::greater<>(), get_score);

    for (const move_t& move : moves) {
        position.do_move(move);
        const score_t score = -qsearch(height + 1, -beta, -alpha);
        position.undo_move(move);

        if (score >= beta)
            return score;
        if (score > alpha)
            alpha = score;
    }

    return alpha;
}

score_t negamax_t::evaluate() const noexcept {
    using namespace attacks::generator;
    const side_t side = position.side();
    score_t score{};
    // score += KING * (position.by(side, K).empty() - position.by(~side, K).empty());
    score += score_t{QUEEN} * (int(position.by(side, Q).size()) - int(position.by(~side, Q).size()));
    score += score_t{ROOK} * (int(position.by(side, R).size()) - int(position.by(~side, R).size()));
    score += score_t{BISHOP} * (int(position.by(side, B).size()) - int(position.by(~side, B).size()));
    score += score_t{KNIGHT} * (int(position.by(side, N).size()) - int(position.by(~side, N).size()));
    score += score_t{PAWN} * (int(position.by(side, P).size()) - int(position.by(~side, P).size()));

    score += score_t{CP} * int(leapers(position.by(side, K), position.by(side, N)).size());
    score -= score_t{CP} * int(leapers(position.by(~side, K), position.by(~side, N)).size());

    score += score_t{CP} * int(sliders(position.by(side, R, Q), position.by(side, B, Q), position.by()).size());
    score -= score_t{CP} * int(sliders(position.by(~side, R, Q), position.by(~side, B, Q), position.by()).size());

    score += score_t{CP} * int(pawns(position.by(side, P), side).size());
    score -= score_t{CP} * int(pawns(position.by(~side, P), ~side).size());

    return score_t(score);
}

} // namespace chess
