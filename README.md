# NibbleKnight

NibbleKnight uses compact pre-trained tables to judge positions and promising moves, with a large chunk of its judgement coming from the shape of the boards rather than piece positional awareness.

## Build

```sh
cc -O3 -std=c99 -c engine.c -o engine.o
```

## Board

- Squares: a1 = 0 to h8 = 63.
- Pieces: 0 = empty, 1–6 = White PNBRQK, 7–12 = Black PNBRQK.
- Moves: `from * 64 + to`. A promotion is always to a queen.
- The engine plays White. To play Black, swap the colours and mirror the ranks.

## Use

1. Call `c32_init` one time.
2. Call `c32_set_castling`, `c32_set_halfmove` and `c32_set_history`.
3. Call `c32_legal_moves` to get the move list.
4. Call `c32_choose` (fixed even depth) or `c32_choose_timed` (time limit). Each returns the index of the chosen move.

`c32_choose_timed` reads the time from `c32_clock_ms`. Set it to a monotonic millisecond clock if the default is not available.

The engine uses global state. It is not thread-safe.

## Limits

- The engine searches at most 40 White moves at each node.
- The engine promotes only to a queen.
- In the search, a repetition check compares only the board and the side to move.
- The capture search has no repetition check.
- The engine skips moves that look bad, so the search depth is not a full count of every move.
