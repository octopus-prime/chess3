module; 

#include <cstddef>
#include <cstdint>
#include <type_traits>

export module types.side:impl;

import :decl;

namespace chess {

constexpr side_t::side_t() noexcept : _index{} {}

constexpr side_t::side_t(const side_e side) noexcept : _index(side) {}

constexpr side_t::operator side_e() const noexcept {
    return side_e(_index);
}

constexpr side_t side_t::operator~() const noexcept {
    return side_e(1 - _index);
}

}
