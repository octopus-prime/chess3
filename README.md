# Chess

A C++23 project using C++ modules and Clang-specific SIMD builtins, split into
four static libraries and two applications:

- `chess_types`: coordinate, square-set, piece, side, type, and score modules;
  independent of Core and Engine.
- `chess_core`: positions, moves, attacks, hashes, and perft; links publicly
  to `chess_types`.
  [core/main.cpp](src/core/main.cpp) builds the standalone `perft` benchmark.
- `chess_nnue`: NNUE network loading and inference; links publicly to `chess_core`.
- `chess_engine`: negamax, NNUE evaluation, history, killers, and the transposition table; links
  publicly to `chess_core` and `chess_nnue`.
  [engine/main.cpp](src/engine/main.cpp) builds `chess3`.
- With `BUILD_TESTING`, `chess_types_tests`, `chess_core_tests`,
  `chess_nnue_tests`, and `chess_engine_tests` link to their respective libraries.
  Dependencies flow from Types to Core and NNUE to Engine through public links.

CMake discovers modules under `src/types/`, `src/core/`, `src/nnue/`, and
`src/engine/`, excluding application entry points from libraries.
Adding or removing matching files triggers CMake reconfiguration during builds.

Module names use `types.*`, `core.*`, `nnue.*`, and `engine.*`. Attack and hash imports are
`core.attacks.generator`, `core.attacks.lookup`, `core.hashes.hash`, and `core.hashes.lookup`.
These dotted names do not change the C++ namespaces (`chess` and
`chess::attacks::generator`) or implicitly import other modules.

The `types.squares` module separates declarations and definitions into independently
compiled interface partitions:

- [squares.decl.cppm](src/types/squares/squares.decl.cppm): `types.squares:decl` exports
  `chess::squares_t` and re-exports `types.square`, including its file and rank
  exports.
- [squares.impl.cppm](src/types/squares/squares.impl.cppm): `types.squares:impl` imports `:decl` and
  defines the constexpr constructors and square-set methods.
- [squares.cppm](src/types/squares/squares.cppm): the primary interface re-exports
  `:decl` and imports `:impl`, so consumers only need `import types.squares;`.

Importing the definition partition from the primary interface makes its constexpr definitions reachable
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
  and imports [square.impl.cppm](src/types/square/square.impl.cppm) (`types.square:impl`), which defines
  the constexpr constructors, conversion operator, and coordinate accessors.

The `file` and `rank` modules contain enums and type aliases, so they need no
separate `:impl` partitions. Consumers use `import types.file;`,
`import types.rank;`, or `import types.square;`; importing `types.square` exposes all three types.

## Directional rays

`attacks::lookup::ray(from, direction)` returns all squares from the next square
in the specified direction through the board edge, excluding `from` and ignoring
occupancy. North increases rank; east increases file. The compile-time ray table
has 64 origins and eight directions. [lookup.cpp](test/core/lookup.cpp) checks every entry
against coordinate stepping, including empty edge rays, and verifies that the
eight disjoint rays combine to the empty-board queen attacks.

`test_snipers_and_blockers` in [position.cpp](test/core/position.cpp) checks exact sniper and
friendly-blocker bitboards at runtime: all eight ray directions, both king
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

`do_null_move()` is a search-only operation requiring a legal position outside
check. It switches sides, clears en passant and its hash component, preserves
pieces, castling rights and the halfmove counter, and resets the repetition
count to one. `last_move()` becomes an empty move. Normal and null undo restore
the previous last move and counters along with the other cached state.
Repetition scanning visits every saved state to detect null boundaries, but
compares hashes only at matching side-to-move parity. It includes the position
immediately after a null move and excludes older history.

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

