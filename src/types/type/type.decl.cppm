module;

#include <cstddef>
#include <cstdint>

export module types.type:decl;

export namespace chess {

enum type_e : uint8_t {
    NO_TYPE, 
    P, N, B, R, Q, K
};

using type_t = type_e;

constexpr size_t MAX_TYPE = 8;

}
