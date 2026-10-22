# Akira Ride the Tiger: authentic MC68000 PCM stream walker, not prefill

Date: 2026-10-22. Owner ROM: `vf2(20261018-203049).zip`
(36 chip files, verified against this repository's checksums).
No original ROM bytes, decoded wave dumps, or proprietary WAVs are
committed to the repository.

## Root cause and original instructions

The 10-second original 68000 `AD 10 04` song test previously called
stream L2 (`0x1f16`) and ring drain (`0x07a6`) but omitted
the **main-loop descriptor walker at `0x2242`**. The sound CPU
had already staged the first 512 PCM bytes of many 8191-byte
looping sample windows, but the next-page cursors at
`RAM 0x2000 + slot*0x20 + {2,6}` did not advance during the
simulation. That caused the software SCSP renderer to repeat
wave-then-silence patterns: the audible 'helicopter' flutter.

We executed **actual original MC68000 code** with the owner ROM
and a snapshot copied from the genuine first Akira music tick.
The first 0x2242 descriptor walk performed:

- 75 original instructions (first one followed by 74 until
  recurrence at PC `0x2242`);
- copied one **512-byte page** from sample ROM via the original
  stream logic, changing 510 previously zero sound-RAM PCM bytes;
- slot 0 source `0x8f96d4` -> `0x8f98d4`, destination
  `0x10200` -> `0x10400`;
- consumed no UART/ring command and returned with A0=`0x2020`.

Bit 7 in SCSP register `0x0409` was tested as a hardware flag:
forcing it ON in the independent snapshot gave the same
first 75-instruction result. No claim about all other paths.

Continuing the SAME original CPU registers and RAM through all
**32 descriptor iterations** took 588 instructions total, ended
with A0 wrapping to `0x2000`, and advanced the records for
**slots 0, 3, 4, and 7**. The walker changed **2,039 PCM bytes**
in those sample windows, and never used synthetic prefill.

## Actual original-walker musical playback (opt-in)

`tests/m68k/test_l3_real_rom_selections.c` now exposes:

```sh
mkdir -p out
build/vf2_m68k_l3_selection_tests /local/vf2 \
  --akira-original-walk-wav-dir out
```

The program interprets the **original Motorola 68000**
main-loop stream walker for each of 32 descriptors each
experimental 50-frame audio tick, with RAM/SCSP and all
stream source/destination cursors kept live across callbacks.
No 512-byte transfer is invented by C; whether a page is
copied and its exact source/destination come entirely from the
original-ROM CPU execution. This was explicitly kept
owner-ROM-only in a test harness, not promoted to production
`src/hardware/sound_board.c` or public-CI runs.

Measured from that owner-ROM run:

| Result | Owner-ROM interpreter output |
| --- | --- |
| Song command | `AD 10 04` (`Akira`, sound test #11) |
| Music span | 10 s (8820 experimental ticks, 50 PCM frames/tick) |
| Sound command dispatches | 456 |
| Original walker MC68000 instructions | 4,984,294 |
| Original walker PCM page copies | 2,160 |
| 16-bit stereo SCSP-model PCM frames | 441,000 |
| Non-silent stereo frames | 440,991 |
| Absolute raw PCM peak | 10,893 |
| Original walker return/status | all iterations completed |

The new output is named
`vf2_akira_10s_ORIGINAL_WALK_APPROX_TIMER_scsp_model.wav`.
An optional owner-local listening copy uses the same gain for
both channels (+7.84 dB to a safe 0.82 peak) with **no high-pass
filter**. The original file and listening copy stay local.

## Listening-diagnostic comparison

Using exactly the same analysis for both WAVs:
compute stereo RMS over 20 ms blocks, Welch power spectrum
at 50 Hz of that envelope, and the 6–11 Hz-band share of
the 0.5–40 Hz envelope-variation power:

| WAV | 6–11 Hz share |
| --- | ---: |
| Previous pitched/mixed preview, no streaming | 47.3% |
| Original-walker streaming preview | **22.2%** |
| Prior synthetic prefill hypothesis | 22.2% |

The authentic walker result and the prefill hypothesis also
have 0.996 PCM correlation after fitting a common gain;
they are **not byte-identical**, because copies occur at
different times. This supports (but cannot prove exhaustively)
that the missing walker was the dominant artifact in this
particular ten-second passage.

## Validation / bounds

The owner-only `--probe-akira-walk` prints original single-loop
results plus an independent full 32-descriptor scan. The
opt-in `--verify-akira-walk` and
`-DVF2_VERIFY_AKIRA_WALK=ON` CTest assert those exact original
ROM read/write effects without distributing the owner's assets.

The **original arcade scheduler cadence is not yet measured**.
Choosing one full walk per 50 output frames is a controllable
experiment, not a cycle-exact 68000/SCSP co-simulation.
The SCSP PCM interpolation, envelope, DSP/wet effects and
longer-than-10-second complete song still need separate checks.
Do not label this a hardware-perfect original recording.
