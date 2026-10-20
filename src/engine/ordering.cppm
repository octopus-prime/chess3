module;

#include <algorithm>
#include <cstddef>
#include <functional>
#include <ranges>
#include <span>

export module engine.ordering;

import types.score;
import core.position;
import core.move;
import engine.history;

export namespace chess {

auto ordered_moves(const position_t& position, const std::span<move_t> moves,
                   const move_t best_move, const std::span<const move_t, 2> killer_moves,
                   const history_t& history, const capture_history_t& capture_history,
                   const history_t* continuation = nullptr) noexcept {
    const auto sort_tactical = [&position, &capture_history](const std::span<move_t> tactical) {
        const auto capture_score = [&](const move_t move) {
            const type_t victim = position.at(move.to()).type();
            const type_t captured = victim == NO_TYPE &&
                    position.at(move.from()).type() == P &&
                    position.en_passant()[move.to()]
                ? type_t{P}
                : victim;
            return captured == NO_TYPE ? 0 :
                capture_history.get(position.at(move.from()), move.to(), captured);
        };
        std::ranges::sort(tactical, [&](const move_t left, const move_t right) {
            const int left_see = position.see(left);
            const int right_see = position.see(right);
            if (left_see != right_see)
                return left_see > right_see;
            return capture_score(left) > capture_score(right);
        });
    };
    const auto create = [&position, moves, best_move, killer_moves, &history, continuation,
                         sort_tactical, count = size_t{}](const int phase) mutable -> std::span<move_t> {
        const auto remaining = moves.subspan(count, moves.size() - count);
        switch (phase) {
            case 0: {
                const auto best = std::ranges::find(moves, best_move);
                count += best != moves.end();
                if (best != moves.end()) {
                    std::iter_swap(moves.begin(), best);
                    remaining.front() = best_move;
                }
                return remaining.first(count);
            }
            case 1: {
                const auto tail = std::ranges::partition(remaining, [&](const move_t move) { return position.tactical(move) && position.see(move) >= 0; });
                const auto result = remaining.first(tail.begin() - remaining.begin());
                count += result.size();
                sort_tactical(result);
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
                std::ranges::sort(result, std::greater<>(), [&](const move_t move) {
                    const auto piece = position.at(move.from());
                    return history.score(piece, move.to(), continuation);
                });
                return result;
            }
            default: {
                sort_tactical(remaining);
                count += remaining.size();
                return remaining;
            }
        }
    };
    return std::views::transform(std::views::iota(0, 5), create) | std::views::join;
}

} // namespace chess
