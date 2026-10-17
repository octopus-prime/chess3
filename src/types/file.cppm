module; 

#include <cstddef>
#include <cstdint>
#include <string_view>

export module types.file;

// decl

export namespace chess {

enum file_e : uint8_t {
    a, b, c, d, e, f, g, h
};

struct file_t final {
  constexpr file_t() noexcept;
  constexpr file_t(const file_e file) noexcept;
  constexpr file_t(const std::string_view file) noexcept;
  constexpr operator file_e() const noexcept;

private:
  std::underlying_type_t<file_e> _index;
};

constexpr size_t MAX_FILE = 8;

}

// impl

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
