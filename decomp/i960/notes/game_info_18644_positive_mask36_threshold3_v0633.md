# v0633 — `fa_game_info` positive mask `0x36`, threshold 3

## Measured question

Can the no-bit-8 state-8 compositions `0x00000036` (bits 1+2+4+5),
`0x0000003a` (bits 1+3+4+5) and `0x0000003e` (bits 1+2+3+4+5) use the existing native mixed-low
child at threshold 3 without widening unmeasured thresholds or distributions?

## Evidence

The full-dispatch validator was run with the repository ROM set for:

```text
mask       0x00000036, 0x0000003a, 0x0000003e
threshold  3
records    fighter-0 only, fighter-1 only, bilateral
countdown  0 and 1
mode bit 6  clear and set
```

Before the admission, all 12 cases for each mask reached the native corridor but differed
only in the dispatcher join: `+3` instructions for countdown zero and `-2`
for nonzero countdown. The reference and native snapshots differed only in
the compare-result byte.

After reusing the already measured mixed-low correction, all **12/12** cases
match in CPU state, condition state, local-frame state, procedure counters and
mutable Model 2A memory. The threshold-4 control remains unsupported in all
12 cases, so the recovery is bounded to the measured threshold-3 slice.

## Recovery

`hybrid_execute_game_info_bit31_native` admits `0x36`, `0x3a` and `0x3e` only for the
measured matrix distribution and threshold `<= 3`. The existing mixed-low accounting
sets the countdown-derived EQUAL/LESS condition and applies the `+3`/`-2`
instruction join. No field is assigned a semantic name and no ROM-derived
artifact is stored.
