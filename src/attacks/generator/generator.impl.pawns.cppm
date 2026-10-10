module; 

#include <immintrin.h>

export module attacks.generator:impl.pawns;

import types.squares;
import types.side;

namespace chess::attacks::generator {

constexpr squares_t pawns(const squares_t pawns, const side_t side) noexcept {
  using vector_t = __v2du;
  constexpr vector_t shift{9, 7};
  constexpr vector_t exclude_left{squares_t{a}, squares_t{h}};
  constexpr vector_t exclude_right{squares_t{h}, squares_t{a}};
  const vector_t mask{pawns, pawns};
  const vector_t result = side == WHITE ? (mask << shift) & ~exclude_left
                                      : (mask >> shift) & ~exclude_right;
  return squares_e(__builtin_reduce_or(result));
}

}
