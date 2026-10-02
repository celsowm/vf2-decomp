# 0x28184 head siblings (v0707, native)

## Oracle tree (tmp-lift-27ce0 park, --set-ip 0x27d00, counter +0x1aa == 1)

Head (`0x28184 ldos +0x1aa -> g6; 0x28188 ld +0xbd8 -> r6;
`0x2818c lda +0x690 -> g5; 0x28190 ld 0x500068 -> r10`):

- bit17 clear: `bbc-17` taken -> `0x281c8` (b0/edge irrelevant).
- bit17 set: `ldos 0x500092 -> r14`; bit0 clear: `bbc-0` taken ->
  `0x281c8`; bit0 set: `ldos +0x0800 -> r13`, `cmpobe g6,r13`:
  equal -> `0x281c8`, not-equal -> float path (`0x281b0 cvtir ...`,
  `call 0x2891c`).

Merge `0x281c8`: bit20 clear -> `0x28208` (`ld +0x854 -> r10`,
`cmpobe 0,r10 -> 0x2825c` iff curve == 0); bit20 set -> `0x281cc`
(`cmpobe 1,g6` taken, counter == 1) -> `0x2825c`. Tail `0x2825c`:
`cmpobne 1,g6` (not-taken, counter == 1), `ldob +0xbdd -> r15`,
`bbs 0,r15` (falls through iff status bit0 clear) -> `0x28268`
(`call 0x28780`).

Measured call-through-`0x28268` totals (bit-20-clear, curve 0):

| bit17 | bit0 | edge==counter | steps | outcome |
| 0 | - | - | 12 | `0x28268` (S0, was native) |
| 1 | 0 | - | 14 | `0x28268` (S1, NEW native) |
| 1 | 1 | yes | 16 | `0x28268` (S2, NEW native) |
| 1 | 1 | no | 10 to `0x281b0` | float path, stays unsupported |

Bit-20-set S0 row traced (11 steps: `0x281c8 -> 0x281cc ->
`0x2825c` tail), confirming the pre-existing +10 native cost.

## Native (`hybrid_execute_player_28184_prefix`, chained after
`hybrid_execute_player_27d00_call` in dispatch)

- S1: +2 steps (native +13), `r14 = u16(0x500092)`.
- S2: +4 steps (native +15), `r14` + `r13 = u16(+0x0800)`.
- Tail `cmpobe`-taken pins EQUAL on every fighter: the old F1-only
  NONE clear is retired (F1 S0 oracle row reads EQUAL).
- Two latent holes closed with new gates (fail closed):
  `status_byte` bit0 set (`bbs`-taken shape was accepted as
  `0x28268` without proof); sibling 1/2 with bit20 set
  (unmeasured combo, previously admitted with S0 costs).
- Float fall-through, `counter != 1`, bit20-clear `curve != 0`
  remain `VF2_ERROR_UNSUPPORTED`.

## Proof

`test_player_28184_head_matrix_rom_pin` (6 rows: 5 F0 + F1 S0
readout; nonzero `sense`/`edge` values prove the `r14`/`r13` loads
and the irrelevance claims) asserts reference ip/steps plus full
live-state equality; `test_player_28184_float_fail_closed_rom_pin`
asserts the reference reaches `0x281b0` in 10 while native refuses.
Strict build + direct ROM binary green.
