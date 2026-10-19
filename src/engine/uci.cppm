module;

#include <string_view>
#include <istream>
#include <thread>
#include <chrono>

export module engine.uci;

import engine.engine;

export namespace chess {

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
