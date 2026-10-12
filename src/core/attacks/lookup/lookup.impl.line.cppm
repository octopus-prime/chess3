module; 

#include <array>

export module core.attacks.lookup:impl.line;

import :decl;
import :impl.bishop;
import :impl.rook;
import types.squares;
import types.side;

namespace chess::attacks::lookup {

using line_table_t = std::array<std::array<squares_t, MAX_SQUARE>, MAX_SQUARE>;

static_assert(sizeof(line_table_t) == 32768);

consteval line_table_t build_line_table() noexcept {
  line_table_t temp{};
  for (square_t from = a1; from <= h8; ++from)
    for (square_t to = a1; to <= h8; ++to) {
      temp[from][to] = squares_t{from, to};
      if (from == to)
        continue;
      squares_t board = bishop(from, {}) | rook(from, {});
      if (!board[to])
        continue;
      for (int sq = from; sq != to; ) {
        temp[from][to] |= squares_t{static_cast<square_t>(static_cast<square_e>(sq & 63))};
        sq += (from.file() == to.file()) ? 0 : (from.file() > to.file()) ? -1 : +1;
        sq += (from.rank() == to.rank()) ? 0 : (from.rank() > to.rank()) ? -8 : +8;
      }
    }
  return temp;
}

constexpr squares_t line(const square_t from, const square_t to) noexcept {
  static constexpr line_table_t table = build_line_table();
  return table[from][to];
}

} // namespace chess::attacks::lookup
