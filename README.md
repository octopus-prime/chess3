# Chess

A C++26 project using C++ modules, split into four static libraries and three applications:

- `chess_types`: coordinate, square-set, piece, side, type, and score modules;
  independent of Core and Engine.
- `chess_core`: positions, moves, attacks, hashes, and perft; links publicly
  to `chess_types`.
  [core/main.cpp](src/core/main.cpp) builds the standalone `perft` benchmark.
- `chess_nnue`: NNUE network loading; links publicly to `chess_core`.
  [nnue/main.cpp](src/nnue/main.cpp) builds `nnue_demo`, which loads `nnue.bin`
  from the working directory or a network file passed as its first argument.
- `chess_engine`: negamax, history, killers, and the transposition table; links
  publicly to `chess_core` and `chess_nnue`.
  [engine/main.cpp](src/engine/main.cpp) builds `demo`.
- With `BUILD_TESTING`, `chess_types_tests`, `chess_core_tests`,
  `chess_nnue_tests`, and `chess_engine_tests` link to their respective libraries.
  Dependencies flow from Types to Core and NNUE to Engine through public links.

CMake discovers modules under `src/types/`, `src/core/`, `src/nnue/`, and
`src/engine/`, excluding application entry points from libraries.
Adding or removing matching files triggers CMake reconfiguration during builds.

Module names use `types.*`, `core.*`, and `engine.*`. Attack and hash imports are
`core.attacks.generator`, `core.attacks.lookup`, and `core.hashes.hash`.
These dotted names do not change the C++ namespaces (`chess` and
`chess::attacks::generator`) or implicitly import other modules.

The `types.squares` module separates declarations and definitions into independently
compiled interface partitions:

- [squares.decl.cppm](src/types/squares/squares.decl.cppm): `types.squares:decl` exports
  `chess::squares_t` and re-exports `types.square`, including its file and rank
  exports.
- [squares.def.cppm](src/types/squares/squares.def.cppm): `types.squares:def` imports `:decl` and
  defines the constexpr constructors and square-set methods.
- [squares.cppm](src/types/squares/squares.cppm): the primary interface re-exports both
  partitions so consumers only need `import types.squares;`.

Re-exporting the definition partition makes the constexpr definitions reachable
for constant evaluation by importers, without including implementation files.
Non-constexpr definitions can live in one or more separately compiled `.cpp`
files (for example `squares.impl.cpp`) declaring `module types.squares;`.
There are currently no runtime-only methods, so no runtime implementation file
is needed yet. CMake collects `.cppm` interfaces and partitions into its module
file set and `.cpp` implementation units as ordinary sources.
Only the primary interface declares `export module types.squares;`; a second
primary interface would generate the same compiled module file and fail the
build.

The coordinate modules use the same wrapper-and-partition layout:

- [file.cppm](src/types/file/file.cppm) re-exports [file.decl.cppm](src/types/file/file.decl.cppm)
  (`types.file:decl`), which defines `chess::file_t`.
- [rank.cppm](src/types/rank/rank.cppm) re-exports [rank.decl.cppm](src/types/rank/rank.decl.cppm)
  (`types.rank:decl`), which defines `chess::rank_t`.
- [square.cppm](src/types/square/square.cppm) re-exports
  [square.decl.cppm](src/types/square/square.decl.cppm) (`types.square:decl`), which defines
  `chess::square_e`, declares `chess::square_t`, and re-exports `types.file` and `types.rank`,
  along with [square.def.cppm](src/types/square/square.def.cppm) (`types.square:def`), which defines
  the constexpr constructors, conversion operator, and coordinate accessors.

The `file` and `rank` modules contain enums and type aliases, so they need no
separate `:def` partitions. Consumers use `import types.file;`,
`import types.rank;`, or `import types.square;`; importing `types.square` exposes all three types.

## Directional rays

`attacks::lookup::ray(from, direction)` returns all squares from the next square
in the specified direction through the board edge, excluding `from` and ignoring
occupancy. North increases rank; east increases file. The compile-time ray table
has 64 origins and eight directions. [lookup.cpp](test/core/lookup.cpp) checks every entry
against coordinate stepping, including empty edge rays, and verifies that the
eight disjoint rays combine to the empty-board queen attacks.

