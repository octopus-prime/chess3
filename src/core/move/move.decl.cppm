module; 

#include <cstdint>

export module core.move:decl;

import types.square;
import types.type;

export namespace chess {

struct move_t final {
  constexpr move_t() noexcept;
  constexpr move_t(const square_t from, const square_t to, const type_t promotion = NO_TYPE) noexcept;
  constexpr operator uint16_t() const noexcept;
  constexpr square_t from() const noexcept;
  constexpr square_t to() const noexcept;
  constexpr type_t promotion() const noexcept;

private:
  uint16_t _from : 6;
  uint16_t _to : 6;
  uint16_t _promotion : 4;
};

}
