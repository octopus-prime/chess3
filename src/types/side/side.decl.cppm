module; 

#include <cstddef>
#include <cstdint>
#include <type_traits>

export module types.side:decl;

export namespace chess {

enum side_e : uint8_t {
    WHITE, BLACK
};

struct side_t {
    constexpr side_t() noexcept;
    constexpr side_t(const side_e side) noexcept;
    constexpr operator side_e() const noexcept;
    constexpr side_t operator~() const noexcept;

private:
    std::underlying_type_t<side_e> _index;
};

constexpr size_t MAX_SIDE = 2;

}
