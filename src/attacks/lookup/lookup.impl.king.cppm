module; 

#include <array>

export module attacks.lookup:impl.king;

import :decl;
import types.squares;
import types.side;
import attacks.generator;

namespace chess::attacks::lookup {

using king_table_t = std::array<squares_t, MAX_SQUARE>;

static_assert(sizeof(king_table_t) == 512);

consteval king_table_t build_king_table() noexcept {
  king_table_t temp{};
  for (square_t square = a1; square <= h8; ++square)
    temp[square] = generator::leapers({square}, {});
  return temp;
}

constexpr squares_t king(const square_t king) noexcept {
  static constexpr king_table_t table = build_king_table();
  return table[king];
}

} // namespace chess::attacks::lookup
