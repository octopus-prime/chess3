module;

#include <cassert>
#include <string_view>

export module types.square:impl;

import :decl;

namespace chess {

constexpr square_t::square_t() noexcept : _index{} {}

constexpr square_t::square_t(const square_e square) noexcept : _index(square) {
    assert(square <= h8);
}

constexpr square_t::square_t(const file_t file, const rank_t rank) noexcept : _index(file + rank * MAX_FILE) {
    assert(file <= h);
    assert(rank <= _8);
}

constexpr square_t::square_t(const std::string_view square) noexcept : square_t(square.substr(0), square.substr(1)) {
    assert(square.size() >= 2);
    assert(file() <= h);
    assert(rank() <= _8);
}
    
constexpr square_t::operator square_e() const noexcept {
    return square_e(_index);
}

constexpr file_t square_t::file() const noexcept {
    return file_e(_index % MAX_FILE);
}

constexpr rank_t square_t::rank() const noexcept {
    return rank_e(_index / MAX_FILE);
}

constexpr square_t& square_t::operator++() noexcept {
    ++_index;
    return *this;
}

constexpr square_t& square_t::operator--() noexcept {
    --_index;
    return *this;
}

}
