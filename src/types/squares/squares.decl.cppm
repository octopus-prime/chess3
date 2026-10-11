module;

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <type_traits>

export module types.squares:decl;

export import types.square;

export namespace chess {

enum squares_e : uint64_t {
  NO_SQUARES = 0ull,
  ALL_SQUARES = ~0ull
};

struct squares_t final {
    struct iterator_t final {
        constexpr iterator_t(std::underlying_type_t<squares_e> mask) noexcept;
        constexpr square_t operator*() const noexcept;
        constexpr iterator_t &operator++() noexcept;
        constexpr bool operator==(std::default_sentinel_t) noexcept;
    private:
        std::underlying_type_t<squares_e> mask;
    };

    constexpr squares_t() noexcept;
    constexpr squares_t(const squares_e squares) noexcept;
    constexpr squares_t(const std::initializer_list<square_t> squares) noexcept;
    constexpr squares_t(const std::initializer_list<rank_t> ranks) noexcept;
    constexpr squares_t(const std::initializer_list<file_t> files) noexcept;
    constexpr operator squares_e() const noexcept;
    constexpr bool operator[](const square_t square) const noexcept;
    constexpr bool empty() const noexcept;
    constexpr size_t size() const noexcept;
    constexpr square_t front() const noexcept;
    constexpr square_t back() const noexcept;
    constexpr iterator_t begin() const noexcept;
    constexpr std::default_sentinel_t end() const noexcept;
    constexpr void set(square_t square) noexcept;
    constexpr void set(squares_t squares) noexcept;
    constexpr void reset(square_t square) noexcept;
    constexpr void reset(squares_t squares) noexcept;
    constexpr void flip(square_t square) noexcept;
    constexpr void flip(squares_t squares) noexcept;
    constexpr squares_t operator~() const noexcept;
    constexpr squares_t operator&(const squares_t other) const noexcept;
    constexpr squares_t operator|(const squares_t other) const noexcept;
    constexpr squares_t operator^(const squares_t other) const noexcept;
    constexpr squares_t &operator&=(const squares_t other) noexcept;
    constexpr squares_t &operator|=(const squares_t other) noexcept;
    constexpr squares_t &operator^=(const squares_t other) noexcept;
    constexpr squares_t operator<<(const int shift) const noexcept;
    constexpr squares_t operator>>(const int shift) const noexcept;
private:
    std::underlying_type_t<squares_e> _mask;
};

}
 