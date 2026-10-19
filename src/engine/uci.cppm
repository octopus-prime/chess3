module;

#include <string_view>
#include <istream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstdint>

export module engine.uci;

import engine.engine;

export namespace chess {

struct clock_budget_t {
    std::chrono::milliseconds soft;
    std::chrono::milliseconds hard;
};

constexpr clock_budget_t clock_budget(const int64_t remaining, const int64_t increment,
                                      const int64_t moves, const int overhead) noexcept {
    // Bound before multiplying: UCI permits an arbitrarily large movestogo.
    const auto reserve = std::min(remaining, std::min(moves, remaining) * overhead);
    const auto available = remaining - reserve;
    const auto soft = std::min(available, available / moves + increment * 3 / 4);
    return {std::chrono::milliseconds{soft},
            std::chrono::milliseconds{std::min(available, soft * 3)}};
}

struct uci_t final {
    uci_t();
    void operator()(std::istream& stream) noexcept;
private:
    void uci() noexcept;
    void isready() noexcept;
    void ucinewgame() noexcept;
    void position(const std::string_view args) noexcept;
    void go(std::string_view args, std::chrono::steady_clock::time_point received) noexcept;
    void stop() noexcept;
    void setoption(std::string_view args) noexcept;

    engine_t engine;
    std::jthread worker;
    int move_overhead = 20;
};

} // namespace chess
