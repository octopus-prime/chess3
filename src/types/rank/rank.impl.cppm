module;

#include <string_view>

export module types.rank:impl;

import :decl;

namespace chess {

constexpr rank_t::rank_t() noexcept : _index() {
}

constexpr rank_t::rank_t(const rank_e rank) noexcept : _index(rank) {
}

constexpr rank_t::rank_t(const std::string_view rank) noexcept : _index(rank.front() - '1') {
}

constexpr rank_t::operator rank_e() const noexcept {
    return rank_e(_index);
}

}
