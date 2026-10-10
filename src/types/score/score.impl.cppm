module; 

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

export module types.score:impl;

import :decl;

namespace chess {

constexpr std::array<score_e, MAX_TYPE> scores {DRAW, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, DRAW};

constexpr score_t::score_t() noexcept : _value(DRAW) {}

constexpr score_t::score_t(const score_e score) noexcept : _value(score) {}

constexpr score_t::score_t(const type_t type) noexcept : _value(scores[type]) {}

constexpr score_t::operator score_e() const noexcept {
    return score_e(_value);
}

constexpr bool score_t::ignore() const noexcept {
    return _value == IGNORE || _value == -IGNORE;
}

constexpr bool score_t::win() const noexcept {
    return _value >= WIN;
}

constexpr bool score_t::loss() const noexcept {
    return _value <= LOSS;
}

constexpr bool score_t::decisive() const noexcept {
    return win() || loss();
}

constexpr bool score_t::draw() const noexcept {
    return _value == DRAW;
}

constexpr score_t score_t::operator-() const noexcept {
    return score_e(-_value);
}

constexpr score_t score_t::operator+(const score_t other) const noexcept {
    return score_e(_value + other._value);
}

constexpr score_t score_t::operator-(const score_t other) const noexcept {
    return score_e(_value - other._value);
}

constexpr score_t& score_t::operator+=(const score_t other) noexcept {
    _value += other._value;
    return *this;
}

constexpr score_t& score_t::operator-=(const score_t other) noexcept {
    _value -= other._value;
    return *this;
}

constexpr score_t score_t::operator*(const int value) const noexcept {
    return score_e(_value * value);
}

constexpr score_t score_t::operator/(const int divisor) const noexcept {
    return score_e(_value / divisor);
}
}
