# v0262 base 0x8040 any-composition — 134,217,728 masks +3/+6

ROM-backed `fighter0_state==8 && fighter1_state==8 &&
measured_matrix_distribution && shared_fighter_threshold>=0` with

- `base = (combined & ~0xFFFE3EBF) == 0x8040` (0x8000+0x40)
- Any `MIDDLE 0x1BFE3EA9` (20 bits) subset, incl. bare, incl. bit21,
  any `low 8`, any `outer 16` — uniformly `+3` uni / `+6` bi (native overcounts):
  - Bare `0x8040`, singles `0x8240/0x8440/0x9040`, high `0xA040`, bit21 `0x00208040`, many `0x1BDE8E49` all `0/36 DIFF +3/+6` → `36/36` after.
- Accounting: `native_instructions -= bl?6:3`, `COMPARE LESS/EQUAL`, `stale_low 0x41000000`.
  Shares `+3/+6` with base `0x140`.

Counts: `2^20=1,048,576` *128=134,217,728.

Total `402,654,935 + 134,217,728 = 536,872,663` positive masks 36/36.

Next frontier: bases `0xC040` (-2/-4), `0x10040` (-4/-7), `0x14040` (-2/-3), `0x18040` (-4/-8), `0x1C040` (-2/-4) and helper runtime bit5.
