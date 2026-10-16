module;

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <string_view>

export module types.piece:decl;

export import types.side;
export import types.type;

export namespace chess {

enum piece_e : uint8_t {
    NO_PIECE = NO_TYPE,
    WP = P,            WN, WB, WR, WQ, WK,
    BP = P + MAX_TYPE, BN, BB, BR, BQ, BK
};

struct piece_t final {
  constexpr piece_t() noexcept;
  constexpr piece_t(const piece_e piece) noexcept;
  constexpr piece_t(const side_t side, const type_t type) noexcept;
  constexpr piece_t(const std::string_view piece) noexcept;
  constexpr operator piece_e() const noexcept;
  constexpr side_t side() const noexcept;
  constexpr type_t type() const noexcept;

private:
  std::underlying_type_t<piece_e> _index;
};

constexpr size_t MAX_PIECE = MAX_TYPE * MAX_SIDE;

}
