module; 

#include <cstddef>
#include <cstdint>
#include <type_traits>

export module types.score:decl;

import types.type;

export namespace chess {

enum score_e : int16_t {
    IGNORE = -32100,
    MIN = -32000, 
    LOSS = -30000,
    DRAW = 0, 
    CP = 1,
    PAWN = 100,
    KNIGHT = 300,
    BISHOP = 300,
    ROOK = 500,
    QUEEN = 900,
    KING = 20000,
    WIN = -LOSS, 
    MAX = -MIN
};

struct score_t final {
    constexpr score_t() noexcept;
    constexpr score_t(const score_e score) noexcept;
    constexpr score_t(const type_t type) noexcept;
    constexpr operator score_e() const noexcept;
    constexpr bool ignore() const noexcept;
    constexpr bool win() const noexcept;
    constexpr bool loss() const noexcept;
    constexpr bool decisive() const noexcept;
    constexpr bool draw() const noexcept;
    constexpr score_t operator-() const noexcept;
    constexpr score_t operator+(const score_t other) const noexcept;
    constexpr score_t operator-(const score_t other) const noexcept;
    constexpr score_t& operator+=(const score_t other) noexcept;
    constexpr score_t& operator-=(const score_t other) noexcept;
    constexpr score_t operator*(const int value) const noexcept;
    constexpr score_t operator/(const int value) const noexcept;

private:
    std::underlying_type_t<score_e> _value;
};

}
