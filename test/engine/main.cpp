#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <string_view>

#ifdef _WIN32
#include <malloc.h>
#endif

namespace {
std::atomic<size_t> allocations{};
}

size_t test_allocation_count() noexcept {
    return allocations.load(std::memory_order_relaxed);
}

void* operator new(const size_t size) {
    allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* pointer = std::malloc(size ? size : 1))
        return pointer;
    throw std::bad_alloc{};
}

void* operator new[](const size_t size) {
    return ::operator new(size);
}

void* operator new(const size_t size, const std::align_val_t alignment) {
    allocations.fetch_add(1, std::memory_order_relaxed);
    const auto align = static_cast<size_t>(alignment);
#ifdef _WIN32
    void* pointer = _aligned_malloc(size ? size : 1, align);
#else
    const auto bytes = size ? size : 1;
    if (bytes > std::numeric_limits<size_t>::max() - (align - 1))
        throw std::bad_alloc{};
    void* pointer = std::aligned_alloc(align, (bytes + align - 1) / align * align);
#endif
    if (pointer)
        return pointer;
    throw std::bad_alloc{};
}

void* operator new[](const size_t size, const std::align_val_t alignment) {
    return ::operator new(size, alignment);
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::align_val_t) noexcept {
#ifdef _WIN32
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
}
void operator delete[](void* pointer, std::align_val_t alignment) noexcept { ::operator delete(pointer, alignment); }
void operator delete(void* pointer, size_t, std::align_val_t alignment) noexcept { ::operator delete(pointer, alignment); }
void operator delete[](void* pointer, size_t, std::align_val_t alignment) noexcept { ::operator delete(pointer, alignment); }

bool run_engine_tests();
bool run_mate_tests();
bool run_evaluator_tests();
bool run_uci_tests();

int main(int argc, char** argv) {
    const std::string_view suite = argc == 2 ? argv[1] : "all";
    if (argc > 2 || (suite != "all" && suite != "engine" && suite != "evaluator" && suite != "mates" && suite != "uci")) {
        std::cerr << "Usage: chess_engine_tests [all|engine|evaluator|mates|uci]\n";
        return 2;
    }
    bool passed = true;
    if (suite == "all" || suite == "engine")
        passed &= run_engine_tests();
    if (suite == "all" || suite == "evaluator")
        passed &= run_evaluator_tests();
    if (suite == "all" || suite == "mates")
        passed &= run_mate_tests();
    if (suite == "all" || suite == "uci")
        passed &= run_uci_tests();
    return passed ? 0 : 1;
}
