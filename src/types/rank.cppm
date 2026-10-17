module;

#include <cstddef>
#include <cstdint>
#include <string_view>

export module types.rank;

// decl

export namespace chess {

enum rank_e : uint8_t {
    _1, _2, _3, _4, _5, _6, _7, _8
};

struct rank_t final {
  constexpr rank_t() noexcept;
  constexpr rank_t(const rank_e rank) noexcept;
  constexpr rank_t(const std::string_view rank) noexcept;
  constexpr operator rank_e() const noexcept;

private:
  std::underlying_type_t<rank_e> _index;
};  

constexpr size_t MAX_RANK = 8;

}

// impl

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
