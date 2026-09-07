# Recurring `fa_game_disp` corridor through frame 30 — 2026-09-07

This note records the exact differential validation of the recurring gameplay
corridor after extending the measured `fa_game_disp` state through frame 30 and
restoring the resumed VBlank poll phase.

## Validated corridor

The native runtime was resumed at the measured frame-7 `fa_game_disp` entry
(`0x0002b1f8`) and allowed to execute 898 recovered blocks continuously. The
run reaches the frame-30 `fa_game_disp` return at `0x00010dcc` and crosses the
measured scheduler regime change between frames 22 and 23 without reinjection
or interpreter fallback.

Three independently measured game-display states were validated:

- P1 event state;
- P2 event state; and
- bilateral event state.

## Resumed VBlank phase

Snapshots serialize CPU and Model 2A state, but the host-side frame scheduler
phase is runtime sidecar state. A mid-game resume with balanced, nonzero i960
interrupt-entry/interrupt-return counters is already one poll into the observed
four-visit VBlank period.

Commit `7df13561cfbfd2dd25d9c7023d523f05692d95f7` restores that phase at the
`0x00010f90` frame-wait entry. The normal four-visit period is unchanged; only
the first resumed period is seeded one visit ahead.

Before the fix, CPU/memory snapshots matched the ROM but the native cumulative
instruction counter was exactly two instructions high. The first divergence
was the `0x00010f90 -> 0x00000bc0` VBlank block: the ROM accounted 5
instructions while the resumed native path accounted 7.

After the fix, the native path accounts the same 5 instructions at that first
resumed VBlank and remains exact through frame 30.

## Official artifact

Validation used the exact `vf2i960-linux` artifact from GitHub Actions CI run
`34153165435` for commit
`7df13561cfbfd2dd25d9c7023d523f05692d95f7`:

- artifact id: `10030053634`;
- CI result: success;
- `decomp.dev progress` run `34153165335`: success.

## Final frame-30 counters

The reference interpreter was advanced from the same frame-7 entry snapshots
to build the ROM oracles. Native and reference snapshots then matched exactly.

| State | IP | Instructions | Calls | Returns | IRQ entries | IRQ returns | Snapshot |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| P1 ROM | `0x00010dcc` | 14,432,407 | 13,215 | 13,214 | 51 | 51 | oracle |
| P1 native | `0x00010dcc` | 14,432,407 | 13,215 | 13,214 | 51 | 51 | exact match |
| P2 ROM | `0x00010dcc` | 14,432,409 | 13,215 | 13,214 | 51 | 51 | oracle |
| P2 native | `0x00010dcc` | 14,432,409 | 13,215 | 13,214 | 51 | 51 | exact match |
| bilateral ROM | `0x00010dcc` | 14,432,474 | 13,216 | 13,215 | 51 | 51 | oracle |
| bilateral native | `0x00010dcc` | 14,432,474 | 13,216 | 13,215 | 51 | 51 | exact match |

The comparison covers CPU state, local frames, mutable Model 2A memory and
modeled device-visible state. The explicit counter check additionally confirms
instruction, procedure call/return and interrupt entry/return accounting.

## Frontier

Frame 30 is therefore no longer a validation boundary for the measured
recurring corridor. Continuing from its output executes the scheduler and
reaches the next `fa_game_disp` entry at `0x0002b1f8`; the current native guard
then fails closed because the next measured global value is outside the
accepted frame window. That is the next recovery frontier rather than a VBlank
or scheduler mismatch.
