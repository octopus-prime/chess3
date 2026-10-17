module;

#include <string_view>

export module types.type:impl;

import :decl;

namespace chess {

constexpr type_t::type_t() noexcept : _index() {
}

constexpr type_t::type_t(const type_e type) noexcept : _index(type) {
}

constexpr type_t::type_t(const std::string_view type) noexcept : _index() {
    if (!type.empty()) {
        switch (type.front()) {
            case 'p': _index = P; break;
            case 'n': _index = N; break;
            case 'b': _index = B; break;
            case 'r': _index = R; break;
            case 'q': _index = Q; break;
            case 'k': _index = K; break;
            default: _index = NO_TYPE; break;
        }
    }
}

constexpr type_t::operator type_e() const noexcept {
    return type_e(_index);
}

}
