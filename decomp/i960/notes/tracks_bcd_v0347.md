# v0347: tracks B–D — player park, dispatch 12+, site-B-only gate

## Verdict

Three autonomous slices measured and wired where evidence allowed.

- **Track B (fa_player `0x19ef8`)**: measurement-only. A live-valid
  drive now exists on `punch10` for selector `0x4505`, but the
  accepted warm `0x505` corridor still faults on every available
  park. Recovery stays fail-closed.
- **Track C (native dispatch 12+)**: `native-nth-dispatch` MATCH
  through dispatch **40** on this build (regime transition 34→35
  confirmed). No hard boundary in range. CTest now also pins
  dispatch **12**.
- **Track D (coli site-B-only)**: the `0x22dd4` board-bit-9-clear
  gate is wired natively using the already-proven `0x502a4`#siteB
  and `0x7fc0`#4 helpers. Probe span measured.

## Track B — player `0x19ef8`

### Parks probed

| Park | Drive | Result |
| --- | --- | --- |
| `player-14288-rt` | `--set-ip 0x14288 --set-reg g0=0x4505` | fault `0x2705C` after 985 steps (v0342 blocker unchanged) |
| `punch10` | `--set-ip 0x14288 --set-reg g0=0x505 --set-reg g7=0x510980` | fault `0x287A0` after 1781 steps |
| `punch10` | `g0=0x4505`, `F0+0x1a4=0x20` | **OK** to `0x1428c`, **1745** steps |

### punch10 bit-14 complete corridor (reference)

```text
vf2probe --rom-dir roms/vf2 --snapshot out/punch10.vf2snap \
  --set-ip 0x00014288 --set-reg g0=0x4505 --set-reg g7=0x00510980 \
  --set-u32 0x00510b24=0x20 --until 0x0001428c --max-steps 5000
```

Observed final memory (LE words):

| Address | Value |
| --- | --- |
| `F0+0x0` (`0x510980`) | `0x04000400` |
| `F0+0x1a4` (`0x510b24`) | `0x200` |
| `F0+0xbe4` | `0` |
| `F0+0x5cc` | `0` |
| `F0+0xbd4` / `F0+0xbd8` | `0` / `0` |

The path does **not** visit `0x2705C`. Count **1745** is the
punch10-state shape; it is **not** the predicted `1652+5=1657`
from the degenerate `player-14288-rt` park (different fighter
scratch/float state). C `hybrid_execute_player_19ef8` still
admits only selectors `0x505` / `0x284` and keeps bit-14
siblings fail-closed.

### Unblock still required for native `0x4505`

1. A park where the **accepted** `0x505` corridor completes under
   reference (needed to prove the shared tail), **or**
2. Multiple measured `0x4505` shapes from the same park with
   identical tail behavior to a proven `0x505` baseline.

Do **not** implement the v0342 recipe (`selector==0x4505`,
`+0x1a4 ⊆ {5,6,21}`, `selector&=0x1fff`, prologue `+5`) until
those counts are locked against reference on one live state.

## Track C — native dispatch 12+

Measured on MSVC Debug, `VF2_ROM_DIR=roms/vf2`:

```text
native-nth-dispatch 12 → MATCH, 37 blocks / 2169 insns
native-nth-dispatch 20 → MATCH, 37 blocks / 2169 insns
native-nth-dispatch 40 → MATCH
  dispatch 34: 2163 insns MATCH
  dispatch 35: 1563 insns MATCH  (regime transition)
  dispatch 36–40: 1561/1566/… MATCH
```

Endurance note (`native_dispatch_endurance.md`) previously reached
dispatch 3749 MATCH; this build shows no hard boundary by 40.
CTest registers `vf2_native_twelfth_dispatch` alongside the
existing eleventh pin.

`frontier.py` on `out/state8-corpus-small/manifest.jsonl`
(6 cases, 154 edges) produced **no non-recovered ranked edges** —
that corpus sits entirely inside recovered ranges. Working
frontier remains the coli/player sibling set in
`docs/UNCOVERED_BRANCHES.md`, not a dispatch gap.

## Track D — coli site-B-only gate

### ROM (from `vf2i960 disasm roms/vf2 0x22dd4`)

```text
0x22dd4 ldob 0x6d9(g8), r3
0x22dd8 addo 1, r3, r3
0x22ddc stob r3, 0x6d9(g8)
0x22de0 ld   0x00508000, r15
0x22de8 bbs  9, r15, 0x00022e24
0x22dec lda  0x0100085e, g9
0x22df4 lda  0x00503200, g1
0x22dfc st   r3, (sp)
0x22e00 mov  1, r15
0x22e04 balx 0x000502a4, r14
```

Board bit 9 **set** → skip helper, join `0x22e24` (already native,
v0324 shape 289). Board bit 9 **clear** → site-B helper + `0x7fc0`#4.

### Probe span (controlled)

```text
vf2probe --snapshot out/coli-225cc-entry.vf2snap \
  --set-ip 0x00022dd4 \
  --set-reg g7=0x00510980 --set-reg g8=0x00512980 \
  --set-reg g9=0x0100085e --set-reg g1=0x00503200 \
  --set-reg r14=0x00022e0c --set-reg r15=1 \
  --set-u32 0x00512b24=0x4000 --set-u32 0x00508000=0x8800 \
  --until 0x00022e24 --max-steps 300
```

Result: **237** steps, `bbs 9` not taken, balx `0x502a4`, digit
loop (`dmovt`/`mulo`), bx-out `0x22e20`, `call 0x7fc0`, ret to
`0x22e24`. Same helper/span as the v0344-B site-B unit (170-step
balx-to-bx) plus the proven `0x7fc0` leaf.

### Wiring

`coli_225cc_long_body` bit-14 arm (`g8+0x1a4` bit 14) no longer
returns `UNSUPPORTED` when board bit 9 is clear. It runs
`coli_502a4_body(..., 0x22e0c, ...)` (assert bx-out `0x22e20`) and
`coli_7fc0_body(..., 0x0100085e)`, then joins `0x22e24`. Board-bit-9
set keeps the measured skip. Unmeasured neighbors stay fail-closed.

### Reachability note

Cascade at `0x2292c` takes site A when board bit 9 is clear, and
site A's continuation already includes site B. The long-body
bit-14 arm is the board-**set** cascade continuation; for it to
see board clear at `0x22dd4` the board word would have to change
between the two reads. No live ROM drive currently witnesses that
composition; the gate is wired on the measured instruction span
and helper pins, not on a whole-task shape pin.

## Validation performed (this slice)

Observed, not claimed:

- CTest Debug non-dispatch: **50/50 PASS**
- CTest Debug `native_third/fourth/fifth/sixth_dispatch`: **4/4 PASS**
- CTest Debug `vf2_native_eleventh_dispatch`: PASS
- CTest Debug `vf2_native_twelfth_dispatch`: PASS (**new pin**)
- `vf2_native_runtime` / `vf2_native_runtime_state`: PASS
- `vf2cycles --rom-dir roms/vf2 --snapshot scratch-sixth.vf2snap
  --cycles 8 --input 16`: **8/8 MATCH** (296 blocks / 17,340
  instructions, both sides `0x1645c`)
- `native-nth-dispatch 12/20/40`: MATCH (pre-commit measurement)
- Player/site-B probes as above

## Open

- fa_player `0x4505` native recovery (blocked on shared-tail proof)
- Live whole-task drive for long-body site-B-only composition
- Hybrid coli non-warm whole-task shape pins
- Dispatch boundary beyond 40 / endurance re-run on this build
