# v0260 base 0x40 any-composition — 134,217,728 masks +3/+7

ROM-backed `fighter0_state==8 && fighter1_state==8 &&
measured_matrix_distribution && shared_fighter_threshold>=0` with

- `base = (combined & ~0xFFFE3EBF) == 0x40` (bit6 alone)
- Any `MIDDLE 0x1BFE3EA9` (20 bits incl. bit23) subset, incl. bare, incl. bit21,
  any `low 8` (0x02/0x04/0x10 combos), any `outer 16` — uniformly `+3` uni / `+7` bi:
  - Bare `0x40`, single `0x240/0x840`, double `0x1840`, bit21 `0x00200040`,
    bit21+single `0x00200240`, bit21+double `0x00201840`, low `0x42`,
    many `0x1BDE3EE9/0x1BFE3EE9` all `0/36 DIFF +3/+7` → `36/36` after.
- Accounting: `native_instructions -= bl?7:3` (native overcounts), `COMPARE LESS/EQUAL`, `stale_low 0x41000000`.

Counts: `2^20=1,048,576` *128=134,217,728 (outer 16 * low 8 * middle 2^20).

Total `134,219,479 + 134,217,728 = 268,437,207` positive masks 36/36.

Next frontier: base `0x100` (-3) and other bases (0x44, 0x100+ etc) plus helper runtime bit5.
