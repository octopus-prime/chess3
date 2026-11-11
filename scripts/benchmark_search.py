#!/usr/bin/env python3
"""Compare deterministic fixed-depth search results and throughput."""

import argparse
import pathlib
import re
import selectors
import subprocess
import time


POSITIONS = {
    "startpos": "startpos",
    "kiwipete": "fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "promotion": "fen r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
}
INFO = re.compile(
    r"info depth (\d+) .*?score (cp|mate) ([+-]?\d+) nodes (\d+) nps (\d+) "
    r".*?time (\d+) pv (.*)"
)


def search(binary, position, depth):
    cwd = pathlib.Path(__file__).resolve().parents[1] / "src"
    with subprocess.Popen(
        [str(binary)], cwd=cwd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, bufsize=0,
    ) as process:
        try:
            process.stdin.write(f"position {position}\ngo depth {depth}\n".encode())
            result = None
            pending = b""
            deadline = time.monotonic() + 120
            with selectors.DefaultSelector() as selector:
                selector.register(process.stdout, selectors.EVENT_READ)
                done = False
                while not done:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0 or not selector.select(remaining):
                        raise TimeoutError(f"{binary}: search timed out")
                    chunk = process.stdout.read(4096)
                    if not chunk:
                        raise RuntimeError(f"{binary}: exited before bestmove")
                    pending += chunk
                    while b"\n" in pending:
                        line, pending = pending.split(b"\n", 1)
                        line = line.decode()
                        match = INFO.fullmatch(line.strip())
                        if match and int(match[1]) == depth:
                            result = {
                                "score": (match[2], int(match[3])),
                                "nodes": int(match[4]),
                                "nps": int(match[5]),
                                "pv": match[7].strip(),
                            }
                        if line.startswith("bestmove "):
                            if result is None:
                                raise RuntimeError(f"{binary}: missing depth-{depth} result")
                            result["bestmove"] = line.strip()
                            done = True
            process.stdin.write(b"quit\n")
            if process.wait(timeout=10) != 0:
                raise RuntimeError(f"{binary}: unsuccessful exit")
            if result["nps"] <= 0:
                raise RuntimeError(f"{binary}: invalid throughput")
            return result
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=pathlib.Path)
    parser.add_argument("candidate", type=pathlib.Path)
    parser.add_argument("--depth", type=int, default=18)
    parser.add_argument("--rounds", type=int, default=8)
    parser.add_argument("--allow-search-changes", action="store_true",
                        help="report changed search results instead of requiring exact equivalence")
    args = parser.parse_args()
    if not 1 <= args.depth <= 255 or args.rounds < 1:
        parser.error("depth must be 1..255 and rounds must be positive")
    binaries = [args.baseline.resolve(strict=True), args.candidate.resolve(strict=True)]
    totals = {name: [[0, 0.0], [0, 0.0]] for name in POSITIONS}
    for round_index in range(args.rounds):
        for name, position in POSITIONS.items():
            results = {}
            for mode in ([0, 1] if round_index % 2 == 0 else [1, 0]):
                row = search(binaries[mode], position, args.depth)
                results[mode] = row
                totals[name][mode][0] += row["nodes"]
                totals[name][mode][1] += row["nodes"] / row["nps"]
                print(f"{round_index + 1} {name} {'baseline' if mode == 0 else 'candidate'} "
                      f"{row['nps']} NPS ({row['nodes']} nodes)", flush=True)
            for key in ("score", "nodes", "pv", "bestmove"):
                if results[0][key] != results[1][key]:
                    if not args.allow_search_changes:
                        raise RuntimeError(f"{name}: {key} diverged: {results}")
                    print(f"{name}: {key}: {results[0][key]} -> {results[1][key]}", flush=True)
    overall = [[0, 0.0], [0, 0.0]]
    for name, modes in totals.items():
        rates = [nodes / seconds for nodes, seconds in modes]
        print(f"{name}: {rates[0]:.0f} -> {rates[1]:.0f} NPS "
              f"({(rates[1] / rates[0] - 1) * 100:+.2f}%)")
        if args.allow_search_changes:
            print(f"  nodes: {modes[0][0]} -> {modes[1][0]} "
                  f"({(modes[1][0] / modes[0][0] - 1) * 100:+.2f}%); "
                  f"time: {modes[0][1]:.3f} -> {modes[1][1]:.3f} s "
                  f"({(modes[1][1] / modes[0][1] - 1) * 100:+.2f}%)")
        for mode in (0, 1):
            overall[mode][0] += modes[mode][0]
            overall[mode][1] += modes[mode][1]
    rates = [nodes / seconds for nodes, seconds in overall]
    print(f"Overall: {rates[0]:.0f} -> {rates[1]:.0f} NPS "
          f"({(rates[1] / rates[0] - 1) * 100:+.2f}%)")
    if args.allow_search_changes:
        print(f"Total nodes: {overall[0][0]} -> {overall[1][0]} "
              f"({(overall[1][0] / overall[0][0] - 1) * 100:+.2f}%)")
        print(f"Total search time: {overall[0][1]:.3f} -> {overall[1][1]:.3f} s "
              f"({(overall[1][1] / overall[0][1] - 1) * 100:+.2f}%)")


if __name__ == "__main__":
    main()