`generate_tactical_moves` returns legal captures, including en passant and
capture promotions, plus quiet promotions to all four pieces, without other
quiet moves or castling. There is no separate public capture-only generator. It uses
a separate pawn destination mask so adding the promotion rank cannot admit
quiet moves by other pieces. Both masks respect check-evasion and pin restrictions.
The [engine.engine](src/engine/engine.cppm) quiescence search uses tactical moves outside check. In check,
it skips stand pat, searches all legal evasions, and scores checkmate if none exist.
Engine search depths, heights, principal-variation lengths, history bonuses,
killer indices, and transposition-table depths use `ply_t` (`uint8_t`).
Search stops extending at height 255 to prevent height wraparound. At that
horizon quiescence returns static evaluation without generating moves or detecting checkmate.
The iterative-deepening counter uses a wider integer so a requested depth of 255 terminates.
The `chess3` app prints depths numerically.

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

The TT also owns a separate direct-mapped static-evaluation cache with 65,536
entries (about 1 MiB). Each entry verifies the complete 64-bit position hash and
stores only the raw side-to-move NNUE score, never a search bound or mate-distance
adjustment. Main search and quiescence share it through `engine_t::evaluate()`.
The evaluator currently depends only on hashed board state, not the halfmove
clock or repetition history; any future rule-50 scaling must be applied after
the cached raw evaluation. A miss runs the normal accumulator-based evaluator.
The cache is allocated once with the TT and cleared with it, including after
each UCI search. The six-entry TT buckets and replacement strategy are unchanged.

Main-search move ordering uses five lazy spans flattened with C++23 `views::join`:
the legal TT move, tactical moves with nonnegative SEE sorted descending,
both killers in priority order, quiet moves sorted by history, and remaining
tactical moves sorted by SEE. Each phase
only processes the unconsumed suffix. A cutoff skips later phases. Missing or
already-consumed TT/killer moves are ignored; every legal move is visited once.
The production ordering overload uses `position_t::see(move)` for tactical
partitioning and sorting. The projection-based overload lets tests supply
custom scores.

Quiet moves in the fourth phase use [engine.history](src/engine/history.cppm), a zero-initialized
`int16_t` piece-to table indexed by colored piece and destination square. Remaining
tactical moves retain their SEE ordering. History does not affect the
nonnegative tactical-score partition or TT/killer priority.
A quiet beta cutoff rewards its move and penalizes previously searched quiet
moves at that node, after restoring the parent position. A fixed-size list excludes
pruned moves from these penalties. The depth bonus is
`min(16 * depth * depth, 2048)`, calculated with a bounded depth to avoid overflow.
Updates use history gravity (`value += bonus - value * abs(bonus) / 16384`) with
the bonus clamped to `[-16384, 16384]` and arithmetic in `int`. Captures and
promotions do not update history. Each `engine_t` owns its table and retains it
across iterative-deepening calls; constructing a new search starts with zero history.

Quiet ordering also adds a one-ply continuation-history score, indexed by the
previous move's colored piece on its destination and the current move's colored
piece and destination. Quiet cutoffs apply the same gravity bonus and searched-quiet
penalties to this contextual table. The previous move may be tactical; a promoted
piece uses its promoted type, and castling uses the king's destination. Positions
without a previous move, including immediately after a null move, use ordinary
history alone. Quiet ordering and LMR use the same sum of ordinary and continuation
history. Tactical phases are unchanged.
The fixed-size table uses about 1.5 MiB, allocated once when constructing the engine;
searching and clearing it perform no allocations. `clear()` resets both histories,
including the existing UCI clearing after each search.

## Selective search

[engine.cpp](src/engine/engine.cpp) deliberately trades exhaustive coverage for
greater selective depth. Nominal depth is not a guarantee that every line is
searched that deeply; quiet sacrifices and zugzwang remain potential weaknesses.

- Reverse futility pruning applies at Non-PV nodes through depth seven, outside
  check and decisive beta windows, with an `80 * depth` centipawn margin.
- Null-move pruning requires a Non-PV node at depth three or greater, outside
  check, a non-decisive beta, a preceding real move, non-pawn/non-king material,
  more than eight legal moves, and static evaluation at least beta. Its reduction
  is `2 + depth / 3 + min((static_eval - beta) / 200, 3)`, capped at `depth - 1`.
  This is guarded, not verified, null-move pruning.
