module;

#include <cassert>
#include <cstdint>
#include <string_view>

export module core.move:impl;

import :decl;

namespace chess {

constexpr move_t::move_t() noexcept : _from{}, _to{}, _promotion{} {}

constexpr move_t::move_t(const square_t from, const square_t to, const type_t promotion) noexcept
    : _from(from), _to(to), _promotion(promotion) {}

constexpr move_t::move_t(const std::string_view move) noexcept {
  assert(move.size() >= 4);
  _from = square_t(move.substr(0));
  _to = square_t(move.substr(2));
  _promotion = type_t(move.substr(4));
}

constexpr move_t::operator uint16_t() const noexcept {
  return _from | (_to << 6) | (_promotion << 12);
}

constexpr square_t move_t::from() const noexcept {
  return square_e(_from);
}

constexpr square_t move_t::to() const noexcept {
  return square_e(_to);
}

constexpr type_t move_t::promotion() const noexcept {
  return type_e(_promotion);
}

} // namespace chess
