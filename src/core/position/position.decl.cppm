module;

#include <array>
#include <span>
#include <string_view>
#include <tuple>

export module core.position:decl;

import types.square;
import types.type;
import types.piece;
import types.side;
import types.squares;
import core.move;
export import hashes.hash;

export namespace chess {

struct position_t final {
  struct state_t final {
    squares_t checkers;
    squares_t special;
    hash_t hash;
    piece_t captured;
    square_t captured_square;
  };
  struct stack_t final {
    constexpr stack_t() noexcept;
    constexpr void clear() noexcept;
    constexpr void push(const state_t& state) noexcept;
    constexpr void pop() noexcept;
    constexpr const state_t& top() const noexcept;
  private:
      std::array<state_t, 1024> _data;
      size_t _size;
  };
  constexpr position_t() noexcept;
  constexpr position_t(const std::string_view fen) noexcept;
  constexpr piece_t at(const square_t square) const noexcept;
  constexpr squares_t by() const noexcept;
  constexpr squares_t by(const side_t side) const noexcept;
  constexpr squares_t by(const type_t type) const noexcept;
  constexpr squares_t by(const piece_t piece) const noexcept;
  constexpr squares_t by(const side_t side, const type_t type) const noexcept;
  constexpr squares_t by(const type_t type1, const type_t type2) const noexcept;
  constexpr squares_t by(const side_t side, const type_t type1, const type_t type2) const noexcept;
  constexpr side_t side() const noexcept;
  constexpr hash_t hash() const noexcept;
  constexpr squares_t checkers() const noexcept;
  constexpr squares_t en_passant() const noexcept;
  constexpr squares_t castling() const noexcept;
  constexpr squares_t attackers(const side_t side) const noexcept;
  constexpr squares_t attackers(const square_t square) const noexcept;
  constexpr bool tactical(const move_t move) const noexcept;

  std::span<move_t> generate_all_moves(const std::span<move_t, 256> buffer) const noexcept;
  std::span<move_t> generate_tactical_moves(const std::span<move_t, 256> buffer) const noexcept;

  constexpr void do_move(const move_t move) noexcept;
  constexpr void undo_move(const move_t move) noexcept;

  constexpr std::tuple<squares_t, squares_t> find_snipers_and_blockers(const side_t side) const noexcept;
private:
  std::span<move_t> generate_moves(const squares_t filter, const squares_t pawn_filter,
                                 const std::span<move_t, 256> buffer) const noexcept;

  std::array<piece_t, MAX_SQUARE> _piece_at_square;
  std::array<squares_t, MAX_TYPE> _occupied_by_type;
  std::array<squares_t, MAX_SIDE> _occupied_by_side;
  squares_t _checkers;
  squares_t _special; // castle and ep
  side_t _side;
  hash_t _hash;
  stack_t _stack;
};

} // namespace chess
