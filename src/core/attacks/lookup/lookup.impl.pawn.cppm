module;

#include <array>

export module core.attacks.lookup:impl.pawn;

import :decl;
import types.squares;
import types.side;
import core.attacks.generator;

namespace chess::attacks::lookup {

using table_t = std::array<std::array<squares_t, MAX_SQUARE>, MAX_SIDE>;

static_assert(sizeof(table_t) == 1024);

consteval table_t build_pawn_table() noexcept {
  table_t temp{};
  for (side_t side : {WHITE, BLACK})
    for (square_t square = a1; square <= h8; ++square)
      temp[side][square] = generator::pawns({square}, side);
  return temp;
}

constexpr squares_t pawn(const square_t pawn, const side_t side) noexcept {
  static constexpr table_t table = build_pawn_table();
  return table[side][pawn];
}

} // namespace chess::attacks::lookup
