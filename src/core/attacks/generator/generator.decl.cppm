export module core.attacks.generator:decl;

import types.squares;
import types.side;

export namespace chess::attacks::generator {

constexpr squares_t leapers(const squares_t kings, const squares_t knights) noexcept;
constexpr squares_t sliders(const squares_t rooks, const squares_t bishops, const squares_t occupied) noexcept;
constexpr squares_t pawns(const squares_t pawns, const side_t side) noexcept;

}
