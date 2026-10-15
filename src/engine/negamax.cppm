module;

#include <cstddef>
#include <span>
#include <string_view>

export module engine.negamax;

export import core.move;
export import types.square;
export import types.score;
export import types.ply;
import core.position;
import engine.transposition;
import engine.killer;
import engine.history;
import engine.evaluator;

export namespace chess {

struct negamax_t final {
    explicit negamax_t(const std::string_view fen) noexcept;
    score_t operator()(ply_t depth) noexcept;
    size_t nodes() const noexcept;
    size_t full() const noexcept;
    std::span<move_t> extract_pv(const std::span<move_t, MAX_PLY> buffer, ply_t max_length) noexcept;

private:
    score_t asearch(const score_t eval, const ply_t depth) noexcept;
    template <bool PV>
    score_t search(ply_t depth, ply_t height, score_t alpha, score_t beta) noexcept;
    template <bool PV>
    score_t qsearch(ply_t height, score_t alpha, score_t beta) noexcept;
    score_t evaluate() noexcept;

    position_t position;
    transposition_t transposition;
    killer_t killer;
    history_t history;
    evaluator_t evaluator;
    size_t nodes_searched;
};

} // namespace chess