- At shallow Non-PV nodes, late quiet moves are skipped after `3 + depth * depth`
  quiet candidates (through depth five), or when static evaluation plus
  `100 + 120 * depth` cannot reach alpha (through depth three). Losing tactical
  moves with SEE below `-100 * depth` may be skipped through depth four.
  These rules retain at least one searched move and protect evasions, checking
  moves, promotions, killers, advanced pawn pushes, and decisive alpha windows.
- Late-move reductions start at depth three, after the third move at PV nodes
  and the second at Non-PV nodes. Quiet non-killer moves are reduced only outside
  check, without giving check, and without an advanced pawn push or decisive alpha.
  The reduction is `int(0.75 + log(depth) * log(move_count) / 2.25)`, plus one
  at Non-PV nodes, minus `(history + continuation history) / 8192`, clamped to
  `[1, depth - 2]`. Without a previous move, the continuation term is zero.
  An alpha improvement triggers a full-depth null-window re-search, followed by
  a full-window PV search when necessary.
- Quiescence outside check filters negative-SEE captures and captures whose
  material gain plus a 150-centipawn margin cannot reach alpha. Promotions and
  checking moves are exempt. Sorting uses `position_t::mvv_lva(move)` rather
  than repeatedly running SEE: `16 * victim + promotion - attacker`, using
  material values and counting the captured pawn for en passant. This shared
  ordering score is not a static evaluation or an exchange-safety test.
  Scores are computed once per move. AVX-512 insertion sorts the first up to
  16 scored moves; scalar insertion sort inserts all remaining moves into the
  sorted prefix. Equal scores retain their input order.
  In check, every legal evasion is retained.
- Mate-distance bounds constrain non-root nodes. TT bounds cause direct cutoffs
  only at Non-PV nodes rather than narrowing a PV window. Aspiration starts at
  depth five for non-mate scores and repeatedly widens failed bounds until the
  result fits or the full window is reached.

Clock polling is amortized over 1024 nodes in both full search and quiescence.
Aborted iterations restore the position and keep the last completed score and
best move; if no iteration completes, a legal root move is available as fallback.
Node counts restart with each public search call. These changes introduce no
additional heap-owned search storage.

A local Release-build comparison on four fixed positions gave the following
completed depths with a one-second search budget (not a playing-strength/Elo test):

| Position | Previous depth | Selective depth |
|---|---:|---:|
| Initial position | 8 | 16 |
| Kiwipete | 8 | 14 |
| Middlegame (`r1b2rk1/2q1b1pp/...`) | 9 | 20 |
| Rook endgame (`8/2p5/3p4/...`) | 14 | 23-24 |

At fixed depth nine, node counts fell from 1,984,749 / 4,737,413 / 1,500,500 /
84,624 to 19,207 / 40,482 / 46,096 / 7,476 respectively. The reference best
moves at that depth remained `e2e4`, `e2a6`, `c3d5`, and `b4f4`.

## Tests

Tests use CTest without an external test framework. Compile-time assertions
are built as part of the test executables; runtime failures print a named check
and source location and return a nonzero exit status.
Sources mirror the library layout in `test/types/`, `test/core/`, and
`test/nnue/`, and `test/engine/`, with `main.cpp` as each executable's entry point. The shared
assertion helper is `test/test_support.hpp`. Test targets link only their
corresponding library; CMake builds prerequisites before consumers and may
build independent test targets in parallel.

- `chess.types`: piece/side encoding, coordinate round trips, string-view construction
  of files, ranks, all 64 squares, piece types and both colors of pieces, square-set
  operations and iteration, score arithmetic/classification, 64-bit hash
  boundaries and reversible XOR updates, and ply range/arithmetic; checked at
  compile time and runtime while linking only Types.
- `chess.lookup`: attack generation/lookup assertions and all 512 ray entries.
- `chess.position`: move encoding assertions, string-view UCI move construction
  (including all promotion choices for both colors and non-null-terminated views), FEN queries, sniper/blocker
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
- `chess.nnue`: exact Stockfish feature-index and raw-evaluation references across
  all 32 network stacks, incremental/full accumulator equivalence, and network
  loader error checks.
