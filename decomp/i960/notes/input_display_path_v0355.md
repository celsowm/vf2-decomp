# Input-driven display path (v0355)

## Setup

Base park: `out/sixth-fresh.vf2snap` (`native-sixth-dispatch` MATCH).
Cycles: `vf2cycles --input <mask> --min-blocks 1 --max-blocks 4096`.
Display decoded from tile-ram `0x80xx` words (host-side, not Model2 pixels).

## Measured trajectories (strict differential)

| Input | Cycles | Insns (run) | Final display | Selector / a4 / countdown |
| --- | ---: | ---: | --- | --- |
| COIN+START `0x180` | 12 | 26,010 MATCH | **TEST MENU** intact | `0x11` / `0x0b` / `639` |
| PUNCH `0x10` | 12 | 57,785 MATCH | **EXIT TEST MODE** | `0x11` / **`0x8b`** / **310** (`a5=0xff`) |
| PUNCH `0x10` | +20 (32) | 50,318 MATCH | EXIT TEST MODE | `0x8b` / countdown **290** |
| PUNCH `0x10` | +300 (332) | 14,929,632 MATCH | **TEST MENU** redrawn | `0x11` / `0x0b` / **0** |

Headline: **332** PUNCH cycles from sixth complete under native lockstep
(**12,216** compared blocks in the 300-cycle leg alone; park insns
`29,315,178`). The oracle **naturally** drives PUNCH → EXIT TEST MODE →
countdown expiry → **back to TEST MENU**. No `SEGA` tile appears.

COIN/START does **not** leave the operator menu in this state.

## Geometry / buffer streams

`out/dump_geometry_fifo.py` on `park-warm-sel3b`, `park-exit-8b` and
`sixth-fresh` shows the **same** geometry-RAM packet pattern (small
counts + `0x8808`/`0x101` tags). `buffer-ram` `0x00900000` holds the same
color/intensity ramp (`1.0, 0.794, 0.63, …` with tags `0x00ef3f7f` /
`0x00ee5f5f` / `0x00eeaa80`) in all three parks — palette/lighting-like
data, **not** a unique Sega-logo mesh. Ring pointers differ; payloads do not.

## Fail-closed conclusion

With this ROM + backup/config state, attract/Sega logo is **not** the
reached display. Native C **does** run the operator EXIT TEST MODE corridor
under strict differential. Logo remains an open frontier (other game-assign /
TGP attract state), not a tile witness on this trajectory.

Parks: `park-sixth-coin12`, `park-sixth-punch12`, `park-punch32`,
`park-punch-332` (+ `.runtime` sidecars). Tools: `out/dump_geometry_fifo.py`,
`out/dump_buffer_ram.py`.
