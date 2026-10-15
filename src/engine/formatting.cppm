module;

#include <format>
#include <span>

export module engine.formatting;

import types.file;
import types.rank;
import types.square;
import types.type;
import types.score;
import core.move;

export template <>
struct std::formatter<chess::file_t, char> {
    template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <class FormatContext>
    auto format(chess::file_t const& value, FormatContext& ctx) const { return format_to(ctx.out(), "{:c}", 'a' + value); }
};

export template <>
struct std::formatter<chess::rank_t, char> {
    template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <class FormatContext>
    auto format(chess::rank_t const& value, FormatContext& ctx) const { return format_to(ctx.out(), "{:c}", '1' + value); }
};

export template <>
struct std::formatter<chess::square_t, char> {
    template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <class FormatContext>
    auto format(chess::square_t const& value, FormatContext& ctx) const { return format_to(ctx.out(), "{}{}", value.file(), value.rank()); }
};

export template <>
struct std::formatter<chess::type_t, char> {
    template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <class FormatContext>
    auto format(chess::type_t const& value, FormatContext& ctx) const {
        static constexpr std::string_view chars[chess::MAX_TYPE] = {"", "p", "n", "b", "r", "q", "k", ""};
        return format_to(ctx.out(), "{}", chars[value]);
    }
};

export template <>
struct std::formatter<chess::score_t, char> {
    template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <class FormatContext>
    auto format(chess::score_t const& value, FormatContext& ctx) const {
        if (value.decisive()) {
            const int dist = value.win() ? chess::MAX - value + 1 : chess::MIN - value;
            return format_to(ctx.out(), "mate {:+}", dist / 2);
        }
        else
            return format_to(ctx.out(), "cp {:+}", int(value));
    }
};

export template <>
struct std::formatter<chess::move_t, char> {
    template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <class FormatContext>
    auto format(chess::move_t const& value, FormatContext& ctx) const {
        return format_to(ctx.out(), "{}{}{}", value.from(), value.to(), value.promotion());
    }
};

export template<typename T>
struct std::formatter<std::span<T>> {
    template <class ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <class FormatContext>
    auto format(std::span<T> span, FormatContext& ctx) const {
        for (size_t i = 0; i < span.size(); ++i) {
            if (i > 0) std::format_to(ctx.out(), "{}", ' ');
            std::format_to(ctx.out(), "{}", span[i]);
        }
        return ctx.out();
    }
};
