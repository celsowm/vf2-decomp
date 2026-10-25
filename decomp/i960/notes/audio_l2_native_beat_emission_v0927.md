# v0927 — original 68000 prescaler beat emission recovered in native C

Date: 2026-10-25. Branch: `master`.

## Objective: stop treating a known audible command as unsupported

The original 68000 Timer-A prescaler at `0x21BA` runs when record
`0x3000` has nonzero rate (+0C) and its byte countdown (+0D)
expires. The previously documented owner-ROM differential at
`tests/m68k/test_cdispatch.c` already verified one exact case:
rate 8, countdown 1, beat 8; the original 68000 executes 67
instructions and emits `BE 10 04`, reloads +0D to 8,
sets +0E to 4 and increments the command queue by one.
That original program path and test already existed; only the
native C path was still refusing it.

`vf2_sound_board_maintain_streams` now admits the **nonnegative
beat** at original PC `0x21CA..0x2206`, and only when its
command-ring cursor is aligned. The exact 8-bit byte decrement
is `beat = (old+0E - 4) & 0xFF`; if the resulting sign bit is
set (BMI), it REFUSES atomically. The incomplete stream-end
branch `0x2208..0x223C` remains unsupported.

| Original operation | Native C effect |
| --- | --- |
| `SUBQ.B #4,+0E(A2)` | byte value changes 8 -> 4 |
| Normal beat packet | ring `BE 10 04` |
| 4-byte FIFO cells | fourth byte preserved |
| Advance write cursor | `(old + 4) & 0xFFF` |
| Ring count | +1 (original ISR gate skips at FF) |
| Reload +0D | copies rate byte +0C |
| SCSP timer writes | previous unconditional 0418/0422 pokes |

When a prescaler beat and a worker packet expire together,
the unmeasured multi-writer ordering is refused before mutations;
the tick reports prescaler stage 1. Malformed ring cursor, negative
result/stream-end and unknown workers also remain fail-closed.

## Verification

The public ROM-free `vf2_sound_board` test now covers the
positive beat, subsequent 4 -> 0 beat, refusal of the negative
0 -> FC stream-end handoff, malformed cursor and a 0xFFC
ring-wrap. The preexisting owner-original 68000
`vf2_m68k_cdispatch_tests` test for the 67-instruction case
is upgraded from “expected C refusal” to **strict C-vs-original
full RAM and SCSP differential**, excluding only the interpreter
stack scratch region as in earlier measured fixtures.

No private owner ROM is installed in public CI, so that
differential still needs an owner-local run:

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON -DVF2_WARNINGS_AS_ERRORS=ON \
  -DVF2_ROM_DIR=/private/vf2
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

This is an actual **native command-production path**, not a
fabricated waveform. It does not claim a newly generated sound
file, accurate asynchronous 68000 scheduling, recovery of the
music-stream A0 applier, or arcade-identical SCSP output.
The already experimentally audible 10-second Akira output
remains opt-in/owner-ROM only (see v0903, v0918).

References: `audio_l2_stream_tick_v0852.md`,
`audio_akira_original_pcm_walk_v0903.md`,
`audio_scsp_timer_a_900hz_v0918.md`.
