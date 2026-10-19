module;

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <syncstream>
#include <thread>

module engine.uci;

import core.position;

namespace chess {

namespace {

constexpr std::string_view startpos =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

std::string_view token(std::string_view& text) noexcept {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == text.npos) {
        text = {};
        return {};
    }
    text.remove_prefix(first);
    const auto end = text.find_first_of(" \t\r");
    const auto result = text.substr(0, end);
    text.remove_prefix(end == text.npos ? text.size() : end);
    return result;
}

bool number(std::string_view text, int64_t& value) noexcept {
    if (text.empty())
        return false;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size() &&
        value >= 0;
}

bool valid_fen(std::string_view fen) noexcept {
    const auto board = token(fen);
    int rank = 7, file = 0, pieces = 0, pawns = 0, white_king = 0, black_king = 0;
    for (const char piece : board) {
        if (piece == '/') {
            if (file != 8 || rank == 0)
                return false;
            --rank;
            file = 0;
        } else if (piece >= '1' && piece <= '8')
            file += piece - '0';
        else {
            if (std::string_view{"PNBRQKpnbrqk"}.find(piece) == std::string_view::npos)
                return false;
            ++file;
            ++pieces;
            white_king += piece == 'K';
            black_king += piece == 'k';
            if (piece == 'P' || piece == 'p') {
                if (rank == 0 || rank == 7)
                    return false;
                ++pawns;
            }
        }
        if (file > 8)
            return false;
    }
    const auto side = token(fen);
    const auto castling = token(fen);
    const auto ep = token(fen);
    int64_t halfmove, fullmove;
    return rank == 0 && file == 8 && pieces <= 32 && pawns <= 16 &&
        white_king == 1 && black_king == 1 && (side == "w" || side == "b") &&
        !castling.empty() && (castling == "-" || castling.find_first_not_of("KQkq") == castling.npos) &&
        (ep == "-" || (ep.size() == 2 && ep[0] >= 'a' && ep[0] <= 'h' &&
                       ep[1] == (side == "w" ? '6' : '3'))) &&
        number(token(fen), halfmove) && halfmove <= 255 &&
        number(token(fen), fullmove) && fullmove > 0;
}

void error(const std::string_view message) {
    std::osyncstream{std::cout} << "info string " << message << '\n' << std::flush;
}

}

uci_t::uci_t() : engine{startpos} {}

void uci_t::stop() noexcept {
    if (worker.joinable()) {
        worker.request_stop();
        worker.join();
    }
}

void uci_t::operator()(std::istream& stream) noexcept {
    std::string line;
    while (std::getline(stream, line)) {
        std::string_view args{line};
        const auto command = token(args);
        if (command == "ucinewgame") {
            stop();
            ucinewgame();
        } else if (command == "uci")
            uci();
        else if (command == "isready")
            isready();
        else if (command == "position") {
            stop();
            position(args);
        } else if (command == "go") {
            const auto received = std::chrono::steady_clock::now();
            stop();
            go(args, received);
        } else if (command == "stop")
            stop();
        else if (command == "setoption") {
            stop();
            setoption(args);
        } else if (command == "quit")
            break;
    }
    stop();
}

void uci_t::uci() noexcept {
    std::osyncstream{std::cout}
        << "id name chess3\nid author me\n"
           "option name Move Overhead type spin default 20 min 0 max 5000\nuciok\n"
        << std::flush;
}

void uci_t::isready() noexcept {
    std::osyncstream{std::cout} << "readyok\n" << std::flush;
}

void uci_t::ucinewgame() noexcept {
    engine.reset();
}

void uci_t::setoption(std::string_view args) noexcept {
    if (token(args) != "name" || token(args) != "Move" ||
        token(args) != "Overhead" || token(args) != "value") {
        error("unsupported option");
        return;
    }
    int64_t value;
    if (!number(token(args), value) || value > 5000 || !token(args).empty()) {
        error("invalid Move Overhead");
        return;
    }
    move_overhead = static_cast<int>(value);
}