- `chess.engine`: quiescence quiet promotions, checkmate, quiet check evasions, and TT mate-score
  conversion across search heights for exact, lower-bound, and upper-bound entries,
  history gravity bounds and piece/square isolation, and history ordering without
  changing capture-phase classification. Ordering tests also check that the
  underlying buffer prefix matches the searched sequence, allowing cutoff
  bookkeeping to distinguish searched moves. Search regressions check fixed-depth
  node budgets, reference best moves, legal PVs, deadline handling and root restoration.
- `chess.evaluator`: Stockfish reference scores through the accumulator cache,
  repeated refreshes, king moves, castling, en passant, queen captures,
  promotions/underpromotions, move undo, and null-move round trips. Deterministic
  random walks from every reference fixture compare cached evaluation against a
  full accumulator rebuild after moves, null moves, and undo, then verify the
  original Stockfish score is restored. Allocation checks cover cold cache entries.
- `chess.mates`: mate-in-3/5/6 regressions with exact mate scores. The selective
  search uses depths 5/9/15 respectively; the mate-in-6 distance remains 11 plies,
  independent of its larger search-depth budget. The mate-in-8 case remains disabled.
- `chess.uci`: in-process command/response checks for identification, readiness,
  new games, unknown commands, EOF and quit. Responses use `std::cout`, captured
  by the tests with a standard C++ string stream.
- `chess.uci.protocol`: when Python 3 is available, subprocess tests for readiness
  during search, stop/restart, node/time/depth limits, invalid input, options,
  terminal positions, quit, and EOF during search.
- `chess.engine.all`: all Engine suites, including the longer mate regressions.

CTest is available through VS Code's CMake Tools test integration. To run a suite
directly from the `build/` directory, use `./chess_types_tests` for Types,
`./chess_core_tests lookup`, `./chess_core_tests position`, or
`./chess_core_tests perft`; no argument runs all Core suites.
`./chess_engine_tests` runs all Engine suites. The `mates` selector
(`chess.mates` in CMake Tools) runs the longer mate-in-3/5/6 search regressions,
using internal iterative deepening through depths 5/9/15 and checking exact mate
distances and legal PV moves. The `engine` selector runs the shorter search tests.
The Core `position` suite also checks null-move hash/EP/last-move round trips,
halfmove-counter preservation, and repetition isolation across null moves.
`./chess_engine_tests uci` runs the small UCI protocol suite. The UCI input loop
returns on EOF or `quit`; it does not terminate the hosting process.
`./chess_nnue_tests` runs the NNUE suite.
CTest labels `types`, `core`, `nnue`, and `engine` group tests by library.
Set `BUILD_TESTING=OFF` to omit all four test executables.

## NNUE network

