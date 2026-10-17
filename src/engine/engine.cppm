module;

#include <cstddef>
#include <span>
#include <string_view>
#include <chrono>

export module engine.engine;

export import core.move;
export import types.square;
export import types.score;
export import types.ply;
export import types.side;
import core.position;
import engine.transposition;
import engine.killer;
import engine.history;
import engine.evaluator;

export namespace chess {

struct engine_t final {
    explicit engine_t(const std::string_view fen) noexcept;
    score_t operator()(const ply_t depth, const std::chrono::steady_clock::duration duration) noexcept;
    size_t nodes() const noexcept;
    size_t full() const noexcept;
    std::span<move_t> extract_pv(const std::span<move_t, MAX_PLY> buffer, ply_t max_length) noexcept;

    void setup(const std::string_view fen, const std::string_view moves) noexcept;
    void reset() noexcept;
    void clear() noexcept;

    side_t side() const noexcept;

private:
    score_t asearch(const score_t eval, const ply_t depth) noexcept;
    template <bool PV>
    score_t search(ply_t depth, ply_t height, score_t alpha, score_t beta) noexcept;
    score_t qsearch(ply_t height, score_t alpha, score_t beta) noexcept;
    score_t evaluate() noexcept;

    position_t _position;
    transposition_t _transposition;
    killer_t _killer;
    history_t _history;
    evaluator_t _evaluator;
    size_t _nodes;
    std::chrono::steady_clock::time_point _end;
};

} // namespace chess
