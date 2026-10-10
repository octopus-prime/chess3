module; 

#include <cstdint>
#include <immintrin.h>

export module attacks.generator:impl.sliders;

import types.squares;
import types.side;

namespace chess::attacks::generator {

constexpr squares_t sliders(const squares_t rooks, const squares_t bishops, const squares_t occupied) noexcept {
  using vector_t = __v4du;
  constexpr vector_t shift{1, 8, 7, 9};
  constexpr vector_t exclude_left{squares_t{a}, squares_t(), squares_t{h}, squares_t{a}};
  constexpr vector_t exclude_right{squares_t{h}, squares_t(), squares_t{a}, squares_t{h}};
  const vector_t mask{rooks, rooks, bishops, bishops};

  vector_t left(mask);
  vector_t right(mask);

  vector_t board(uint64_t(~occupied) & ~exclude_left); 
  left |= board & (left << shift);
  board &= (board << shift);
  left |= board & (left << (shift * 2));
  board &= (board << (shift * 2));
  left |= board & (left << (shift * 4));
  left = (left << shift) & ~exclude_left;

  board = vector_t(uint64_t(~occupied) & ~exclude_right);
  right |= board & (right >> shift);
  board &= (board >> shift);
  right |= board & (right >> (shift * 2));
  board &= (board >> (shift * 2));
  right |= board & (right >> (shift * 4));
  right = (right >> shift) & ~exclude_right;

  return squares_e(__builtin_reduce_or(left | right));
}

}
