module; 

#include <cstdint>
#include <array>
#include <immintrin.h>

export module core.attacks.lookup:impl.rook;

import :decl;
import types.squares;
import types.side;
import core.attacks.generator;

namespace chess::attacks::lookup {

using rook_table_t = std::array<std::array<uint8_t, 256>, 8>;

static_assert(sizeof(rook_table_t) == 2048);

consteval rook_table_t build_rook_table() noexcept {
  rook_table_t temp{};
  for (file_t file : {a, b, c, d, e, f, g, h}) {
    constexpr squares_t all{_1};
    const squares_t squares{square_t(file, _1)};
    for (size_t index = 0; index < 256; ++index) {
      const squares_t blockers = squares_e(__builtin_ia32_pdep_di(index, all));
      const squares_t attacks = generator::sliders(squares, {}, blockers);
      temp[file][index] = uint8_t(__builtin_ia32_pext_di(attacks, all));
    }
  }
  return temp;
}

constexpr squares_t rook(const square_t rook, const squares_t occupied) noexcept {
  static constexpr rook_table_t table = build_rook_table();
  const squares_t rank_mask{rook.rank()};
  const squares_t file_mask{rook.file()};
  const squares_t rank_attacks = squares_e(__builtin_ia32_pdep_di(table[rook.file()][__builtin_ia32_pext_di(occupied, rank_mask)], rank_mask));
  const squares_t file_attacks = squares_e(__builtin_ia32_pdep_di(table[rook.rank()][__builtin_ia32_pext_di(occupied, file_mask)], file_mask));
  return squares_e(rank_attacks | file_attacks);
}

} // namespace chess::attacks::lookup
