module;

#include <array>

export module attacks.lookup:impl.knight;

import :decl;
import types.squares;
import types.side;
import attacks.generator;

namespace chess::attacks::lookup {

using knight_table_t = std::array<squares_t, MAX_SQUARE>;

static_assert(sizeof(knight_table_t) == 512);

consteval knight_table_t build_knight_table() noexcept {
  knight_table_t temp{};
  for (square_t square = a1; square <= h8; ++square)
    temp[square] = generator::leapers({}, {square});
  return temp;
}

constexpr squares_t knight(const square_t knight) noexcept {
  static constexpr knight_table_t table = build_knight_table();
  return table[knight];
}

} // namespace chess::attacks::lookup
