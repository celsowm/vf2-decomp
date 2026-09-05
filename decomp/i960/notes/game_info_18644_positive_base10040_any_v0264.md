# v0264 base 0x10040 any-composition — 134,217,728 masks -4/-7 + work-RAM 0x510b24/0x512b24

ROM-backed `fighter0_state==8 && fighter1_state==8 &&
measured_matrix_distribution && shared_fighter_threshold>=0` with

- `base = (combined & ~0xFFFE3EBF) == 0x10040` (0x10000+0x40)
- Any `MIDDLE 0x1BFE3EA9` (20 bits) subset, incl. bare, incl. bit21,
  any `low 8`, any `outer 16` — uniformly `-4` uni / `-7` bi (native undercounts):
  - Bare `0x10040`, single `0x10240/0x11240`, bit21 `0x00210040`, many `0x30040/0x50040` all `0/36 DIFF -4/-7` → `36/36` after.
- Reference also sets work-RAM:
  - `*(0x00510b24) = combined|0x800` if fighter0 has mask
  - `*(0x00512b24) = combined|0x800` if fighter1 has mask
  - Bilateral writes both; seen `0x10840` for bare, `0x10a40` for `0x10240` etc.
  - Verified via `vf2probe --memory-trace` on `full-dispatch` start snaps: f0 writes `0x510b24`, f1 writes `0x512b24`, bi both.
- Accounting: `native_instructions += bl?7:4`, `COMPARE LESS/EQUAL`, `stale_low 0x41000000`, plus per-fighter work-RAM.

Counts: `2^20=1,048,576` *128=134,217,728.

Total `671,090,391 + 134,217,728 = 805,308,119` positive masks 36/36.

Next frontier: bases `0x14040` (-2/-3), `0x18040` (-4/-8), `0x1C040` (-2/-4) and helper runtime bit5.
