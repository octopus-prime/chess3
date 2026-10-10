module;

#include <bit>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iterator>

export module types.squares:impl;

import :decl;

namespace chess {

constexpr squares_t::iterator_t::iterator_t(std::underlying_type_t<squares_e> mask) noexcept : mask{mask} {}

constexpr square_t squares_t::iterator_t::operator*() const noexcept {
    return square_e(std::countr_zero(mask));
}

constexpr squares_t::iterator_t &squares_t::iterator_t::operator++() noexcept {
    mask &= (mask - 1);
    return *this;
}

constexpr bool squares_t::iterator_t::operator==(std::default_sentinel_t) noexcept {
    return !mask;
}

constexpr squares_t::squares_t() noexcept : _mask{} {}

constexpr squares_t::squares_t(const squares_e squares) noexcept : _mask(squares) {}
    
constexpr squares_t::squares_t(const std::initializer_list<square_t> squares) noexcept : _mask{} {
    constexpr auto mask = 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'00000001ull;
    for (square_t square : squares) {
        assert(square <= h8);
        _mask |= mask << square;
    }
}

constexpr squares_t::squares_t(const std::initializer_list<rank_t> ranks) noexcept : _mask{} {
    constexpr auto mask = 0b00000000'00000000'00000000'00000000'00000000'00000000'00000000'11111111ull;
    for (rank_t rank : ranks) {
        assert(rank <= _8);
        _mask |= mask << (8 * rank);
    }
}

constexpr squares_t::squares_t(const std::initializer_list<file_t> files) noexcept : _mask{} {
    constexpr auto mask = 0b00000001'00000001'00000001'00000001'00000001'00000001'00000001'00000001ull;
    for (file_t file : files) {
        assert(file <= h);
        _mask |= mask << file;
    }
}

constexpr squares_t::operator squares_e() const noexcept {
    return squares_e(_mask);
}

constexpr bool squares_t::operator[](const square_t square) const noexcept {
    assert(square <= h8);
    return _mask & squares_t{square};
}

constexpr bool squares_t::empty() const noexcept {
    return !_mask;
}

constexpr size_t squares_t::size() const noexcept {
    return std::popcount(_mask);
}

constexpr square_t squares_t::front() const noexcept {
    assert(!empty());
    return square_e(std::countr_zero(_mask));
}

constexpr square_t squares_t::back() const noexcept {
    assert(!empty());
    return square_e(63 - std::countl_zero(_mask));
}

constexpr squares_t::iterator_t squares_t::begin() const noexcept {
    return iterator_t(_mask);
}

constexpr std::default_sentinel_t squares_t::end() const noexcept {
    return {};
}

constexpr void squares_t::set(square_t square) noexcept {
    _mask |= squares_t{square}._mask;
}

constexpr void squares_t::set(squares_t squares) noexcept {
    _mask |= squares._mask;
}

constexpr void squares_t::reset(square_t square) noexcept {
    _mask &= ~squares_t{square}._mask;
}

constexpr void squares_t::reset(squares_t squares) noexcept {
    _mask &= ~squares._mask;
}

constexpr void squares_t::flip(square_t square) noexcept {
    _mask ^= squares_t{square}._mask;
}

constexpr void squares_t::flip(squares_t squares) noexcept {
    _mask ^= squares._mask;
}

constexpr squares_t squares_t::operator~() const noexcept {
    return squares_e(~_mask);
}

constexpr squares_t squares_t::operator&(const squares_t other) const noexcept {
    return squares_e(_mask & other._mask);
}

constexpr squares_t squares_t::operator|(const squares_t other) const noexcept {
    return squares_e(_mask | other._mask);
}

constexpr squares_t squares_t::operator^(const squares_t other) const noexcept {
    return squares_e(_mask ^ other._mask);
}

constexpr squares_t &squares_t::operator&=(const squares_t other) noexcept {
    _mask &= other._mask;
    return *this;
}

constexpr squares_t &squares_t::operator|=(const squares_t other) noexcept {
    _mask |= other._mask;
    return *this;
}

constexpr squares_t &squares_t::operator^=(const squares_t other) noexcept {
    _mask ^= other._mask;
    return *this;
}
    
constexpr squares_t squares_t::operator<<(const int shift) const noexcept {
    return squares_e(_mask << shift);
}

constexpr squares_t squares_t::operator>>(const int shift) const noexcept {
    return squares_e(_mask >> shift);
}

} // namespace chess
