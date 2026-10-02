# Fighter-candidate offsets from whole-task `fa_coli` traces (v0696)

Two full whole-task `fa_coli` reference traces (parked snapshot
`out/coli-parked-221e8.vf2snap` → `0x00010dcc`, both fighter bit-8
flags set, `field_0820 = 0`, `0x005149cc = 0xffff`), captured with
`vf2probe --trace --memory-trace` and analyzed with
`tools/python/infer_structs.py --base fighter0=0x00510980 --base
fighter1=0x00512980`:

- case A: F0 scan 6 / F1 scan 0 (9524 steps, 3123 accesses);
- case B: F0 scan 2 / F1 scan 2 (9534 steps, 3125 accesses).

## Stable new offsets (both bases, both cases, same width and R/W role)

| offset | width | role | guest IPs |
| --- | --- | --- | --- |
| +0x0018 | 4B | R+W | 0x2380c, 0x2381c, 0x23824, 0x23834 |
| +0x0020 | 4B | R+W | 0x23810, 0x23820, 0x23828, 0x23838 |
| +0x0808 | 2B | R | 0x238b8, 0x22440 |
| +0x0820 | 1B | R | 0x238c0, 0x22450 |

These meet the header inclusion criteria and are added as neutral
`field_XXXX` entries (no semantic names). Previously known offsets
(+0x0000, +0x0004, +0x01a4, +0x01a8, +0x01aa, +0x01f4, +0x01f8,
+0x01fc, +0x0644, +0x064c, +0x0650, +0x06dc, +0x0821) reproduce with
identical widths/roles in both traces.

## Corridor-dependent widths (NOT changed)

The whole-task corridor touches +0x0644/+0x064c/+0x0650 and the
+0x0d00 cluster as 4B words (ips 0x23bac/0x23bb0, 0x2399c/0x23a38 —
the 30-trip poly-cluster copy), while the header records 2B from the
v0387 corridor. Width at these offsets is corridor-dependent, so the
existing `VF2_FIGHTER_WIDTH_*` values and `uint16_t` struct fields
are left untouched pending a same-corridor width proof.

## Also added (missing defines only, no new evidence)

`VF2_FIGHTER_OFF_01F8/_0644/_064C/_0650/_0D04/_0D08`: struct fields
already existed; only the offset defines were missing.
