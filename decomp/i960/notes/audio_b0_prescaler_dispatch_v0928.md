# v0928 — B0 0x12 prescaler setup, BE10 real dispatch frontier

Date: 2026-10-25. Direct commit to `master`.

## Why the previous beat was not yet a complete sound command

The native L2 (v0927) emits `BE 10 04`, but the C
`sound_board_dispatch_b0` previously treated every B0 selector
except uploader 0x3A as a successful no-op. The actual 68000
**jump table is not a default RTS** for 0x10: it enters
`0x181C`, a volume/descriptor updater. The authentic
`BE 50 7E` end-of-sequence command enters 0x50 ->
`0x1C0A`, a secondary dispatcher. Neither can be safely
retired without recovering its side effects.

This update explicitly refuses **only** these two live
unrecovered selectors with `VF2_ERROR_UNSUPPORTED`, keeping
the FIFO count and read cursor unchanged. It reports their
true ROM handler addresses. All other historical B0 behavior
is left unchanged pending branch-by-branch validation.

## Newly recovered original B0 table handler 0x12

The original jump table selector 0x12 targets PC `0x18A8`:
`LEA $2000(A6),A0`; two `MOVE.B D0,+$0C/+0D(A0)`;
`RTS`. In the B0 primer, D0 is the **third ring byte**,
NOT the selector. This installs the sound-stream prescaler
rate and countdown in RAM `0x300C` and `0x300D`, which
feed the original L2 routine at `0x21BA`.

New bounded C path:
- `BE 12 08` dispatch writes both prescaler bytes to 8.
- Native L2, with the admitted expiry state, emits `BE 10 04`
  and reloads its countdown.
- Dispatch of that new command now *refuses* at the exact
  unresolved `0x181C` boundary; it no longer incorrectly
  consumes it.

## New verification

A ROM-free chain test in `tests/hardware/test_sound_board.c`
checks B0 third-byte provenance, descriptor matching, FIFO
retirement of the setup, L2 beat production, and FIFO hold
for the 0x10 and 0x50 target handlers.

An owner-ROM opt-in `test_cdispatch.c` fixture executes
the **actual four original 68000 instructions** starting
at 0x18A8 for input bytes 00, 08 and FF, comparing the
entire affected 0x20-byte music descriptor and SCSP state
against native C. That fixture requires execution with
the private sound ROM. Public ROM-free CI compiling it
does not validate original-ROM parity.

Run locally with owner-provided ROM:

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON \
  -DVF2_WARNINGS_AS_ERRORS=ON -DVF2_ROM_DIR=/private/vf2
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Next priority is the full 0x181C B0 handler, including
its shared factor and per-descriptor volume update,
then the 0x1C0A sub-dispatch for the terminal marker.
Do not automatically drain a known nontrivial command,
insert fake audio, or claim arcade PCM equivalence.

Evidence: `audio_b0_table_map_v0799.md`,
`audio_b0_table_surface_v0788.md`,
`audio_80_b0_byte2_v0793.md`,
`audio_l2_native_beat_emission_v0927.md`.
