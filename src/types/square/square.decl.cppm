module;

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <array>

export module types.square:decl;

export import types.file;
export import types.rank;

export namespace chess {

enum square_e : uint8_t {
    a1, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8
};

struct square_t final {
  constexpr square_t() noexcept;
  constexpr square_t(const square_e square) noexcept;
  constexpr square_t(const file_t file, const rank_t rank) noexcept;
  constexpr operator square_e() const noexcept;
  constexpr file_t file() const noexcept;
  constexpr rank_t rank() const noexcept;
  constexpr square_t& operator++() noexcept;
  constexpr square_t& operator--() noexcept;

private:
  std::underlying_type_t<square_e> _index;
};

constexpr size_t MAX_SQUARE = 64;

}
