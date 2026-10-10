module;

#include <algorithm>
#include <cassert>
#include <iterator>
#include <span>
#include <immintrin.h>
#include <cstdint>
#include <cstring>

// export module core.position:impl.move;
module core.position;

import :decl;
// import :impl.query;
import attacks.lookup;

namespace chess {

constexpr __v64qu AllSquares = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};

size_t splat_leapers(const square_t from, const squares_t to, const std::span<move_t> buffer) noexcept {
    if (to.empty())
        return 0;
    // assert(count <= 8);  // max 8 attacks
    constexpr square_e placeholder{0};
    const __v8hu vec_from = _mm_set1_epi16(move_t(from, placeholder));
    const __v8hu vec_to = _mm_cvtepi8_epi16(_mm512_castsi512_si128(_mm512_maskz_compress_epi8(to, AllSquares)));
    const __v8hu moves = vec_from | vec_to << 6;// move_t::TO_SQ_SHIFT;
    _mm_storeu_si128(reinterpret_cast<__m128i*>(buffer.data()), moves);
    return to.size();
}

size_t splat_sliders(const square_t from, const squares_t to, const std::span<move_t> buffer) noexcept {
    if (to.empty())
        return 0;
    // assert(count <= 16);  // max 16 attacks
    constexpr square_e placeholder{0};
    const __v16hu vec_from = _mm256_set1_epi16(move_t(from, placeholder));
    const __v16hu vec_to = _mm256_cvtepi8_epi16(_mm512_castsi512_si128(_mm512_maskz_compress_epi8(to, AllSquares)));
    const __v16hu moves = vec_from | vec_to << 6; //move_t::TO_SQ_SHIFT;
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(buffer.data()), moves);
    return to.size();
}

size_t splat_queens(const square_t from, const squares_t to, const std::span<move_t> buffer) noexcept {
    if (to.empty())
        return 0;
    // assert(count <= 32);  // max 32 attacks
    constexpr square_e placeholder{0};
    const __v32hu vec_from = _mm512_set1_epi16(move_t(from, placeholder));
    const __v32hu vec_to = _mm512_cvtepi8_epi16(_mm512_castsi512_si256(_mm512_maskz_compress_epi8(to, AllSquares)));
    const __v32hu moves = vec_from | vec_to << 6; //move_t::TO_SQ_SHIFT;
    _mm512_storeu_si512(reinterpret_cast<__m512i*>(buffer.data()), moves);
    return to.size();
}

size_t splat_pawns(const int16_t offset, const squares_t to, const std::span<move_t> buffer) noexcept {
    if (to.empty())
        return 0;
    // assert(count <= 8);  // max 8 attacks
    const __v8hu vec_to    = _mm_cvtepi8_epi16(_mm512_castsi512_si128(_mm512_maskz_compress_epi8(to, AllSquares)));
    const __v8hu vec_from  = vec_to + offset;
    const __v8hu moves = vec_from | vec_to << 6;
    _mm_storeu_si128(reinterpret_cast<__m128i*>(buffer.data()), moves);
    return to.size();
}

size_t splat_promotions(const int16_t offset, const square_t to, const std::span<move_t> buffer) noexcept {
    constexpr __v8hu vec_promo{Q, R, B, N, 0, 0, 0, 0};
    // assert(from >= 0 && from < MAX_SQUARE);
    // assert(buffer.size() >= 4);
    const __v8hu vec_to = _mm_set1_epi16(to);
    const __v8hu vec_from = vec_to + offset;
    const __v8hu moves = vec_from | vec_to << 6 | vec_promo << 12;
    _mm_storel_epi64(reinterpret_cast<__m128i*>(buffer.data()), moves);
    return 4;
}

// std::span<move_t> position_t::generate_moves(const std::span<move_t, 256> buffer) const noexcept {
//     using namespace attacks::lookup;

//     const side_t side = this->side();
//     const squares_t occupied = by();
//     const squares_t empty = ~occupied;
//     const squares_t enemies = by(~side);
//     const squares_t us = by(side);
//     const square_t ksq = by(side, K).front();
//     const squares_t attacked = attackers(~side);

//     squares_t targets = ~us;
//     size_t count{};

//     count += splat_leapers(ksq, king(ksq) & targets & ~attacked, buffer.subspan(count));

//     if (_checkers.size() > 1)
//         return buffer.first(count);