The evaluator uses the unmodified Stockfish network
[`nn-c9e480e92425.nnue`](https://github.com/official-stockfish/networks/blob/master/nn-c9e480e92425.nnue),
matching Stockfish revision `effdc3fd`. Its SHA-256 is
`c9e480e92425f887df6ab9b05c6180fbd762b07879df9d9845f1aa75440caa73`.
The implementation follows Stockfish's GPLv3 NNUE architecture while retaining
this project's independent, refresh-based design.

Before running `chess3` or Engine/NNUE tests, download
[`nn-c9e480e92425.nnue`](https://raw.githubusercontent.com/official-stockfish/networks/master/nn-c9e480e92425.nnue)
and place it in the repository root, keeping its original filename.
No extraction, renaming, or conversion is needed. The loader opens
`../nn-c9e480e92425.nnue`, so run these executables from `build/`; CTest uses that directory.
It reads the standard little-endian `.nnue` format directly and checks version,
architecture/section hashes, truncation, and trailing data. The previous custom
network format is no longer supported; no network conversion is needed.

There are three sparse input groups contributing to one 1024-element accumulator
per perspective:

- `FullThreats`: 59,808 attack/defence features, recomputed with current occupancy.
- `PP_3Wide`: 4,560 pawn-pair features for pawns on the same or adjacent files.
- `K32Q2`: 45,056 piece-square features conditioned on king bucket and opposing
  queen presence.

The evaluator keeps its per-color, per-king-square accumulator cache. Features
for both perspectives are enumerated together, sharing attack and pawn-pair
generation. Each `refresh()` uses a fixed-size membership table to subtract
cached features that disappeared and add new features, without sorting.
Cached features are marked once and cleared as current features are visited;
remaining marks identify removals, with branchless delta collection. Pawn-pair
enumeration uses adjacent-file bitboard masks rather than testing every pair.
Feature and delta buffers have a conservative legal-position bound of 1,144
indices (32 PSQs, at most 32 * 31 directed threats, and 120 pawn pairs).
Evaluation performs no heap allocations, including the first visit to any king
cache entry; only evaluator construction allocates the network and cache.
Accumulator updates stream weight rows in SIMD register tiles, combining paired
additions and removals into weight differences before updating the accumulator.
Unpaired rows are applied separately. This includes
remote slider threats, pawn pairs, and queen-plane changes. There are no
dirty-piece/dirty-threat records, accumulator stacks, or changes to move execution.

After pairwise clipped activation, the dense network is
`1024 -> 32 -> 32 -> 1`, with squared and linear activations, concatenated skip
connections, and 32 stacks selected by piece count and both sides' queen presence.
Evaluation matches Stockfish's raw `Network::evaluate()` integer result, including
its output scaling; Stockfish's additional evaluation/search adjustments are not
applied. AVX-512 VNNI is used for dense inference. The first affine layer detects
nonzero four-byte input groups with SIMD masks and visits only those groups;
zero groups contribute nothing and require no weight loads.

The checked-in reference fixtures in [reference_positions.hpp](test/nnue/reference_positions.hpp)
were generated from the adjacent Stockfish implementation and this exact network.
Tests require only the downloaded network, not a Stockfish checkout or executable.

## Build and run

Requires CMake 3.28 or newer, Ninja, and Clang with its matching
`clang-scan-deps` and a standard library supporting the C++23 features used here,
including `views::enumerate` and `ranges::fold_left`.
The current tested compiler is Clang 22.1.8 on Linux x86-64; an older minimum
Clang version has not been established.
GCC is not supported by the current sources: they use Clang-specific vector
reduction builtins such as `__builtin_reduce_or` and `__builtin_reduce_add`,
as well as the Clang option `-fconstexpr-steps=500000000`.
The implementation also requires x86 SIMD/BMI2 support, including AVX2,
AVX-512 (notably VBMI2 byte/word compression and VNNI inference), and BMI2
PEXT/PDEP. There is no portable scalar fallback or runtime CPU dispatch.
Link-time optimization (LTO) is enabled for the libraries and applications in all build
configurations, so the selected compiler and linker must support LTO.
Use a Release configuration
(`-DCMAKE_BUILD_TYPE=Release` for Ninja) for optimized runtime performance.
Compilation and linking use `-march=native` to target the build machine's CPU,
including during LTO. This requires a compiler accepting that flag (such as
Clang); the executable may not run on CPUs lacking the selected features.

In VS Code, select the Clang kit and Ninja generator in CMake Tools, configure
a Release build, then build the desired targets through CMake Tools.
CMake manages module compilation order and compiled module artifacts; no manual
module flags or header units are needed.

After downloading the network, run the applications from the build directory:

```sh
cd build
./perft
./chess3
```

`perft` prints perft timings for depths one through six over the benchmark FENs.
`chess3` runs a UCI command loop and uses NNUE evaluation. Tests do not run during
application startup.

## UCI and match testing

Search runs on one worker thread, leaving the command loop responsive to
`isready`, `stop`, and `quit`. A position change or new search stops and joins the
previous search before changing engine state. EOF also stops search. The last
completed iteration supplies the best move; if none completed, a legal move is
returned. Positions without legal moves return `bestmove 0000`.

Supported search limits (in any argument order) are `depth`, `nodes`, `movetime`,
`wtime`, `btime`, `winc`, `binc`, `movestogo`, and `infinite`. `go infinite` waits
for `stop` even if the maximum search depth is reached. Unsupported limits,
malformed numbers, malformed positions, and illegal move lists produce
`info string` diagnostics. Pondering, `searchmoves`, configurable Hash/Threads,
and Chess960 are not implemented; this engine uses one search thread and a fixed
16 MiB transposition table.

`Move Overhead` is a UCI spin option (default 20 ms, range 0–5000 ms). Fixed
movetime subtracts this reserve once. Clock searches reserve `Move Overhead`
for **each** of the `movestogo` moves (default 25), not just the current move.
For example, `40/1` with 20 ms overhead reserves 800 ms for communication and
scheduling across the period, initially leaving a 5 ms soft search budget.
If the reserve consumes the remaining clock, the engine returns a legal fallback.
Clock searches use a soft budget of remaining
usable time divided by `movestogo` (default 25), plus 75% of the increment, capped
by usable time. The hard budget is at most three times the soft budget and never
exceeds usable time. Soft limits stop between iterations; hard limits are polled
every 128 nodes. Stop requests and node limits are checked at every node.
Deadlines start when the UCI command loop receives `go`, before joining the
previous worker or starting the next one. An already expired search returns a
legal fallback without running NNUE or initial quiescence. An interrupted initial
quiescence uses a neutral fallback score rather than evaluating after the deadline.
Scheduling delays can still cause deadline overshoot, so retain a safety reserve.
TT/history clearing after each UCI search is intentionally unchanged for now;
`ucinewgame` resets the engine too.

For interactive testing, enter each command after the previous response:

```text
uci
isready
position startpos
go movetime 1000
```

Wait for `bestmove`, or send `stop`; sending `quit` immediately after `go`
interrupts the search rather than waiting for the requested depth.

### Automated fastchess matches

Requires Python 3.11+ and `fastchess` on PATH (tested with alpha 1.8.2). Preserve
the match-ready baseline binary **before** making a candidate change:

```sh
mkdir -p matches
cp build/chess3 matches/baseline
# Make one candidate change, build, and run tests.
python3 scripts/match.py --baseline matches/baseline --candidate build/chess3 \
  --rounds 4 --tc 2+0.02 --output matches/smoke
```

The bundled six-position EPD suite is only a protocol smoke test, not a strength
test. For measurements, supply a large, suitably varied EPD or PGN opening suite:

```sh
python3 scripts/match.py --baseline matches/baseline --candidate build/chess3 \
  --openings /path/to/openings.epd --rounds 1000 --tc 10+0.1 \
  --concurrency 1 --output matches/experiment
```

Each round plays the same opening with colors reversed; the seed defaults to
20261018. Both engines receive identical time controls and run with `build/` as
their working directory so the network is found. `--engine-dir` overrides that
directory. The runner snapshots both binaries and the opening suite, records
their SHA-256 hashes, network hash, source revision/worktree status, fastchess
version and exact command, and saves PGNs, console logs, and fastchess resume
state in the output directory. Incomplete matches, abnormal PGN terminations
(including time forfeits), and fastchess PV/protocol warnings make the runner
return a failure status. Output directories must be new and are ignored
by Git. Keep the working network unchanged throughout a match.

Optional `--sprt` enables normalized-Elo bounds [0, 5] with alpha/beta 0.05.
Choose a sufficiently large round cap; reaching that cap without crossing an
SPRT boundary is inconclusive. Positive results require sufficient games and
uncertainty reporting, not just a favorable small sample. Check PGNs/logs for
crashes, illegal moves, and time forfeits before interpreting Elo. No
evaluation-score-based resignation or draw adjudication is enabled.
Fastchess lists baseline first in the summary, so its displayed Elo is baseline
relative to candidate; inspect the player names when interpreting the sign.

`core.perft` and `engine.engine` expose class declarations in their `.cppm`
interfaces; runtime method definitions live in matching `.cpp` implementation
units. The lazy ordering template lives in `engine.ordering`, imported by
the search implementation and focused ordering tests, not by its public
interface. LTO remains enabled across library and application boundaries.

## License

This project is licensed under the [GNU General Public License Version 3](LICENSE).
