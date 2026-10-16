module;

#include <cstdint>
#include <format>
#include <memory>
#include <span>
#include <string_view>
#include <iostream>
#include <ranges>
#include <chrono>

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
    while (stream.getline(buffer.data(), buffer.size())) {
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
            return;
    }
}

void uci_t::uci() noexcept {
    constexpr std::string_view buffer = "id name chess3\nid author me\nuciok\n";
    std::cout << buffer << std::flush;
}

void uci_t::isready() noexcept {
    constexpr std::string_view buffer = "readyok\n";
    std::cout << buffer << std::flush;
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
    auto parts = std::views::split(args, ' ');
    auto part = parts.begin();

    if (std::string_view(*part).starts_with("depth"sv)) {
        part++; //depth
        std::string_view depth_part {*part++};
        ply_t depth;
        std::from_chars(&*depth_part.begin(), &*depth_part.end(), depth);

        engine(depth, std::chrono::hours(24));
        engine.clear();
        return;
    }

    part++; //wtime
    std::string_view wtime_part {*part++};
    std::int64_t wtime;
    std::from_chars(&*wtime_part.begin(), &*wtime_part.end(), wtime);

    part++; //btime
    std::string_view btime_part {*part++};
    std::int64_t btime;
    std::from_chars(&*btime_part.begin(), &*btime_part.end(), btime);

    part++; //winc
    std::string_view winc_part {*part++};
    std::int64_t winc;
    std::from_chars(&*winc_part.begin(), &*winc_part.end(), winc);

    part++; //binc
    std::string_view binc_part {*part++};
    std::int64_t binc;
    std::from_chars(&*binc_part.begin(), &*binc_part.end(), binc);

    auto timeleft = (engine.side() == WHITE) ? wtime : btime;
    auto timeinc = (engine.side() == WHITE) ? winc : binc;
    auto timetogo = timeinc;

    if (part != parts.end()) {
        part++; //movestogo
        std::string_view movestogo_part {*part++};
        std::int64_t movestogo;
        std::from_chars(&*movestogo_part.begin(), &*movestogo_part.end(), movestogo);
        timetogo += timeleft / (movestogo + 1);
    } else {
        timetogo += timeleft * 4 / 100;
    }

    engine(MAX_PLY - 2, std::chrono::milliseconds(timetogo));
    engine.clear();
}

} // namespace chess
