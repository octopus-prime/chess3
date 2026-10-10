module; 

#include <immintrin.h>

export module attacks.generator:impl.leapers;

import types.squares;

namespace chess::attacks::generator {

constexpr squares_t leapers(const squares_t kings, const squares_t knights) noexcept {
  using vector_t = __v8du;
  constexpr vector_t shift{1,  8,  7,  9, 10, 17, 15, 6};
  constexpr vector_t exclude_left{
      squares_t{a},    squares_t(),  squares_t{h}, squares_t{a},
      squares_t{a, b}, squares_t{a}, squares_t{h}, squares_t{g, h}};
  constexpr vector_t exclude_right{
      squares_t{h},    squares_t(),  squares_t{a}, squares_t{h},
      squares_t{g, h}, squares_t{h}, squares_t{a}, squares_t{a, b}};
  const vector_t mask{kings,   kings,   kings,   kings,
                    knights, knights, knights, knights};
  const vector_t left = (mask << shift) & ~exclude_left;
  const vector_t right = (mask >> shift) & ~exclude_right;
  return squares_e(__builtin_reduce_or(left | right));
}

}
