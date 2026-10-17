module;

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <string_view>

export module types.piece;

export import types.side;
export import types.type;

// decl

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

// impl

namespace chess {

constexpr piece_t::piece_t() noexcept : _index{} {}

constexpr piece_t::piece_t(const piece_e piece) noexcept : _index(piece) {
    assert(piece <= BK);
}

constexpr piece_t::piece_t(const side_t side, const type_t type) noexcept : _index(type + side * MAX_TYPE) {
    assert(side <= BLACK);
    assert(type <= K);
}

constexpr piece_t::piece_t(const std::string_view piece) noexcept : _index() {
    switch (piece.front()) {
        case 'P': _index = WP; break;
        case 'p': _index = BP; break;
        case 'N': _index = WN; break;
        case 'n': _index = BN; break;
        case 'B': _index = WB; break;
        case 'b': _index = BB; break;
        case 'R': _index = WR; break;
        case 'r': _index = BR; break;
        case 'Q': _index = WQ; break;
        case 'q': _index = BQ; break;
        case 'K': _index = WK; break;
        case 'k': _index = BK; break;
        default: _index = NO_PIECE; break;
    }
    assert(piece.size() >= 1);
    assert(side() <= BLACK);
    assert(type() <= K);
}

constexpr piece_t::operator piece_e() const noexcept {
    return piece_e(_index);
}

constexpr side_t piece_t::side() const noexcept {
    return side_e(_index / MAX_TYPE);
}

constexpr type_t piece_t::type() const noexcept {
    return type_e(_index % MAX_TYPE);
}

}
