module;

#include <cstdint>
#include <format>
#include <memory>
#include <span>
#include <string_view>
#include <iostream>
#include <ranges>

module engine.uci;

import types.square;
import types.score;
import types.side;
import types.piece;
import types.squares;
import nnue.nnue;

using namespace std::string_view_literals;

namespace chess {

uci_t::uci_t() : engine{"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"} {
}

void uci_t::operator()(std::istream& stream) noexcept {
    std::array<char, 4096> buffer;
    for(;;) {
        stream.getline(buffer.data(), buffer.size());
        std::string_view line{buffer.data()};
        if (line.starts_with("ucinewgame"sv))
            ucinewgame();
        else if (line.starts_with("uci"sv))
            uci();
        else if (line.starts_with("isready"sv))
            isready();
        else if (line.starts_with("position "sv)) {
            line.remove_prefix("position "sv.size());
            position(line);
        }
        else if (line.starts_with("go "sv)) {
            line.remove_prefix("go "sv.size());
            go(line);
        }
        else if (line.starts_with("quit"sv))
            quit();
    }
}

void uci_t::uci() noexcept {
    constexpr std::string_view buffer = "id name chess\nid author me\nuciok\n";
    std::fwrite(buffer.data(), sizeof(char), buffer.size(), stdout);
}

void uci_t::isready() noexcept {
    constexpr std::string_view buffer = "readyok\n";
    std::fwrite(buffer.data(), sizeof(char), buffer.size(), stdout);
}

void uci_t::ucinewgame() noexcept {
    engine.reset();
}

void uci_t::position(const std::string_view args) noexcept {
    auto parts = std::views::split(args, " moves "sv);
    auto part = parts.begin();

    std::string_view position{};
    std::string_view moves{};

    std::string_view position_part {*part++};
    if (position_part == "startpos"sv) {
        position = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"sv;
    } else {
        position_part.remove_prefix("fen "sv.size());
        position = position_part;
    }

    if (part != parts.end()) {
        std::string_view moves_part {*part++};
        moves = moves_part;
    }

    engine.setup(position, moves);
}

void uci_t::go(const std::string_view args) noexcept {
    // engine.search();
}

void uci_t::quit() noexcept {
    std::exit(0);
}

} // namespace chess
