module;

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <string_view>

export module types.type:decl;

export namespace chess {

enum type_e : uint8_t {
    NO_TYPE, 
    P, N, B, R, Q, K
};

struct type_t final {
  constexpr type_t() noexcept;
  constexpr type_t(const type_e type) noexcept;
  constexpr type_t(const std::string_view type) noexcept;
  constexpr operator type_e() const noexcept;

private:
  std::underlying_type_t<type_e> _index;
};

constexpr size_t MAX_TYPE = 8;

}
