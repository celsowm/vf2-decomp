# v0261 base 0x4040 any-composition — 134,217,728 masks -2/-3

ROM-backed `fighter0_state==8 && fighter1_state==8 &&
measured_matrix_distribution && shared_fighter_threshold>=0` with

- `base = (combined & ~0xFFFE3EBF) == 0x4040` (0x4000+0x40)
- Any `MIDDLE 0x1BFE3EA9` (20 bits incl. bit23) subset, incl. bare, incl. bit21,
  any `low 8`, any `outer 16` — uniformly `-2` uni / `-3` bi (native undercounts):
  - Bare `0x4040`, single `0x4240`, high `0x44040`, bit21 `0x00204040`, many `0x1BDE6E49` all `0/36 DIFF -2/-3` → `36/36` after.
- Accounting: `native_instructions += bl?3:2` (add), `COMPARE LESS/EQUAL`, `stale_low 0x41000000`.

Counts: `2^20=1,048,576` *128=134,217,728.

Total `268,437,207 + 134,217,728 = 402,654,935` positive masks 36/36.

Next frontier: base `0x8040` etc and helper runtime bit5.
