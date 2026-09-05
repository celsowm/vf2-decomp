# v0263 base 0xC040 any-composition — 134,217,728 masks -2/-4

ROM-backed `fighter0_state==8 && fighter1_state==8 &&
measured_matrix_distribution && shared_fighter_threshold>=0` with

- `base = (combined & ~0xFFFE3EBF) == 0xC040` (0x4000+0x8000+0x40)
- Any `MIDDLE 0x1BFE3EA9` (20 bits) subset, incl. bare, incl. bit21,
  any `low 8`, any `outer 16` — uniformly `-2` uni / `-4` bi (native undercounts):
  - Bare `0xC040`, single `0xC240`, bit21 `0x0020C040`, many `0x1BDECE49` all `0/36 DIFF -2/-4` → `36/36` after.
- Accounting: `native_instructions += bl?4:2`, `COMPARE LESS/EQUAL`, `stale_low 0x41000000`.

Counts: `2^20=1,048,576` *128=134,217,728.

Total `536,872,663 + 134,217,728 = 671,090,391` positive masks 36/36.

Next frontier: bases `0x10040` (-4/-7), `0x14040` (-2/-3), `0x18040` (-4/-8), `0x1C040` (-2/-4) and helper runtime bit5.
