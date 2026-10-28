# v0944 — atomic first Akira L2 descriptor worker (opt-in)

## Recovered boundary

This change composes the nine existing native micro-probes for the first
Akira music descriptor, from the first B-class packet through the original
68000's **pre-RTS PC 0x2158**. Its original owner-ROM fixture established:

- Fourteen emitted three-byte ring packets (the fourth byte of each
  four-byte ring entry remains unchanged).
- Stream pointer at descriptor +4: `0x615773 -> 0x61579C`.
- Countdown word at descriptor +2: `0x16C7`.
- Status at descriptor +0: `0x88`, pending count +14 and cursor +56,
  with circular 4 KiB ring wrapping.
- No modeled SCSP side effects in the isolated first worker.

**No extension of the known instruction boundary is claimed.** Original
assembly beyond the worker's return, the enclosing L2 scan, subsequent
music descriptors, the interrupt return, accurate SCSP scheduling and
DAC-accurate playback are still unproven.

## Native API and atomicity

`vf2_sound_board_probe_akira_l2_first_worker` is an explicit, optional
developer probe; it does not modify `vf2_sound_board_maintain_streams`.
It applies the same nine proven primitives to a disposable full board
copy and commits RAM/SCSP only if the entire chain and final counters
succeed. Failed ROM fingerprints, malformed ring history, late-stage
source mismatches, pending-counter overflow and allocation failure do
not partially emit packets into the caller board.

The report exposes start/end stream, ring cursor, pending count,
countdown and number of emitted packets. No original ROM data or
derived ROM image is checked into Git.

## Regression contract

The public ROM-free `test_sound_board` now compares the single atomic
call byte-for-byte against sequential native micro-probes. It also
checks late-stage source corruption refusal, full-counter refusal,
fourth-byte preservation and a 0xFFC->0x000 circular write.

The owner-only `test_cdispatch` copies the starting state, executes
original 68000 instructions to PC `0x2158`, and compares its complete
RAM and modeled SCSP against both the nine primitives and the
single atomic call. This test **requires** the locally supplied owner
ROM; its success must not be inferred from public CI.

## Next actual recovery frontier

Run the owner-ROM differential, then step the 68000 from `0x2158`
through RTS into the **enclosing L2 descriptor scan**. Determine scan
counter updates, subsequent descriptor handling, SCSP writes and the
true end-of-interrupt boundary. Only then consider enabling an
opt-in full tick in `maintain_streams`. Do not claim playable music
or audio-fidelity parity from this first worker.
