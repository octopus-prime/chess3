module;

#include <string_view>
#include <istream>

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
    void go(const std::string_view args) noexcept;

    engine_t engine;
};

} // namespace chess
