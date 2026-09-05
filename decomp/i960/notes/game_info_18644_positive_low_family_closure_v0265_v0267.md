# v0265-v0267 remaining low bases any-composition — 3*134,217,728 masks

ROM-backed `fighter0_state==8 && fighter1_state==8 &&
measured_matrix_distribution && shared_fighter_threshold>=0` with

- `base = (combined & ~0xFFFE3EBF)` in `{0x14040,0x18040,0x1C040}`
  - `0x14040` (0x10000+0x4000+0x40) `-2/-3`
  - `0x18040` (0x10000+0x8000+0x40) `-4/-8` plus work-RAM `0x510b24/0x512b24|=0x800` (same as 0x10040)
  - `0x1C040` (0x10000+0x4000+0x8000+0x40) `-2/-4`
- Any `MIDDLE 0x1BFE3EA9` (20 bits) subset, any `low 8`, any `outer 16` — per-base `-2/-3` etc.
  - Verified bare `0x14040/0x18040/0x1C040`, singles `0x14240/0x18240`, bit21 `0x00214040/0x00218040` etc all `0/36 DIFF` → `36/36`.
  - `0x18040` work-RAM same as `0x10040`: writes `0x18840` for bare to `0x510b24`/`0x512b24` (byte `0x88` at `0x10b25`/`0x12b25`).
- Accounting: `native_instructions += bl?3:2` etc, `COMPARE LESS/EQUAL`, `stale_low 0x41000000`.

Counts: each `2^20*128=134,217,728`, total `402,653,184` for three bases.

With `0x10040` v0264, the low `0x40` family (all 16 combos of `0x100/0x4000/0x8000/0x10000` with bit6) is now fully closed:
`0x40,0x140,0x4040,0x8040,0xC040,0x10040,0x14040,0x18040,0x1C040` plus the 7 middle-high bases `8140` etc = all 16.

Total `805,308,119 + 402,653,184 = 1,207,961,303` positive masks 36/36.

Next frontier: helper `runtime bit5` at `0x17b68` and any remaining positive compositions outside low family.
