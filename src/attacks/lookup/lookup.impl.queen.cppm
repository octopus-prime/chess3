export module attacks.lookup:impl.queen;

import :decl;
import types.squares;
import types.side;
import attacks.generator;

namespace chess::attacks::lookup {

constexpr squares_t queen(const square_t queen, const squares_t occupied) noexcept {
  return squares_e(rook(queen, occupied) | bishop(queen, occupied));
}

}
