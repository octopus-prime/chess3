module; 

#include <cstddef>
#include <cstdint>

export module types.file:decl;

export namespace chess {

enum file_e : uint8_t {
    a, b, c, d, e, f, g, h
};

using file_t = file_e;

constexpr size_t MAX_FILE = 8;

}
