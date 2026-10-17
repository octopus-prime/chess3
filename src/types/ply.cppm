module; 

#include <cstddef>
#include <cstdint>

export module types.ply;

export namespace chess {

using ply_t = uint8_t;

constexpr size_t MAX_PLY = 256;

}
