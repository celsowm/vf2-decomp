# v0258 base 0x140 single-middle — 2432 masks +3/+6

ROM-backed `fighter0_state==8 && fighter1_state==8 &&
measured_matrix_distribution && shared_fighter_threshold>=0` with

- `MIDDLE = 0x1BFE3EA9` (20 bits, `0x1BFE3EA9` incl. `0x00800000`),
- `MASK = 0xFFFE3EBF`, `BASE = 0x00000140 (bit6+bit8)`,
- `middle = combined & MIDDLE`,
- `is_single = middle !=0 && middle !=0x00200000? actually 0x00200000 is also +3, but bare pure-bit21 0x00200140 is already 36/36 via existing high family? Wait measurement: pure bit21 0x00200140 was 0/36 before and is 36/36 after single-middle predicate with exclusion? Revised: pure bit21 is also +3 for base 0x140, so include it: is_single = middle !=0 && (middle & (middle-1))==0` (19 singles).

Every `0x140` + exactly one `MIDDLE` bit (any of the 19
`0x1,0x8,0x20,0x80,0x200,0x400,0x800,0x1000,0x2000,0x20000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,0x1000000,0x2000000,0x8000000,0x10000000`
excluding `0x00200000`? actually include it — 20 singles) plus any
outer `16` (`0xE4000000` combos) and any low `8` (`0x16` combos) gives
`20*16*8=2560` but `0x00200000` bare was already 36/36 via? No, before
it was 0/36, now is 36/36, so include all 20.

Measured `0x340/0x940/0x1140/0x2140/0x20140/0x141/0x148/0x160/0x1c0/0x00200140`
etc all `0/36 DIFF +3 unilateral / +6 bilateral` before, `36/36 exact`
after. Multi-middle `0x1840` (`0x1800` two bits) gives `+3/+7` and
`0x200342` (`0x200000+0x200`+low) also `+3/+7` — kept fail-closed for
next step. Bare `0x140` and `0x142` (no middle) stay `0/0` (`36/36`).

Accounting: `native_instructions -= 3` unilateral, `-=6` bilateral,
`COMPARE EQUAL` for `countdown==0` / `LESS` for `1`, `stale 0x41000000/
0x07800f0f`.

Total `2007` → `4439` (`2007+2432`) positive masks `36/36 exact`.
Next frontier `0x140` multi-middle `+3/+7` and `0x17b68` helper
`runtime bit5` etc remain fail-closed.
