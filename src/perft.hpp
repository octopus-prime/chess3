#pragma once

#include <cstdint>
#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <ranges>
#include <string_view>
#include "transposition.hpp"
#include "killer.hpp"

import types.side;
import types.type;
import types.square;
import types.score;
import core.move;
import core.position;
import attacks.generator;
import hashes.hash;

namespace chess {

namespace detail {

template <typename Projection>
auto ordered_moves(const std::span<move_t> moves, const move_t best_move, const std::span<const move_t, 2> killer_moves, Projection get_score) noexcept {
    const auto create = [moves, best_move, killer_moves, get_score, count = size_t{}](const int phase) mutable -> std::span<move_t> {
        const auto remaining = moves.subspan(count);
        switch (phase) {
            case 0: {
                const auto best = std::ranges::find(remaining, best_move);
                count += best != remaining.end();
                if (best != remaining.end())
                    std::iter_swap(remaining.begin(), best);
                return remaining.first(count);
            }
            case 1: {
                const auto tail = std::ranges::partition(remaining, [&](const move_t move) { return get_score(move) > 0; });
                const auto result = remaining.first(tail.begin() - remaining.begin());
                count += result.size();
                std::ranges::sort(result, std::greater<>(), get_score);
                return result;
            }
            case 2: {
                size_t found{};
                for (const move_t killer_move : killer_moves) {
                    const auto tail = remaining.subspan(found);
                    const auto best = std::ranges::find(tail, killer_move);
                    if (best != tail.end()) {
                        std::iter_swap(tail.begin(), best);
                        ++found;
                    }
                }
                count += found;
                return remaining.first(found);
            }
            default: {
                std::ranges::sort(remaining, std::greater<>(), get_score);
                count += remaining.size();
                return remaining;
            }
        }
    };
    return std::views::iota(0, 4) | std::views::transform(create) | std::views::join;
}

} // namespace detail

struct perft_t {
    explicit perft_t(const std::string_view fen) noexcept : position(fen) {}

    size_t operator()(size_t depth) noexcept {
        if (depth == 0)
            return 1;

        std::array<move_t, 256> buffer;
        std::span<move_t> moves = position.generate_all_moves(buffer);

        if (depth == 1)
            return moves.size();

        return std::ranges::fold_left(moves, size_t{}, [&](size_t acc, const move_t& move) {
            position.do_move(move);
            acc += (*this)(depth - 1);
            position.undo_move(move);
            return acc;
        });
    }

private:
    position_t position;
};

struct negamax_t {
    explicit negamax_t(const std::string_view fen) noexcept : position(fen), transposition(), killer(), nodes_searched{} {}

    score_t operator()(size_t depth) noexcept {
        return search(depth, 0, MIN, MAX);
    }

    size_t nodes() const noexcept {
        return nodes_searched;
    }

    size_t full() const noexcept {
        return transposition.full();
    }

    std::span<move_t> extract_pv(const std::span<move_t, 256> buffer, size_t max_length) noexcept {
        size_t length = 0;
        while (length < max_length) {
            const auto entry = transposition.get(position.hash(), length);
            if (!entry || entry->move == move_t{})
                break;

            buffer[length++] = entry->move;
            position.do_move(entry->move);
        }

        for (size_t i = length; i-- > 0;)
            position.undo_move(buffer[i]);

        return buffer.first(length);
    }

private:
    score_t search(size_t depth, size_t height, score_t alpha, score_t beta) noexcept {
        if (depth == 0)
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
        auto ordered = detail::ordered_moves(moves, best_move, killer.get(height), get_score);

        for (const move_t& move : ordered) {
            position.do_move(move);
            const score_t score = -search(depth - 1, height + 1, -beta, -alpha);
            position.undo_move(move);

            if (score >= beta) {
                transposition.put(position.hash(), move, beta, LOWER, depth, height);
                if (!position.tactical(move))
                    killer.put(move, height);
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

    score_t qsearch(size_t height, score_t alpha, score_t beta) noexcept {
        ++nodes_searched;

        const bool in_check = !position.checkers().empty();
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

    score_t evaluate() const noexcept {
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

    position_t position;
    transposition_t transposition;
    killer_t killer;
    size_t nodes_searched;
};

} // namespace chess
