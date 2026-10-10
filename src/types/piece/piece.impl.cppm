module;

#include <cassert>

export module types.piece:impl;

import :decl;

namespace chess {

constexpr piece_t::piece_t() noexcept : _index{} {}

constexpr piece_t::piece_t(const piece_e piece) noexcept : _index(piece) {
    assert(piece <= BK);
}

constexpr piece_t::piece_t(const side_t side, const type_t type) noexcept : _index(type + side * MAX_TYPE) {
    assert(side <= BLACK);
    assert(type <= K);
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
