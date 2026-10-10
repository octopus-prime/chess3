#pragma once

#include <cstdint>
#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>

import types.side;
import types.type;
import types.square;
import core.move;
import core.position;
import attacks.generator;

namespace chess {

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
    enum score_e : int16_t {
        MAX = 32000,
        KING = 20000,
        QUEEN = 900,
        ROOK = 500,
        BISHOP = 330,
        KNIGHT = 320,
        PAWN = 100,
        DRAW = 0,
        MIN = -32000
    };

    static constexpr std::array<score_e, MAX_TYPE> scores = {
        DRAW,
        PAWN,
        KNIGHT,
        BISHOP,
        ROOK,
        QUEEN,
        KING,
        DRAW
    };

    using score_t = int16_t;

    explicit negamax_t(const std::string_view fen) noexcept : position(fen), nodes_searched{} {}

    score_t operator()(size_t depth) noexcept {
        return search(depth, MIN, MAX);
    }

    size_t nodes() const noexcept {
        return nodes_searched;
    }

    void reset() noexcept {
        nodes_searched = 0;
    }

private:
    score_t search(size_t depth, score_t alpha, score_t beta) noexcept {
        if (depth == 0)
            return qsearch(alpha, beta);

        ++nodes_searched;

        std::array<move_t, 256> buffer;
        const std::span<move_t> moves = position.generate_all_moves(buffer);
        const auto get_score = [&](const move_t& move) {
            const score_t win = scores[position.at(move.to()).type()] + scores[move.promotion()];
            const score_t loss = scores[position.at(move.from()).type()] / 100;
            return win - loss;
        };
        std::ranges::sort(moves, std::greater<>(), get_score);

        for (const move_t& move : moves) {
            position.do_move(move);
            const score_t score = -search(depth - 1, -beta, -alpha);
            position.undo_move(move);

            if (score >= beta)
                return score;
            if (score > alpha)
                alpha = score;
        }

        return alpha;
    }

    score_t qsearch(score_t alpha, score_t beta) noexcept {
        ++nodes_searched;

        const score_t stand_pat = evaluate();

        if (stand_pat >= beta)
            return beta;
        if (stand_pat > alpha)
            alpha = stand_pat;

        std::array<move_t, 256> buffer;
        const std::span<move_t> moves = position.generate_capture_moves(buffer);
        const auto get_score = [&](const move_t& move) {
            const score_t win = scores[position.at(move.to()).type()] + scores[move.promotion()];
            const score_t loss = scores[position.at(move.from()).type()] / 100;
            return win - loss;
        };
        std::ranges::sort(moves, std::greater<>(), get_score);

        for (const move_t& move : moves) {
            position.do_move(move);
            const score_t score = -qsearch(-beta, -alpha);
            position.undo_move(move);

            if (score >= beta)
                return score;
            if (score > alpha)
                alpha = score;
        }

        return alpha;
    }

    position_t position;
    size_t nodes_searched = 0;

    score_t evaluate() const noexcept {
        using namespace attacks::generator;
        const side_t side = position.side();
        score_t score = 0;
        // score += KING * (position.by(side, K).empty() - position.by(~side, K).empty());
        score += QUEEN * (position.by(side, Q).size() - position.by(~side, Q).size());
        score += ROOK * (position.by(side, R).size() - position.by(~side, R).size());
        score += BISHOP * (position.by(side, B).size() - position.by(~side, B).size());
        score += KNIGHT * (position.by(side, N).size() - position.by(~side, N).size());
        score += PAWN * (position.by(side, P).size() - position.by(~side, P).size());

        score += leapers(position.by(side, K), position.by(side, N)).size();
        score -= leapers(position.by(~side, K), position.by(~side, N)).size();

        score += sliders(position.by(side, R, Q), position.by(side, B, Q), position.by()).size();
        score -= sliders(position.by(~side, R, Q), position.by(~side, B, Q), position.by()).size();

        score += pawns(position.by(side, P), side).size();
        score -= pawns(position.by(~side, P), ~side).size();

        return score_t(score);
    }

};

} // namespace chess
