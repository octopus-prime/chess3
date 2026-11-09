#!/usr/bin/env python3
"""Regenerate SFNNv20 fixtures using an independently built Stockfish oracle."""

import argparse
import pathlib
import random
import re
import struct
import subprocess
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[2]
HEADER = ROOT / "test/nnue/reference_positions.hpp"


def stack_positions():
    randomizer = random.Random(20261109)
    for raw in range(192):
        band, queens, layout, many = raw // 24, raw // 6 % 4, raw // 2 % 3, raw % 2
        found = False
        for count in range(max(2, band * 4 + 1), band * 4 + 5):
            for pawn_count in ([0] if layout == 2 else range(2 if layout == 0 else 1, 17)):
                officer_count = count - 2 - pawn_count
                for white in range(max(0, officer_count) + 1):
                    black = officer_count - white
                    if min(white, black) < 0 or int(max(white, black) >= 2) != many:
                        continue
                    if white < bool(queens & 2) or black < bool(queens & 1):
                        continue
                    board = {0: "K", 63: "k"}
                    pawn_squares = [s for s in range(8, 56) if layout == 0 or s % 8 < 4]
                    if layout == 0:
                        endpoints = [
                            randomizer.choice([s for s in pawn_squares if s % 8 < 4]),
                            randomizer.choice([s for s in pawn_squares if s % 8 >= 4]),
                        ]
                        pawn_squares = endpoints + randomizer.sample(
                            [s for s in pawn_squares if s not in endpoints], pawn_count - 2
                        )
                    else:
                        pawn_squares = randomizer.sample(pawn_squares, pawn_count)
                    for i, square in enumerate(pawn_squares):
                        board[square] = "P" if i % 2 else "p"
                    remaining = randomizer.sample(
                        [s for s in range(64) if s not in board], officer_count
                    )
                    pieces = (
                        ("Q" if queens & 2 else "") + "N" * (white - bool(queens & 2))
                        + ("q" if queens & 1 else "") + "n" * (black - bool(queens & 1))
                    )
                    board.update(zip(remaining, pieces, strict=True))
                    ranks = []
                    for rank in range(7, -1, -1):
                        text = "".join(board.get(rank * 8 + file, ".") for file in range(8))
                        ranks.append(re.sub(r"\.+", lambda m: str(len(m[0])), text))
                    yield "/".join(ranks) + " w - - 0 1"
                    found = True
                    break
                if found:
                    break
            if found:
                break


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stockfish", type=pathlib.Path, help="Stockfish source checkout")
    args = parser.parse_args()
    network = ROOT / "nn-18cd32e11a9c.nnue"
    with network.open("rb") as stream:
        _, _, description_size = struct.unpack("<III", stream.read(12))
        stream.seek(description_size, 1)
        _, raw_count, stored = struct.unpack("<III", stream.read(12))
        fallbacks = struct.unpack(f"<{raw_count}H", stream.read(2 * raw_count))
    walk_header = HEADER.read_text().split("inline constexpr std::array stack_cases")[0]
    cases = re.findall(r'case_t\{"([^"]+)"', walk_header)
    start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    if start not in cases:
        cases.insert(0, start)
    extra = list(stack_positions())
    checkout = args.stockfish.resolve(strict=True)
    source = checkout / "src"
    sources = (
        [path for path in sorted(source.glob("*.cpp")) if path.name != "main.cpp"]
        + sorted((source / "nnue").glob("*.cpp"))
        + sorted((source / "nnue/features").glob("*.cpp"))
        + [source / "syzygy/tbprobe.cpp"]
    )
    revision = subprocess.check_output(
        ["git", "-C", str(checkout), "rev-parse", "--short=8", "HEAD"], text=True
    ).strip()
    with tempfile.TemporaryDirectory(prefix="chess3-stockfish-reference-") as directory:
        oracle = pathlib.Path(directory) / "reference"
        subprocess.run(
            ["clang++", "-std=c++17", "-O2", "-fconstexpr-steps=500000000", "-DNDEBUG",
             "-DNNUE_EMBEDDING_OFF", "-pthread", "-I" + str(source),
             str(ROOT / "test/nnue/stockfish_reference.cpp"), *map(str, sources),
             "-o", str(oracle)],
            check=True,
        )
        output = subprocess.run(
            [str(oracle), str(network)], input="\n".join(cases + extra) + "\n",
            text=True, capture_output=True, check=True,
        )
    rows = [tuple(map(int, row.split())) for row in output.stdout.splitlines()]
    if len(rows) != len(cases) + len(extra):
        raise RuntimeError("Stockfish oracle did not return one result per position")
    if {row[1] for row in rows} != set(range(stored)):
        raise RuntimeError("generated positions do not cover every stored stack")
    text = (
        "#pragma once\n\n#include <array>\n#include <cstddef>\n#include <cstdint>\n#include <string_view>\n\n"
        f"// Generated from Stockfish {revision}, SFNNv20, nn-18cd32e11a9c.nnue.\n"
        "// Scores are raw Network::evaluate(), without evaluation post-processing.\n"
        "namespace nnue_reference {\n\nstruct case_t {\n    std::string_view fen;\n"
        "    int score;\n    std::array<uint64_t, 2> feature_hashes;\n"
        "    size_t stack;\n};\n\n"
    )
    for name, fens, results in (
        ("cases", cases, rows[:len(cases)]), ("stack_cases", extra, rows[len(cases):])
    ):
        text += f"inline constexpr std::array {name}{{\n"
        for fen, (score, stack, white, black) in zip(fens, results, strict=True):
            text += f'    case_t{{"{fen}", {score}, {{{white}ULL, {black}ULL}}, {stack}}},\n'
        text += "};\n\n"
    text += f"inline constexpr size_t stored_stacks = {stored};\n"
    text += "inline constexpr std::array<uint16_t, 192> fallbacks{\n"
    for offset in range(0, raw_count, 24):
        text += "    " + ", ".join(map(str, fallbacks[offset:offset + 24])) + ",\n"
    text += "};\n\n}\n"
    HEADER.write_text(text)
    print(f"Generated {len(cases)} walk fixtures and {len(extra)} stack fixtures; "
          f"all {stored} stored stacks covered.")


if __name__ == "__main__":
    main()
