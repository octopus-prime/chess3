import pathlib
import queue
import re
import subprocess
import sys
import threading
import time
import unittest


ENGINE = pathlib.Path(sys.argv.pop(1)).resolve()


class ProtocolTests(unittest.TestCase):
    def setUp(self):
        self.process = subprocess.Popen(
            [str(ENGINE)], cwd=ENGINE.parent, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1,
        )
        self.lines = queue.Queue()
        self.transcript = []

        def read():
            for line in self.process.stdout:
                self.lines.put(line.strip())
            self.lines.put(None)

        self.reader = threading.Thread(target=read)
        self.reader.start()
        self.send("uci")
        self.wait("uciok")
        self.send("isready")
        self.wait("readyok")

    def tearDown(self):
        if self.process.poll() is None:
            self.send("quit")
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
                self.fail("quit failed to stop engine")
        self.reader.join(timeout=3)
        self.process.stdin.close()
        self.process.stdout.close()
        errors = self.process.stderr.read()
        self.process.stderr.close()
        self.assertEqual(self.process.returncode, 0, errors)

    def send(self, command):
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def wait(self, prefix, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                line = self.lines.get(timeout=max(0.001, deadline - time.monotonic()))
            except queue.Empty:
                break
            self.transcript.append(line)
            if line is None:
                self.fail(f"engine exited while waiting for {prefix}: {self.transcript}")
            if line.startswith(prefix):
                return line
        self.fail(f"timeout waiting for {prefix}: {self.transcript}")

    def bestmove(self):
        line = self.wait("bestmove ")
        self.assertRegex(line, r"^bestmove ([a-h][1-8][a-h][1-8][qrbn]?|0000)$")
        return line.split()[1]

    def test_stop_ready_and_restart(self):
        self.send("position startpos")
        self.send("go infinite")
        self.wait("info depth ")
        self.send("isready")
        self.wait("readyok", 1)
        self.assertFalse(any(line.startswith("bestmove ") for line in self.transcript))
        start = time.monotonic()
        self.send("stop")
        self.assertNotEqual(self.bestmove(), "0000")
        self.assertLess(time.monotonic() - start, 1)
        self.send("position startpos moves e2e4 e7e5")
        self.send("go depth 2")
        self.assertNotEqual(self.bestmove(), "0000")

    def test_limits_and_argument_order(self):
        for command in (
            "go nodes 200 depth 40",
            "go binc 2 btime 5000 movestogo 20 winc 2 wtime 300",
            "go movetime 60",
            "go movetime 0",
            "go depth 2",
        ):
            self.send("ucinewgame")
            self.send("position startpos")
            start = time.monotonic()
            self.send(command)
            self.assertNotEqual(self.bestmove(), "0000")
            self.assertLess(time.monotonic() - start, 2)
        counts = [int(re.search(r" nodes (\d+)", line)[1]) for line in self.transcript
                  if line and line.startswith("info depth ") and " nodes " in line]
        self.assertTrue(counts)
        self.assertLessEqual(counts[0], 200)
        self.send("position startpos moves e2e4")
        self.send("go winc 0 wtime 5000 binc 0 btime 100 movestogo 20")
        self.assertNotEqual(self.bestmove(), "0000")

    def test_immediate_stop_is_not_lost(self):
        self.send("position startpos")
        for count in range(1, 6):
            self.send("go infinite")
            self.send("stop")
            self.assertNotEqual(self.bestmove(), "0000")
            self.send("isready")
            self.wait("readyok")
            self.assertEqual(sum(line.startswith("bestmove ") for line in self.transcript), count)

    def test_short_tournament_clocks(self):
        for moves in ("", " moves e2e4"):
            for remaining in (1000, 100, 20, 1, 0):
                self.send("position startpos" + moves)
                start = time.monotonic()
                self.send(f"go wtime {remaining} btime {remaining} movestogo 40")
                self.assertNotEqual(self.bestmove(), "0000")
                self.assertLess(time.monotonic() - start, 0.15)

    def test_period_budget_with_communication_delay(self):
        self.send("setoption name Move Overhead value 20")
        self.send("position startpos")
        remaining = 1000.0
        for moves in range(40, 0, -1):
            start = time.monotonic()
            self.send(f"go wtime {int(remaining)} btime {int(remaining)} movestogo {moves}")
            self.assertNotEqual(self.bestmove(), "0000")
            time.sleep(0.010)
            remaining -= (time.monotonic() - start) * 1000
            self.assertGreater(remaining, 0, f"clock exhausted with {moves - 1} moves remaining")

    def test_invalid_input_and_options(self):
        for command in (
            "go depth", "go depth -1", "go nodes 0", "go movestogo 0",
            "go wtime bad", "go searchmoves e2e4",
            "position fen broken", "position startpos moves e2e5",
            "position fen 4k3/8/8/8/8/8/8/4K3 w K - 0 1",
            "position fen 4k3/8/8/8/8/8/8/4K3 w - d6 0 1",
            "setoption name Move Overhead value -1",
        ):
            self.send(command)
            self.wait("info string ")
        self.send("setoption name Move Overhead value 5")
        self.send("position startpos")
        self.send("go movetime 40")
        self.assertNotEqual(self.bestmove(), "0000")

    def test_terminal_position(self):
        self.send("position fen 7k/6Q1/5K2/8/8/8/8/8 b - - 0 1")
        self.send("go depth 2")
        self.assertEqual(self.bestmove(), "0000")

    def test_infinite_waits_even_at_terminal_position(self):
        self.send("position fen 7k/6Q1/5K2/8/8/8/8/8 b - - 0 1")
        self.send("go infinite")
        self.wait("info depth 254 ")
        self.send("isready")
        self.wait("readyok")
        self.assertFalse(any(line.startswith("bestmove ") for line in self.transcript))
        self.send("stop")
        self.assertEqual(self.bestmove(), "0000")

    def test_quit_during_search(self):
        self.send("go infinite")
        self.wait("info depth ")
        self.send("quit")
        self.process.wait(timeout=2)

    def test_eof_during_search(self):
        self.send("go infinite")
        self.wait("info depth ")
        self.process.stdin.close()
        self.process.wait(timeout=2)


if __name__ == "__main__":
    unittest.main()