//     if (_checkers.size() == 1)
//         targets &= line(ksq, _checkers.front());
//     else {
//         if (side == WHITE) {
//             if ((castling() & squares_t{h1}) && !(by() & squares_t{f1, g1}) && !(attacked & squares_t{e1, f1, g1}))
//                 buffer[count++] = {e1, g1};
//             if ((castling() & squares_t{a1}) && !(by() & squares_t{b1, c1, d1}) && !(attacked & squares_t{e1, d1, c1}))
//                 buffer[count++] = {e1, c1};
//         } else {
//             if ((castling() & squares_t{h8}) && !(by() & squares_t{f8, g8}) && !(attacked & squares_t{e8, f8, g8}))
//                 buffer[count++] = {e8, g8};
//             if ((castling() & squares_t{a8}) && !(by() & squares_t{b8, c8, d8}) && !(attacked & squares_t{e8, d8, c8}))
//                 buffer[count++] = {e8, c8};
//         }
//     }

//     for (const square_t from : by(side, N) & ~_pinned)
//         count += splat_leapers(from, knight(from) & targets, buffer.subspan(count));

//     const auto find_valid = [&](const square_t from, squares_t valid) {
//         if (_pinned[from]) {
//             for (const square_t sniper : _snipers) {
//                 const squares_t pin_line = line(ksq, sniper);
//                 if (pin_line[from]) {
//                     valid &= pin_line;
//                     break;
//                 }
//             }
//         }
//         return valid;
//     };

//     for (const square_t from : by(side, B))
//         count += splat_sliders(from, bishop(from, occupied) & find_valid(from, targets), buffer.subspan(count));

//     for (const square_t from : by(side, R))
//         count += splat_sliders(from, rook(from, occupied) & find_valid(from, targets), buffer.subspan(count));

//     for (const square_t from : by(side, Q))
//         count += splat_queens(from, queen(from, occupied) & find_valid(from, targets), buffer.subspan(count));

//     const squares_t pawns = by(side, P) & ~_pinned;

//     if (side == WHITE) {
//         const squares_t push1 = (pawns << 8) & empty & targets;
//         const squares_t push2 = ((pawns & squares_t{_2}) << 8 & empty) << 8 & empty & targets;
//         const squares_t left  = (pawns << 7) & ~squares_t{h} & enemies & targets;
//         const squares_t right = (pawns << 9) & ~squares_t{a} & enemies & targets;
//         count += splat_pawns(-7, left & ~squares_t{_8}, buffer.subspan(count));
//         count += splat_pawns(-9, right & ~squares_t{_8}, buffer.subspan(count));
//         count += splat_pawns(-8, push1 & ~squares_t{_8}, buffer.subspan(count));
//         count += splat_pawns(-16, push2, buffer.subspan(count));

//         for (const square_t to : left & squares_t{_8})
//             count += splat_promotions(-7, to, buffer.subspan(count));

//         for (const square_t to : right & squares_t{_8})
//             count += splat_promotions(-9, to, buffer.subspan(count));

//         for (const square_t to : push1 & squares_t{_8})
//             count += splat_promotions(-8, to, buffer.subspan(count));

//         for (const square_t from : by(side, P) & _pinned) {
//             const squares_t pawns{from};
//             const squares_t valid = find_valid(from, targets);
//             const squares_t push1 = (pawns << 8) & empty;
//             const squares_t push2 = ((pawns & squares_t{_2}) << 8 & empty) << 8 & empty;
//             const squares_t left  = (pawns << 7) & ~squares_t{h} & enemies;
//             const squares_t right = (pawns << 9) & ~squares_t{a} & enemies;
//             const squares_t moves = (push1 | push2 | left | right) & valid;
//             for (const square_t to : moves & ~squares_t{_8})
//                 buffer[count++] = {from, to};
//             for (const square_t to : moves & squares_t{_8})
//                 count += splat_promotions(int(from) - int(to), to, buffer.subspan(count));
//         }

//     } else {
//         const squares_t push1 = (pawns >> 8) & empty & targets;
//         const squares_t push2 = ((pawns & squares_t{_7}) >> 8 & empty) >> 8 & empty & targets;
//         const squares_t left  = (pawns >> 9) & ~squares_t{h} & enemies & targets;
//         const squares_t right = (pawns >> 7) & ~squares_t{a} & enemies & targets;
//         count += splat_pawns(+9, left & ~squares_t{_1}, buffer.subspan(count));
//         count += splat_pawns(+7, right & ~squares_t{_1}, buffer.subspan(count));
//         count += splat_pawns(+8, push1 & ~squares_t{_1}, buffer.subspan(count));
//         count += splat_pawns(+16, push2, buffer.subspan(count));

//         for (const square_t to : left & squares_t{_1})
//             count += splat_promotions(+9, to, buffer.subspan(count));

//         for (const square_t to : right & squares_t{_1})
//             count += splat_promotions(+7, to, buffer.subspan(count));

