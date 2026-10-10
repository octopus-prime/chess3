module;

#include <cstddef>
#include <cstdint>

export module types.rank:decl;

export namespace chess {

enum rank_e : uint8_t {
    _1, _2, _3, _4, _5, _6, _7, _8
};

using rank_t = rank_e;

constexpr size_t MAX_RANK = 8;

}
