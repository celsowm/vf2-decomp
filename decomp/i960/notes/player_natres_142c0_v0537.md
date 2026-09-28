# Player natres `0x14288` → `0x142c0` v0537

The existing player corridor was measured on the base park and already
reached `0x142c0` at `10869` instructions with ten calls and ten returns. The
`out/pre14288-natres.vf2snap` witness follows the same selector `0x505` stream
but preserves a distinct F0 shape at the head.

Reference measurements from the natres snapshot:

- `0x14288 → 0x1428c`: `1622` instructions, `+4` calls and `+4` returns;
- the final float/profile sequence writes `player+0xbdc = 0x20` and then
  clears `player+0xbdd` in the late `0x27130` tail;
- `0x1428c → 0x142c0`: `9247` instructions, `+6` calls and `+6` returns;
- total: `10869` instructions, `+10` calls and `+10` returns;
- the exact entry F0 word at the head is `0x80000882`, with record
  `0x0201c2fc` and the same five selectors
  `0x0505/0x0039/0x00f1/0x00e7/0x00af`.

The recovery adds only the measured `player+0xbdd` clear and the exact natres
F0 admission. The fixture runs both `out/pre14288.vf2snap` and
`out/pre14288-natres.vf2snap`, comparing registers, condition state, frames,
procedure counters and mutable Model 2A memory through `0x142c0`.

No general F0 relaxation is inferred; other flag words, record/scratch shapes
and selector compositions remain `VF2_ERROR_UNSUPPORTED`.
