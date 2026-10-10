module; 

#include <array>

export module attacks.lookup:impl.ray;

import :decl;
import :impl.bishop;
import :impl.rook;
import types.squares;
import types.side;
import attacks.generator;

namespace chess::attacks::lookup {

using ray_table_t = std::array<std::array<squares_t, MAX_DIRECTION>, MAX_SQUARE>;

static_assert(sizeof(ray_table_t) == 4096);

consteval ray_table_t build_ray_table() noexcept {
  ray_table_t temp{};
  for (square_t from = a1; from <= h8; ++from) {
    for (square_t to : rook(from, {})) {
      const auto rank_diff = to.rank() - from.rank();
      const auto file_diff = to.file() - from.file();
      if (file_diff == 0)
        temp[from][rank_diff > 0 ? NORTH : SOUTH] |= squares_t{to};
      if (rank_diff == 0)
        temp[from][file_diff > 0 ? EAST : WEST] |= squares_t{to};
    }
    for (square_t to : bishop(from, {})) {
      const auto rank_diff = to.rank() - from.rank();
      const auto file_diff = to.file() - from.file();
      if (file_diff == rank_diff)
        temp[from][rank_diff > 0 ? NORTH_EAST : SOUTH_WEST] |= squares_t{to};
      if (file_diff == -rank_diff)
        temp[from][rank_diff > 0 ? NORTH_WEST : SOUTH_EAST] |= squares_t{to};
    }
  }
  return temp;
}

constexpr squares_t ray(const square_t from, const direction_e direction) noexcept {
  static constexpr ray_table_t table = build_ray_table();
  return table[from][direction];
}

} // namespace chess::attacks::lookup
