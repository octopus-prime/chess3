module; 

#include <cstddef>
#include <cstdint>

export module core.hashes.lookup:decl;

import types.hash;
import types.piece;
import types.square;
import types.file;

export namespace chess::hashes::lookup {

constexpr hash_t piece(const piece_t piece, const square_t square) noexcept;
constexpr hash_t side() noexcept;
constexpr hash_t castle(const square_t square) noexcept;
constexpr hash_t ep(const file_t file) noexcept;

}
