module;

#include <cstdint>
#include <cstddef>

export module attacks.lookup:decl;

import types.squares;
import types.side;

export namespace chess::attacks::lookup {
    
enum direction_e : uint8_t {
    NORTH,
    NORTH_EAST,
    EAST,
    SOUTH_EAST,
    SOUTH,
    SOUTH_WEST,
    WEST,
    NORTH_WEST
};

using direction_t = direction_e;

constexpr size_t MAX_DIRECTION = 8;

constexpr squares_t king(const square_t king) noexcept;
constexpr squares_t knight(const square_t knight) noexcept;
constexpr squares_t rook(const square_t rook, const squares_t occupied) noexcept;
constexpr squares_t bishop(const square_t bishop, const squares_t occupied) noexcept;
constexpr squares_t queen(const square_t queen, const squares_t occupied) noexcept;
constexpr squares_t pawn(const square_t pawn, const side_t side) noexcept;
constexpr squares_t line(const square_t from, const square_t to) noexcept;
constexpr squares_t ray(const square_t from, const direction_e direction) noexcept;

}
