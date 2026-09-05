# Input polarity + phase-11 driven path — v0270

## Summary

The first driven-input workstream after the `fa_coli` v0269 negative hunt found
an emulated Model 2A input-polarity bug before it found gameplay `fa_coli`.
The ROM already performs the active-low inversion for the P1/P2 input ports;
`model2a.c` was inverting those ports a second time.  With the hardware-facing
port values corrected, the accepted startup/repeated-frame corridor changes
slightly but remains completely native and strict-equal.

This slice also closes the first newly exposed input branches, including the
phase-17 index-11 countdown/reset path reached by driven PUNCH/release input.
This is a diagnostic/control path, not yet evidence that a playable match has
started, and it does not arm recurring `fa_coli 0x221e8`.

## Input-port evidence

The corrected host-input values sampled by the original ROM are:

| host state | sampled word at `0x00500700` |
|---|---:|
| neutral | `0x0f000000` |
| COIN | `0x0f000001` |
| START | `0x0f000010` |
| P1 PUNCH | `0x0f000100` |
| P1 UP | `0x0f002000` |

The input sampler at `0x1064` derives the edge word at `0x00500704` as a
newly-enabled mask.  `vf2cycles --input` and `vf2probe --input` now allow the
same host input to be held while resuming a proven snapshot, so presses and
releases can be driven at cycle boundaries without mutating game state.

## Newly exposed native corridor

Correcting the input polarity changed the accepted post-frame topology.  The
strict ROM-backed dispatch counts are now:

- third: `42` blocks / `55,236` instructions;
- fourth: `78` blocks / `58,863` instructions;
- fifth: `830` blocks / `7,402,732` instructions;
- sixth: `866` blocks / `7,404,901` instructions.

All remain `MATCH` with zero interpreted instructions.

The interrupt initial cluster now returns from the texture dispatcher and
executes the fighter-compare prefix through `0x0c78` instead of relying on the
old double-inverted input state.  The recovered prefix is fail-closed on the
unmeasured unequal/player-bit paths.

## Callback table semantics

Driven PUNCH and LEFT exposed the `0x1284` input callback table.  The first byte
is a bit index, not a literal mask: the ROM compares `1 << table[0]` with the
masked newly-enabled input.  On equality it advances to the next table byte;
on mismatch it reinstalls the `0x1284` fallback.  The recovered code now models
that measured rule and keeps other table shapes fail-closed.

## Phase-17 index 11

A measured PUNCH/release sequence reaches flagged phase index `0x8b` and the
`0x5ef60` phase-11 path.  First visit arms the counter at `0x00500024` to `320`.
Subsequent frames execute the `0x5f060..0x5f078` countdown.  For a positive
post-decrement counter, the final `cmpibge 0,r3` falls through and the caller
sees `LESS`, not `EQUAL`.

Strict differential evidence:

- first corrected countdown frame: `36` blocks / `2,759` instructions, MATCH;
- next `318` driven cycles: MATCH throughout (batched 20 + 100 + 100 + 50 + 48);
- terminal frame reaches the reset target `0x000000b0` on both reference and
  native sides after `14,486` recovered instructions.

The terminal path also exposed warm-reset poststates that had previously been
masked by cold-boot assumptions:

- cold post-boot prefix enters `0x52c` at local-frame depth 0 and leaves the
  measured delay condition `EQUAL`;
- phase-11 warm reset enters the same prefix with three live frames and leaves
  the measured boundary `LESS`;
- `timer-wait-update` must preserve its timer-delta comparison instead of
  forcing `EQUAL`;
- the `0x4afe4` texture wait poll updates condition state for its frame-byte
  comparison before an IRQ is injected;
- the idle interrupt-save path to `0x0c80` exposes `NONE` condition state.

## Remaining frontier

The phase-11 reset itself is recovered.  Continuing the warm boot under strict
per-block differential currently exposes a synchronization granularity issue in
the frame-wait oracle: during the early `0x0f7c` wait, the reference
instruction-by-instruction observer injects the IRQ early enough to execute the
`mov sp,r3; lda 0x40(sp),sp` handler prefix and stops at `0x0bc8`, while the
native frame-wait step ends at the interrupt entry `0x0bc0`.

Do not hide this with relaxed comparison.  Either make the frame-wait step
account for that measured two-instruction IRQ prefix or make reference/native
interrupt injection boundaries identical.

Recurring `fa_coli 0x221e8` remains open.  The corrected input infrastructure
is now trustworthy, but this phase-11 diagnostic path does not produce the
`entry=0x221e8 + runnable` conjunction required by the v0269 scheduler proof.