`test_sinpers_and_blockers` in [position.cpp](test/core/position.cpp) checks exact sniper and
friendly-blocker bitboards at compile time: all eight ray directions, both king
colors, rook/bishop/queen pins, multiple simultaneous pins, nearest-slider
selection, board edges, direct checks, enemy blockers, multiple blockers, and
non-pinning piece placements.

## Input preconditions

Coordinate constructors require square indices in `0..63` (`a1..h8`), file
indices in `0..7` (`a..h`), and rank indices in `0..7` (`_1.._8`).
Square-set constructors require every list element to satisfy the same ranges.
Explicit enum casts do not validate these ranges.

Debug assertions check these preconditions before coordinate arithmetic or
bit shifts. Invalid inputs fail an assertion at runtime and cannot be used in
constant expressions when assertions are enabled. With `NDEBUG`, checks are
disabled and callers must guarantee valid inputs; no clamping or fallback is
performed. `front()` and `back()` also require a non-empty set.

## Move execution

Snipers and pinned pieces are derived locally by `find_snipers_and_blockers`
once per move-generation call, after the double-check early return. They are
not stored in the position or undo stack, and there are no `snipers()` or
`pinned()` getters. FEN setup and move execution do not calculate them, avoiding
pin scans at search nodes that return before generating moves.

`position_t::checkers()` returns the cached enemy pieces attacking the current
side-to-move's king. FEN setup and move execution initialize/recompute this
cache; undo restores it from the saved state. Compile-time and runtime tests
cover both colors, slider/knight/pawn checks, double and discovered checks,
blocked attacks, simultaneous checks and pins, captures, promotions, and nested
undo.

`position_t::do_move(move)` and `undo_move(move)` support ordinary moves,
captures (including en passant), orthodox castling, and promotions to knight,
bishop, rook, or queen. They update the
square-to-piece array, side and type bitboards, and side to move. Captured pieces
are saved in a fixed-capacity stack and restored during undo. Both methods are
`constexpr`; [position.cpp](test/core/position.cpp) checks cached state and round trips
at compile time and runtime, including promotion captures and nested undo.

`position_t::hash()` returns a cached 64-bit Zobrist key using the
`core.hashes.hash` lookup tables. FEN construction hashes occupied piece squares,
the side key for Black, each castling-right rook square, and the file of any
stored en-passant target. Empty squares, move counters, and derived attack/pin
caches are not hashed. An uncapturable en-passant target still contributes:
this is a position-state key, not a canonical FIDE repetition key.
Move execution updates the key incrementally, including captures, promotions,
castling rook moves, and changed special squares. Undo restores the saved key.
Tests independently recompute keys in compile-time lifecycle checks and after
every move/undo in depth-three trees covering starting position, Kiwipete,
en passant, and promotions.

Castling uses the king's move (`e1/e8` to `g1/g8` or `c1/c8`); execution also moves
the rook from `h1/h8` to `f1/f8`, or `a1/a8` to `d1/d8`. King moves remove both
castling rights for that side. Moving from or capturing on a rook's starting
square removes its corresponding right. Undo restores the previous rights and
special-square bitboard.

En passant uses an ordinary pawn move to the empty FEN target square, with no
extra move flag. Execution removes the enemy pawn on the destination file and
source rank; undo restores it using the saved capture square. Every move expires
the previous en-passant target; a two-square pawn push creates a new target on
the intermediate square, even if no enemy pawn can capture there. Undo restores
the previous target and castling rights.

En-passant generation handles both colors separately from ordinary captures.
It applies the source-specific pin mask separately from check evasion: the
destination must block the check, or the captured pawn must be the checker.
It then tests rook/queen and bishop/queen attacks using occupancy with both
source and captured pawns removed and the destination occupied. This also
rejects discovered attacks caused by removing the captured pawn.

Callers must supply valid moves for the current side; these methods do not
validate piece movement or king safety, including attacked castling transit
squares. Chess960 castling is not supported.
A pawn reaching its promotion rank must specify a promotion.
`undo_move` must receive the exact most recently executed move, in reverse
execution order. At most 1024 moves may be outstanding. Debug assertions check
basic move preconditions and stack overflow/underflow; with `NDEBUG`, callers
remain responsible for these preconditions.

## Perft

