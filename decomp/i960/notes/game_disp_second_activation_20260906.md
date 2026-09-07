# `fa_game_disp` second activation recovery — 2026-09-06

This note records the completed clean-room recovery of the measured baseline
second activation of `fa_game_disp`.

## Proven corridor

- task: `fa_game_disp`
- continuation entry: `0x0002b1f8`
- scheduler return: `0x00010dcc`
- functional SHA: `1ed601196e6297b3053484fe403ae4b5514f89df`
- CI run: `34073149409`
- Linux analysis artifact: `10001136866`
- artifact digest: `sha256:6ad37962ea7fac013e324537ee5ed2ec2f0f9abde10a417d3fd80b02fce618f0`

The complete measured corridor executes as one recovered native block:

- instructions: **2903 / 2903**
- procedure calls: **41 / 41**
- procedure returns: **42 / 42**
- final CPU + memory snapshot: **exact match**

The strong comparison covers CPU registers, condition state, all local-frame
slots (including stale frames), counters, Work RAM, Tile RAM, geometry memory,
coprocessor/device state and the remaining snapshot regions.

## Recovered composition

The 2903-instruction path is composed from these measured children plus the
23-instruction parent corridor:

| Component | Instructions |
| --- | ---: |
| fighter leaf `0x0002c5f0`, fighter 0 | 15 |
| fighter leaf `0x0002c5f0`, fighter 1 | 15 |
| numeric HUD counter `0x0002cae0` | 353 |
| HUD fast gate `0x0002c770` | 8 |
| HUD clear branch `0x0002b27c` | 458 |
| event-queue empty gate `0x0002ab94` | 38 |
| composed main display child `0x0002c968` | 1993 |
| parent corridor | 23 |
| **total** | **2903** |

The parent makes seven direct calls. The recovered children account for the
remaining nested calls, yielding the exact observed totals of 41 calls and 42
returns after the parent `ret`.

## Main display child

`0x0002c968` is itself completely recovered for the measured baseline path:

- instructions: **1993 / 1993**
- calls: **28 / 28**
- returns: **29 / 29**
- strong snapshot: **exact match**

It composes previously proven primitives and children, including:

- the 1241-instruction HUD body at `0x0002c9b4`;
- the two 6x2 fighter resource blits at `0x0002cc8c`;
- the four-symbol 2x3 numeric formatter at `0x0002cba0`;
- the final 16x1 resource blit;
- the measured countdown/epilogue state update.

The marker-glyph path preserves the measured distinction between stride-2
2x2 markers and stride-10 numeric glyphs.

## Validation

A fresh ROM oracle snapshot was generated locally from the continuation entry
through `0x00010dcc`. The exact `vf2i960` artifact built from the functional
SHA was then run from the same entry with the native runtime enabled.

Observed native report:

```text
Native resume: blocks=1 instructions=2903 entry=0x0002b1f8 exit=0x00010dcc task=fa_game_disp
  calls=41 returns=42 fighter_flags_or=0xffffffff
```

`compare-snapshots` reported:

```text
Snapshots match.
```

The same functional SHA passed:

- GCC release build/tests;
- Clang release build/tests;
- Clang ASan/UBSan build/tests;
- Python tooling/frontier tests.

## Scope

This closes **100% of the measured baseline second-activation corridor**. It
does not claim that every alternate branch/state of the full `fa_game_disp`
routine family is recovered. Guards remain deliberately narrow and unsupported
states continue to fail closed to the previous runtime/ROM path.

No proprietary ROM data, generated snapshots, traces or extracted assets are
committed to the repository.
