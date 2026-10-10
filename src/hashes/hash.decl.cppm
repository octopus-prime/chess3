module; 

#include <cstddef>
#include <cstdint>

export module hashes.hash:decl;

import types.piece;
import types.square;
import types.file;

export namespace chess {

using hash_t = uint64_t;

namespace hashes::lookup {

constexpr hash_t piece(const piece_t piece, const square_t square) noexcept;
constexpr hash_t side() noexcept;
constexpr hash_t castle(const square_t square) noexcept;
constexpr hash_t ep(const file_t file) noexcept;

}

}