[core.perft](src/core/perft.cppm) contains the existing `perft_t{fen}(depth)` traversal,
shared by the benchmark and tests. It returns a `size_t` leaf count, uses
`do_move`/`undo_move`, and bulk-counts generated moves at depth one. It relies on
the generator for legality. King destinations and castling transit squares use
enemy attacks computed with the moving king removed from occupancy, so its old
square cannot hide a slider attack. Pinned pawns generate pushes, double pushes, captures, and all four
promotions restricted to their pin line and the check-evasion mask. Double pushes
require both squares empty but only the destination to satisfy the evasion mask.
Both pinned and unpinned pawn promotions use `splat_promotions(offset, to, buffer)`.
It emits four 16-bit moves in queen, rook, bishop, knight order using one 8-byte
SIMD store, with source square `to + offset`. The buffer must have at least four
remaining entries and the source index must be in `0..63`.

`generate_capture_moves` returns only legal captures, including en passant and
capture promotions. `generate_tactical_moves` additionally includes quiet
promotions to all four pieces, without other quiet moves or castling. It uses
a separate pawn destination mask so adding the promotion rank cannot admit
quiet moves by other pieces. Both masks respect check-evasion and pin restrictions.
The [engine.negamax](src/engine/negamax.cppm) quiescence search uses tactical moves outside check. In check,
it skips stand pat, searches all legal evasions, and scores checkmate if none exist.
Engine search depths, heights, principal-variation lengths, history bonuses,
killer indices, and transposition-table depths use `ply_t` (`uint8_t`).
Search stops extending at height 255 to prevent height wraparound. At that
horizon it still detects checkmate, otherwise using static evaluation in check
or the stand-pat alpha bound outside check. The demo prints depths numerically.

`position_t::tactical(move)` classifies a legal move in the position before it is
played: captures, all promotions, and pawn moves to the en-passant target are
tactical. Quiet moves (including castling and non-pawn moves to the en-passant
target) are not. Compile-time and runtime tests cover both colors, all four
promotion pieces, and en-passant target creation, expiration, and undo.

The transposition table stores mate scores relative to the stored position:
winning scores add the current ply on storage and subtract the probe ply on lookup;
losing scores do the reverse. This preserves mate distance when the same position
is reached at different search heights. Exact scores and both bound types use
the same conversion; ordinary evaluations and ignore sentinels are unchanged.

Main-search move ordering uses five lazy spans flattened with C++23 `views::join`:
the legal TT move, tactical moves scoring at least zero sorted descending,
both killers in priority order, quiet moves sorted by history, and remaining
tactical moves sorted by capture/promotion score. Each phase
only processes the unconsumed suffix. A cutoff skips later phases. Missing or
already-consumed TT/killer moves are ignored; every legal move is visited once.
The nonnegative-score threshold is an ordering policy, not a static exchange evaluation:
en passant scoring -1 stays in the final phase unless
selected earlier as the TT move or a killer.

Quiet moves in the fourth phase use [engine.history](src/engine/history.cppm), a zero-initialized
`int16_t` piece-to table indexed by colored piece and destination square. Remaining
tactical moves retain their capture/promotion score. History does not affect the
nonnegative tactical-score partition or TT/killer priority.
A quiet beta cutoff rewards its move and penalizes previously searched quiet
moves at that node, after restoring the parent position. The depth bonus is
`min(16 * depth * depth, 2048)`, calculated with a bounded depth to avoid overflow.
Updates use history gravity (`value += bonus - value * abs(bonus) / 16384`) with
the bonus clamped to `[-16384, 16384]` and arithmetic in `int`. Captures and
promotions do not update history. Each `negamax_t` owns its table and retains it
across iterative-deepening calls; constructing a new search starts with zero history.

## Tests

Tests use CTest without an external test framework. Compile-time assertions
are built as part of the test executables; runtime failures print a named check
and source location and return a nonzero exit status.
Sources mirror the library layout in `test/types/`, `test/core/`, and
`test/engine/`, with `main.cpp` as each executable's entry point. The shared
assertion helper is `test/test_support.hpp`. Test targets link only their
corresponding library; CMake builds prerequisites before consumers and may
build independent test targets in parallel.

- `chess.types`: piece/side encoding, coordinate round trips, square-set
  operations and iteration, score arithmetic/classification, 64-bit hash
  boundaries and reversible XOR updates, and ply range/arithmetic; checked at
  compile time and runtime while linking only Types.
