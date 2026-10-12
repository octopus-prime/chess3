module;

#include <cstddef>
#include <span>
#include <string_view>

export module engine.negamax;

export import core.move;
export import types.square;
export import types.score;
import core.position;
import engine.transposition;
import engine.killer;
import engine.history;

export namespace chess {

struct negamax_t {
    explicit negamax_t(const std::string_view fen) noexcept;

    score_t operator()(size_t depth) noexcept;

    size_t nodes() const noexcept;

    size_t full() const noexcept;

    std::span<move_t> extract_pv(const std::span<move_t, 256> buffer, size_t max_length) noexcept;

private:
    score_t search(size_t depth, size_t height, score_t alpha, score_t beta) noexcept;

    score_t qsearch(size_t height, score_t alpha, score_t beta) noexcept;

    score_t evaluate() const noexcept;

    position_t position;
    transposition_t transposition;
    killer_t killer;
    history_t history;
    size_t nodes_searched;
};

} // namespace chess
