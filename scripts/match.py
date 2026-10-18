#!/usr/bin/env python3
"""Run paired fastchess matches with frozen binaries and reproducibility metadata."""

import argparse
import datetime
import hashlib
import json
import pathlib
import re
import shutil
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]


def positive(text):
    value = int(text)
    if value <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return value


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True, type=pathlib.Path)
    parser.add_argument("--candidate", required=True, type=pathlib.Path)
    parser.add_argument("--engine-dir", type=pathlib.Path, default=ROOT / "build",
                        help="working directory for both engines; network must be in its parent")
    parser.add_argument("--openings", type=pathlib.Path, default=ROOT / "test/matches/smoke.epd")
    parser.add_argument("--rounds", type=positive, default=100, help="opening pairs (two games each)")
    parser.add_argument("--tc", default="10+0.1")
    parser.add_argument("--concurrency", type=positive, default=1)
    parser.add_argument("--seed", type=int, default=20261018)
    parser.add_argument("--sprt", action="store_true", help="SPRT [0, 5] normalized Elo, 5%% error rates")
    parser.add_argument("--output", type=pathlib.Path,
                        default=ROOT / "matches" / datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
    args = parser.parse_args()
    runner = shutil.which("fastchess")
    if not runner:
        parser.error("fastchess is not on PATH")
    engine_dir = args.engine_dir.resolve(strict=True)
    network = engine_dir.parent / "nn-c9e480e92425.nnue"
    if not network.is_file():
        parser.error(f"missing network: {network}")
    for path in (args.baseline, args.candidate, args.openings):
        if not path.is_file():
            parser.error(f"missing input: {path}")
    if args.openings.suffix not in (".epd", ".pgn"):
        parser.error("opening suite must have .epd or .pgn extension")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    for name, source in (("baseline", args.baseline), ("candidate", args.candidate)):
        shutil.copy2(source, output / name)
    openings = output / ("openings" + args.openings.suffix)
    shutil.copy2(args.openings, openings)
    command = [
        runner,
        "-config", "discard=true", f"outname={output / 'resume.json'}",
        "-engine", f"cmd={output / 'baseline'}", "name=baseline", f"dir={engine_dir}",
        "-engine", f"cmd={output / 'candidate'}", "name=candidate", f"dir={engine_dir}",
        "-each", "proto=uci", f"tc={args.tc}",
        "-openings", f"file={openings}", f"format={openings.suffix.removeprefix('.')}",
        "order=random",
        "-srand", str(args.seed), "-rounds", str(args.rounds), "-games", "2",
        "-concurrency", str(args.concurrency),
        "-pgnout", f"file={output / 'games.pgn'}", "append=false", "nodes=true",
        "-ratinginterval", "2",
    ]
    if args.sprt:
        command += ["-sprt", "elo0=0", "elo1=5", "alpha=0.05", "beta=0.05", "model=normalized"]
    metadata = {
        "command": command,
        "fastchess": subprocess.check_output([runner, "--version"], text=True).strip(),
        "revision": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "worktree": subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True),
        "network_sha256": digest(network),
        "baseline_sha256": digest(output / "baseline"),
        "candidate_sha256": digest(output / "candidate"),
        "openings_sha256": digest(openings),
        "engine_dir": str(engine_dir),
        "purpose": "smoke" if args.openings.resolve() == (ROOT / "test/matches/smoke.epd") else "experiment",
    }
    (output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"Results: {output}", flush=True)
    if metadata["purpose"] == "smoke":
        print("Bundled openings are for protocol smoke tests, not a strength conclusion.", flush=True)
    protocol_warnings = set()
    with (output / "match.log").open("w") as log:
        process = subprocess.Popen(command, cwd=output, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, bufsize=1)
        try:
            for line in process.stdout:
                if line.startswith("Warning;"):
                    protocol_warnings.add(line.strip())
                print(line, end="", flush=True)
                log.write(line)
                log.flush()
            result = process.wait()
        except KeyboardInterrupt:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            return 130
        finally:
            process.stdout.close()
    pgn = output / "games.pgn"
    games = pgn.read_text() if pgn.exists() else ""
    results = re.findall(r'^\[Result "([^"]+)"\]', games, re.MULTILINE)
    terminations = re.findall(r'^\[Termination "([^"]+)"\]', games, re.MULTILINE)
    abnormal = [reason for reason in terminations if reason != "normal"]
    complete = bool(results) and all(score in ("1-0", "0-1", "1/2-1/2") for score in results)
    if not args.sprt:
        complete &= len(results) == args.rounds * 2
    if result == 0 and (not complete or abnormal or protocol_warnings or len(terminations) != len(results)):
        print(f"Match validation failed: {len(results)} games, abnormal terminations: {abnormal}",
              file=sys.stderr)
        result = 1
    metadata["games"] = len(results)
    metadata["abnormal_terminations"] = abnormal
    metadata["protocol_warnings"] = sorted(protocol_warnings)
    metadata["exit_code"] = result
    (output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    return result


if __name__ == "__main__":
    sys.exit(main())
