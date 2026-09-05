# v0258 frontier: base 0x140 + middle  (fail-closed, not yet admitted)

Masks with `base = 0x00000140 (bit6+bit8)` plus a single middle bit
from `0x1BFE3EA9` (e.g. `0x340` bit9, `0x940` bit11, `0x1140` bit12,
`0x2140` bit13) are `0/36 exact` with `counters +3 unilateral / +6
bilateral` (multi-bit `0x1840` gives `+3/+7`). Reference is `+3/+6`
(`+7` for ≥2 middle bits), native is `0` (fall-through), so `36/36`
requires `native_instructions -= 3` / `-=6` (`-7` for multi-bit) plus
`stale`/`COMPARE EQUAL/LESS` as for other positive families.

Special middle bits keep `0` excess:
- `0x8000` (bit15) is `base 0x8140` (`-3/-5`, already recovered v0254)
- `0x00200000` (bit21) bare `0x00200140` is `0/0` (no correction)

A compact `base 0x140` middle predicate would be
`(combined & MIDDLE)!=0 && (combined & ~0xFFFE3EBF)==0x140`
but per-middle accounting splits `+6` vs `+7` vs `0` vs `-3`,
so a single uniform `+3/+6` would over-correct `0x200140` and
`0x8140`-derived masks. Need per-middle clustering (single vs multi
vs high-15 vs high-21) before admission.

Measured via
`validate_game_info_full_dispatch.py --mask 0x340/0x940/0x1140/0x2140/0x1840`
(`--state 8`, `36` cases): all `0/36 DIFF +3/+6` (`+7` for `0x1840`),
`0x200140` `36/36 exact`, `0x140` bare `36/36 exact`. Helper
`0x17b68` `runtime 0x00508000 bit5` early-exit also measured
(`bit7=1` + `bit5=1` → `6` fewer to `0x10dcc`, `3450` fewer to
`0x18544` vs `bit7=0` path) — also fail-closed.

Keep fail-closed until a measured compact rule is proven.
