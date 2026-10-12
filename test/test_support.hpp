#pragma once

#include <iostream>
#include <source_location>
#include <string_view>

namespace test_support {

inline bool check(const bool passed, const std::string_view name,
                  const std::source_location location = std::source_location::current()) {
    if (!passed)
        std::cerr << location.file_name() << ':' << location.line()
                  << ": FAILED: " << name << '\n';
    return passed;
}

}
