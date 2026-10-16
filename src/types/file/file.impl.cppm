module;

#include <string_view>

export module types.file:impl;

import :decl;

namespace chess {

constexpr file_t::file_t() noexcept : _index() {
}

constexpr file_t::file_t(const file_e file) noexcept : _index(file) {
}

constexpr file_t::file_t(const std::string_view file) noexcept : _index(file.front() - 'a') {
}

constexpr file_t::operator file_e() const noexcept {
    return file_e(_index);
}

}
