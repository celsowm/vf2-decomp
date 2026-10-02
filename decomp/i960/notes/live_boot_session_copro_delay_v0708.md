# Live boot session past copro-init and delay (v0708, measured)

## Session wall

`vf2.exe --rom-dir roms/vf2 --native-snapshot out/boot.vf2snap`
runs 42 tiny frames (post-boot video-wait spins at
`0x0bc0`/`0x0f7c`/`0x2ec4`) then fails frame 43 at `0x0000a178`
(coprocessor initializer, `0x9f70` caller): the recovered
`execute_post_boot_copro_init` block gates `local_frame_depth == 4`
but the live session arrives at depth 1 (main-loop-init from depth 0;
outer frame carries the documented `0x9a00..0x9f70` caller state:
saved `r2 = 0x9f74`, `r4 = 0x00515400`, `r5 = r7 = r14 = 0`).
`r27 + r28 = 0x884000` matches the port check in both shapes.

## Oracle shape (from the failure snapshot, `--set-ip` not needed)

`--until 0x9f74 --max-steps 100000`: 13,324 instructions, call
targets `8x 0x7fc0 + 8x 0x9444 + 2x 0xa3d4`, trace mnemonic counts
10 calls / 11 rets, write regions identical to the corridor
measurement the native block encodes (work RAM fills, `0x90e000`
control words, `0x1000a14..` upload window). The block body never
reads the outer stack (globals + fixed tables + balanced
enter/return pairs), so entry depth only selects the caller context.

## Proof

- `execute_post_boot_copro_init` gate generalized to depth `{1,4}`.
- Failure snapshot (session, depth 1) vs oracle `--output-snapshot`
  at `0x9f74`: `vf2i960 compare-snapshots` reports **match**
  (CPU + all mutable memory).
- Same pattern applied to `execute_post_boot_copro_helper`
  (depth `{2,5}`) and `execute_post_boot_delay` (depth `{0,3}`;
  body is fixed stores + one balanced thunk + the 700000-iteration
  divr loop; oracle from session pre-state measures exactly the
  asserted 2,100,198 steps).
- Session now runs 60/60 frames (frame 43 alone: 224 blocks /
  3.5M instructions through copro + delay + main loop; frame 49:
  743 blocks / 7.3M), reaching new code (`0x10fa4`, real per-frame
  instruction counts). No other source change.

## Session tooling (this slice)

- `vf2.exe --trace-state`: per-frame game mode/phase/flags/input,
  fighter task pointers and heads, dispatch base, depth, `r2`,
  `r27`/`r28`, ip.
- `--failure-snapshot <file>`: capture CPU+RAM when a frame fails.
- `--snapshot-at <frame> --snapshot-path <file>`: mid-run
  checkpoint (raw `.vf2snap` in temp, never committed).