void uci_t::position(std::string_view args) noexcept {
    const auto kind = token(args);
    std::string fen;
    if (kind == "startpos")
        fen = startpos;
    else if (kind == "fen") {
        for (int i = 0; i < 6; ++i) {
            const auto field = token(args);
            if (field.empty()) {
                error("position fen requires six fields");
                return;
            }
            if (i)
                fen += ' ';
            fen += field;
        }
    } else {
        error("position requires startpos or fen");
        return;
    }
    if (!valid_fen(fen)) {
        error("invalid position fen");
        return;
    }
    // Validate move strings and legality before changing the engine's position.
    position_t position{fen};
    for (const auto square : position.castling()) {
        const side_t side = square.rank() == _1 ? WHITE : BLACK;
        const auto rook = position.at(square);
        const auto king = position.at(square_t{e, square.rank()});
        if (rook.type() != R || rook.side() != side || king.type() != K || king.side() != side) {
            error("invalid position castling rights");
            return;
        }
    }
    if (!position.en_passant().empty()) {
        const auto target = position.en_passant().front();
        const auto victim = position.at(square_t{
            target.file(), position.side() == WHITE ? _5 : _4});
        if (position.at(target).type() != NO_TYPE ||
            victim.type() != P || victim.side() == position.side()) {
            error("invalid position en passant");
            return;
        }
    }
    if (!(position.attackers_of(position.by(~position.side(), K).front()) &
          position.by(position.side())).empty()) {
        error("invalid position: non-moving king is in check");
        return;
    }
    std::string moves;
    const auto marker = token(args);
    if (!marker.empty() && marker != "moves") {
        error("expected moves after position");
        return;
    }
    for (auto text = token(args); !text.empty(); text = token(args)) {
        if ((text.size() != 4 && text.size() != 5) ||
            text[0] < 'a' || text[0] > 'h' || text[2] < 'a' || text[2] > 'h' ||
            text[1] < '1' || text[1] > '8' || text[3] < '1' || text[3] > '8' ||
            (text.size() == 5 && std::string_view{"qrbn"}.find(text[4]) == text.npos)) {
            error("invalid position move");
            return;
        }
        const move_t move{text};
        std::array<move_t, 256> buffer;
        const auto legal = position.generate_all_moves(buffer);
        if (std::ranges::find(legal, move) == legal.end()) {
            error("illegal position move");
            return;
        }
        position.do_move(move);
        if (!moves.empty())
            moves += ' ';
        moves += text;
    }
    engine.setup(fen, moves);
}

void uci_t::go(std::string_view args, const std::chrono::steady_clock::time_point received) noexcept {
    std::optional<int64_t> wtime, btime, movetime;
    int64_t winc = 0, binc = 0, movestogo = 25;
    int64_t depth = MAX_PLY - 2;
    size_t nodes = std::numeric_limits<size_t>::max();
    bool infinite = false;
    for (auto key = token(args); !key.empty(); key = token(args)) {
        if (key == "infinite") {
            infinite = true;
            continue;
        }
        const auto text = token(args);
        int64_t value;
        if (!number(text, value)) {
            error("invalid or missing go limit");
            return;
        }
        if ((key == "wtime" || key == "btime" || key == "winc" ||
             key == "binc" || key == "movetime") && value > 86400000) {
            error("go time exceeds 24 hours");
            return;
        }
        if (key == "wtime") wtime = value;
        else if (key == "btime") btime = value;
        else if (key == "winc") winc = value;
        else if (key == "binc") binc = value;
        else if (key == "movetime") movetime = value;
        else if (key == "movestogo" && value > 0) movestogo = value;
        else if (key == "depth" && value > 0 && value < MAX_PLY) depth = value;
        else if (key == "nodes" && value > 0) nodes = static_cast<size_t>(value);
        else {
            error("unsupported go limit or out-of-range value");
            return;
        }
    }
    using namespace std::chrono;
    auto hard = duration_cast<milliseconds>(hours{24});
    auto soft = hard;
    if (!infinite) {
        if (movetime) {
            hard = milliseconds{std::max(int64_t{0}, *movetime - move_overhead)};
            soft = hard;
        } else if (const auto remaining = engine.side() == WHITE ? wtime : btime; remaining) {
            const auto available = std::max(int64_t{0}, *remaining - move_overhead);
            const auto increment = engine.side() == WHITE ? winc : binc;
            const auto budget = std::min(available, available / movestogo + increment * 3 / 4);
            soft = milliseconds{budget};
            hard = milliseconds{std::min(available, budget * 3)};
        }
    }
    worker = std::jthread{[this, depth, hard, soft, nodes, infinite, received](std::stop_token stop) {
        engine.run(static_cast<ply_t>(depth), hard, soft, nodes, stop, infinite, received);
        engine.clear();
    }};
}

} // namespace chess
