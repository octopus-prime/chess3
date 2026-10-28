module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <ranges>
#include <span>

export module engine.ordering;

import types.score;
import core.position;
import core.move;
import engine.history;

export namespace chess {

struct scored_move_t {
    move_t move;
    score_t score;
};

// Sort descending by score, preserving the input order of ties.
void sort_scored_moves(std::span<scored_move_t> moves) noexcept;

struct sorter_t {
    const position_t& position;
    std::span<move_t> moves;
    move_t best_move;
    std::span<const move_t, 2> killer_moves;
    const history_t& history;
    const history_t* continuation = nullptr;

    std::array<scored_move_t, 256> scored_moves;
    size_t count{};

    // The lazy view borrows this sorter's scored buffer.
    auto operator()() & noexcept {
        count = 0;
        auto create = [this](int phase) noexcept -> std::span<scored_move_t> {
            const auto remaining_moves = moves.subspan(count);
            const auto remaining_scored_moves = std::span{scored_moves}.first(moves.size()).subspan(count);
            switch (phase) {
                case 0: {
                    const auto best = std::ranges::find(moves, best_move);
                    count += best != moves.end();
                    if (best != moves.end()) {
                        std::iter_swap(moves.begin(), best);
                        remaining_scored_moves.front() = {best_move};
                    }
                    return remaining_scored_moves.first(count);
                }
                case 1: {
                    for (size_t i = 0; i < remaining_moves.size(); ++i) {
                        const score_t score = position.tactical(remaining_moves[i]) ? position.see(remaining_moves[i]) : score_t{DRAW};
                        remaining_scored_moves[i] = {remaining_moves[i], score};
                    }
                    const auto tail = std::ranges::partition(remaining_scored_moves, [&](const scored_move_t& sm) {
                        return position.tactical(sm.move) && sm.score >= 0;
                    });
                    const auto result = remaining_scored_moves.first(tail.begin() - remaining_scored_moves.begin());
                    count += result.size();
                    sort_scored_moves(result);
                    return result;
                }
                case 2: {
                    size_t found{};
                    for (const move_t killer_move : killer_moves) {
                        const auto tail = remaining_scored_moves.subspan(found);
                        const auto best = std::ranges::find(tail, killer_move, &scored_move_t::move);
                        if (best != tail.end()) {
                            std::iter_swap(tail.begin(), best);
                            ++found;
                        }
                    }
                    count += found;
                    return remaining_scored_moves.first(found);
                }
                case 3: {
                    const auto tail = std::ranges::partition(remaining_scored_moves, [&](const scored_move_t& sm) { return !position.tactical(sm.move); });
                    const auto result = remaining_scored_moves.first(tail.begin() - remaining_scored_moves.begin());
                    count += result.size();
                    for (scored_move_t& sm : result) {
                        sm.score = score_e(history.score(position.at(sm.move.from()), sm.move.to(), continuation));
                    }
                    sort_scored_moves(result);
                    return result;
                }
                default: {
                    sort_scored_moves(remaining_scored_moves);
                    count += remaining_scored_moves.size();
                    return remaining_scored_moves;
                }
            }
        };
        return std::views::transform(std::views::iota(0, 5), create) | std::views::join;
    }
};

} // namespace chess
