module;

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <ranges>
#include <span>

export module engine.ordering;

import core.position;
import core.move;

export namespace chess {

namespace detail {

template <typename Projection, typename RemainingProjection>
auto ordered_moves(const position_t& position, const std::span<move_t> moves, const move_t best_move, const std::span<const move_t, 2> killer_moves, Projection get_score, RemainingProjection get_history_score) noexcept {
    const auto create = [&position, moves, best_move, killer_moves, get_score, get_history_score, count = size_t{}](const int phase) mutable -> std::span<move_t> {
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
                const auto tail = std::ranges::partition(remaining, [&](const move_t move) { return position.tactical(move) && get_score(move) >= 0; });
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
            case 3: {
                const auto tail = std::ranges::partition(remaining, [&](const move_t move) { return !position.tactical(move); });
                const auto result = remaining.first(tail.begin() - remaining.begin());
                count += result.size();
                std::ranges::sort(result, std::greater<>(), get_history_score);
                return result;
            }
            default: {
                std::ranges::sort(remaining, std::greater<>(), get_score);
                count += remaining.size();
                return remaining;
            }
        }
    };
    return std::views::iota(0, 5) | std::views::transform(create) | std::views::join;
}

} // namespace detail

} // namespace chess
