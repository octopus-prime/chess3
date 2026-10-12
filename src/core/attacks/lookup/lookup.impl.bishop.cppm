module; 

#include <cstdint>
#include <array>
#include <immintrin.h>

export module core.attacks.lookup:impl.bishop;

import :decl;
import types.squares;
import types.side;
import core.attacks.generator;

namespace chess::attacks::lookup {

struct meta_t final {
    uint64_t occ;
    uint64_t all;
    size_t offset;
};

struct table_t final {
  std::array<meta_t, MAX_SQUARE> meta{};
  std::array<uint16_t, 5248> data{};
};

static_assert(sizeof(table_t) == 12032);

consteval squares_t relevant_occupancy(const square_t square, const squares_t occupied) noexcept {
    uint64_t mask = occupied;
    if (square.rank() > _1) mask &= ~squares_t{_1};
    if (square.rank() < _8) mask &= ~squares_t{_8};
    if (square.file() > a) mask &= ~squares_t{a};
    if (square.file() < h) mask &= ~squares_t{h};
    return squares_e(mask);
};

consteval table_t build_bishop_table() noexcept {
  table_t temp{};
  size_t offset{};
  for (square_t square = a1; square <= h8; ++square) {
    const squares_t squares{square};
    const squares_t all = generator::sliders({}, squares, {});
    const squares_t occ = relevant_occupancy(square, all);
    temp.meta[square] = {occ, all, offset};
    for (size_t index = 0; index < (1u << occ.size()); ++index, ++offset) {
      const squares_t blockers = squares_e(__builtin_ia32_pdep_di(index, occ));
      const squares_t attacks = generator::sliders({}, squares, blockers);
      temp.data[offset] = uint16_t(__builtin_ia32_pext_di(attacks, all));
    }
  }
  return temp;
}

constexpr squares_t bishop(const square_t bishop, const squares_t occupied) noexcept {
  static constexpr table_t table = build_bishop_table();
  const meta_t &meta = table.meta[bishop];
  return squares_e(__builtin_ia32_pdep_di(table.data[meta.offset + __builtin_ia32_pext_di(occupied, meta.occ)], meta.all));
}

} // namespace chess::attacks::lookup