//         for (const square_t to : push1 & squares_t{_1})
//             count += splat_promotions(+8, to, buffer.subspan(count));

//         for (const square_t from : by(side, P) & _pinned) {
//             const squares_t pawns{from};
//             const squares_t valid = find_valid(from, targets);
//             const squares_t push1 = (pawns >> 8) & empty;
//             const squares_t push2 = ((pawns & squares_t{_7}) >> 8 & empty) >> 8 & empty;
//             const squares_t left  = (pawns >> 9) & ~squares_t{h} & enemies;
//             const squares_t right = (pawns >> 7) & ~squares_t{a} & enemies;
//             const squares_t moves = (push1 | push2 | left | right) & valid;
//             for (const square_t to : moves & ~squares_t{_1})
//                 buffer[count++] = {from, to};
//             for (const square_t to : moves & squares_t{_1})
//                 count += splat_promotions(int(from) - int(to), to, buffer.subspan(count));
//         }
//     }

//     for (const square_t to : en_passant()) {
//         const square_t psq{to.file(), side == WHITE ? _5 : _4};
//         // The checking pawn is captured on psq, not on the destination.
//         if (_checkers.size() == 1 && !targets[to] && !_checkers[psq])
//             continue;
//         for (const square_t from : by(side, P) & pawn(to, ~side)) {
//             if (find_valid(from, squares_t{to}).empty())
//                 continue;
//             const squares_t occ = (occupied & ~squares_t{psq, from}) | squares_t{to};
//             if (!(rook(ksq, occ) & by(~side, R, Q)).empty() || !(bishop(ksq, occ) & by(~side, B, Q)).empty())
//                 continue;
//             buffer[count++] = {from, to};
//         }
//     }

//     return buffer.first(count);
// }

std::span<move_t> position_t::generate_all_moves(const std::span<move_t, 256> buffer) const noexcept {
    return generate_moves(~by(side()), buffer);
}

std::span<move_t> position_t::generate_capture_moves(const std::span<move_t, 256> buffer) const noexcept {
    return generate_moves(by(~side()), buffer);
}