- `chess.lookup`: attack generation/lookup assertions and all 512 ray entries.
- `chess.position`: move encoding assertions, FEN queries, sniper/blocker
  assertions, pin detection after move/undo and checker lifecycle tests, all four castling moves, castling
  rights updates, nested castling undo, en-passant execution/generation,
  king-safety edge cases, en-passant target lifetime/restoration, and exact
  capture/tactical move sets and order for both colors, including promotions,
  pins, checks, and castling exclusion.
- `chess.perft`: initial-position reference counts at depths zero through six
  (`1`, `20`, `400`, `8902`, `197281`, `4865609`, `119060324`),
  Kiwipete at depths zero through five
  (`1`, `48`, `2039`, `97862`, `4085603`, `193690690`), focused en-passant positions for both colors
  at depths zero through two (`1`, `7`, `38`), and repeated-call restoration.
- `chess.engine`: quiescence quiet promotions, checkmate, quiet check evasions, and TT mate-score
  conversion across search heights for exact, lower-bound, and upper-bound entries,
  history gravity bounds and piece/square isolation, and history ordering without
  changing capture-phase classification. Ordering tests also check that the
  underlying buffer prefix matches the searched sequence, allowing cutoff
  penalties to scan only previously searched moves.

CTest is available through VS Code's CMake Tools test integration. To run a suite
directly, use `./build/chess_types_tests` for Types,
`./build/chess_core_tests lookup`, `./build/chess_core_tests position`, or
`./build/chess_core_tests perft`; no argument runs all Core suites.
`./build/chess_engine_tests` runs all Engine suites. The `mates` selector
(`chess.mates` in CMake Tools) runs the longer mate-in-3/5/6/8 search regressions,
using iterative deepening through depths 5/9/11/15 and checking exact mate
distances and legal PV moves. The `negamax` selector runs the shorter search tests.
The Core `position` suite also checks null-move hash/EP/last-move round trips,
halfmove-counter preservation, and repetition isolation across null moves.
CTest labels `types`, `core`, and `engine` group tests by library.
Set `BUILD_TESTING=OFF` to omit all three test targets.

## NNUE network

The NNUE evaluation network is compressed as `nnue.bin.xz`. Before running `demo`,
`nnue_demo`, or the NNUE/evaluator test suites, decompress `nnue.bin.xz` to obtain `nnue.bin`.

### How to extract

- **Windows (with 7-Zip):**
  Right-click `nnue.bin.xz` $\rightarrow$ **7-Zip** $\rightarrow$ **Extract Here**.
- **Linux / macOS:**
  ```sh
  xz -dk nnue.bin.xz
  ```

## Build and run

Requires CMake 3.28 or newer, Ninja, and a compiler supported by CMake's module
dependency scanning (for example GCC 14+ or Clang 16+ with its matching
`clang-scan-deps` installed). Use a recent compiler for best module support.
Link-time optimization (LTO) is enabled for the libraries and applications in all build
configurations, so the selected compiler and linker must support LTO.
Use a Release configuration
(`-DCMAKE_BUILD_TYPE=Release` for Ninja) for optimized runtime performance.
Compilation and linking use `-march=native` to target the build machine's CPU,
including during LTO. This requires a compiler accepting that flag (such as
GCC or Clang); the executable may not run on CPUs lacking the selected features.

```sh
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build
./build/perft
./build/demo
```

For GCC, substitute `-DCMAKE_CXX_COMPILER=g++` and use a separate build directory.
CMake manages module compilation order and compiled module artifacts; no manual
module flags or header units are needed.

`perft` prints perft timings for depths one through six over the benchmark FENs.
`demo` prints iterative-deepening search scores, cumulative nodes/timings, and
principal variations for depths one through twelve over the same FENs.
Tests do not run during application startup or benchmark timing. NNUE and UCI
can be added to Engine without introducing dependencies into Core.

`core.perft` and `engine.negamax` expose class declarations in their `.cppm`
interfaces; runtime method definitions live in matching `.cpp` implementation
units. The lazy ordering template lives in `engine.ordering`, imported by
Negamax's implementation and the focused ordering tests, not by its public
interface. LTO remains enabled across library and application boundaries.

## License

This project is licensed under the [GNU General Public License Version 3](LICENSE).