std::span<move_t> position_t::generate_moves(const squares_t filter, const std::span<move_t, 256> buffer) const noexcept {
    using namespace attacks::lookup;

    const side_t side = this->side();
    const squares_t occupied = by();
    const squares_t empty = ~occupied;
    const squares_t enemies = by(~side);
    const squares_t us = by(side);
    const square_t ksq = by(side, K).front();
    const squares_t attacked = attackers(~side);

    squares_t targets = filter;
    size_t count{};

    count += splat_leapers(ksq, king(ksq) & targets & ~attacked, buffer.subspan(count));

    if (_checkers.size() > 1)
        return buffer.first(count);

    if (_checkers.size() == 1)
        targets &= line(ksq, _checkers.front());
    else {
        if (side == WHITE) {
            if ((castling() & squares_t{h1}) && !(by() & squares_t{f1, g1}) && !(attacked & squares_t{e1, f1, g1}))
                buffer[count++] = {e1, g1};
            if ((castling() & squares_t{a1}) && !(by() & squares_t{b1, c1, d1}) && !(attacked & squares_t{e1, d1, c1}))
                buffer[count++] = {e1, c1};
        } else {
            if ((castling() & squares_t{h8}) && !(by() & squares_t{f8, g8}) && !(attacked & squares_t{e8, f8, g8}))
                buffer[count++] = {e8, g8};
            if ((castling() & squares_t{a8}) && !(by() & squares_t{b8, c8, d8}) && !(attacked & squares_t{e8, d8, c8}))
                buffer[count++] = {e8, c8};
        }
    }

    for (const square_t from : by(side, N) & ~_pinned)
        count += splat_leapers(from, knight(from) & targets, buffer.subspan(count));

    const auto find_valid = [&](const square_t from, squares_t valid) {
        if (_pinned[from]) {
            for (const square_t sniper : _snipers) {
                const squares_t pin_line = line(ksq, sniper);
                if (pin_line[from]) {
                    valid &= pin_line;
                    break;
                }
            }
        }
        return valid;
    };

    for (const square_t from : by(side, B))
        count += splat_sliders(from, bishop(from, occupied) & find_valid(from, targets), buffer.subspan(count));

    for (const square_t from : by(side, R))
        count += splat_sliders(from, rook(from, occupied) & find_valid(from, targets), buffer.subspan(count));

    for (const square_t from : by(side, Q))
        count += splat_queens(from, queen(from, occupied) & find_valid(from, targets), buffer.subspan(count));

    const squares_t pawns = by(side, P) & ~_pinned;

    if (side == WHITE) {
        const squares_t push1 = (pawns << 8) & empty & targets;
        const squares_t push2 = ((pawns & squares_t{_2}) << 8 & empty) << 8 & empty & targets;
        const squares_t left  = (pawns << 7) & ~squares_t{h} & enemies & targets;
        const squares_t right = (pawns << 9) & ~squares_t{a} & enemies & targets;
        count += splat_pawns(-7, left & ~squares_t{_8}, buffer.subspan(count));
        count += splat_pawns(-9, right & ~squares_t{_8}, buffer.subspan(count));
        count += splat_pawns(-8, push1 & ~squares_t{_8}, buffer.subspan(count));
        count += splat_pawns(-16, push2, buffer.subspan(count));

        for (const square_t to : left & squares_t{_8})
            count += splat_promotions(-7, to, buffer.subspan(count));

        for (const square_t to : right & squares_t{_8})
            count += splat_promotions(-9, to, buffer.subspan(count));

        for (const square_t to : push1 & squares_t{_8})
            count += splat_promotions(-8, to, buffer.subspan(count));

        for (const square_t from : by(side, P) & _pinned) {
            const squares_t pawns{from};
            const squares_t valid = find_valid(from, targets);
            const squares_t push1 = (pawns << 8) & empty;
            const squares_t push2 = ((pawns & squares_t{_2}) << 8 & empty) << 8 & empty;
            const squares_t left  = (pawns << 7) & ~squares_t{h} & enemies;
            const squares_t right = (pawns << 9) & ~squares_t{a} & enemies;
            const squares_t moves = (push1 | push2 | left | right) & valid;
            // count += splat_leapers(from, moves & ~squares_t{_8}, buffer.subspan(count));
            for (const square_t to : moves & ~squares_t{_8})
                buffer[count++] = {from, to};
            for (const square_t to : moves & squares_t{_8})
                count += splat_promotions(from - to, to, buffer.subspan(count));
        }

    } else {
        const squares_t push1 = (pawns >> 8) & empty & targets;
        const squares_t push2 = ((pawns & squares_t{_7}) >> 8 & empty) >> 8 & empty & targets;
        const squares_t left  = (pawns >> 9) & ~squares_t{h} & enemies & targets;
        const squares_t right = (pawns >> 7) & ~squares_t{a} & enemies & targets;
        count += splat_pawns(+9, left & ~squares_t{_1}, buffer.subspan(count));
        count += splat_pawns(+7, right & ~squares_t{_1}, buffer.subspan(count));
        count += splat_pawns(+8, push1 & ~squares_t{_1}, buffer.subspan(count));
        count += splat_pawns(+16, push2, buffer.subspan(count));

        for (const square_t to : left & squares_t{_1})
            count += splat_promotions(+9, to, buffer.subspan(count));

        for (const square_t to : right & squares_t{_1})
            count += splat_promotions(+7, to, buffer.subspan(count));

        for (const square_t to : push1 & squares_t{_1})
            count += splat_promotions(+8, to, buffer.subspan(count));

        for (const square_t from : by(side, P) & _pinned) {
            const squares_t pawns{from};
            const squares_t valid = find_valid(from, targets);
            const squares_t push1 = (pawns >> 8) & empty;
            const squares_t push2 = ((pawns & squares_t{_7}) >> 8 & empty) >> 8 & empty;
            const squares_t left  = (pawns >> 9) & ~squares_t{h} & enemies;
            const squares_t right = (pawns >> 7) & ~squares_t{a} & enemies;
            const squares_t moves = (push1 | push2 | left | right) & valid;
            // count += splat_leapers(from, moves & ~squares_t{_1}, buffer.subspan(count));
            for (const square_t to : moves & ~squares_t{_1})
                buffer[count++] = {from, to};
            for (const square_t to : moves & squares_t{_1})
                count += splat_promotions(from - to, to, buffer.subspan(count));
        }
    }

    for (const square_t to : en_passant()) {
        const square_t psq{to.file(), side == WHITE ? _5 : _4};
        // The checking pawn is captured on psq, not on the destination.
        if (_checkers.size() == 1 && !targets[to] && !_checkers[psq])
            continue;
        for (const square_t from : by(side, P) & pawn(to, ~side)) {
            if (find_valid(from, squares_t{to}).empty())
                continue;
            const squares_t occ = (occupied & ~squares_t{psq, from}) | squares_t{to};
            if (!(rook(ksq, occ) & by(~side, R, Q)).empty() || !(bishop(ksq, occ) & by(~side, B, Q)).empty())
                continue;
            buffer[count++] = {from, to};
        }
    }

    return buffer.first(count);
}

} // namespace chess
