# v0381 — coli `0x225cc` live-midbody shape (240/4/5 + CC + g1)

**Status:** recovered C + ROM-backed differential green; committed.

## 1. Witness (current build)

Live midbody park (`out/coli-midbody-22210.vf2snap`, in-tree, gitignored):

```text
vf2probe --snapshot out/coli-midbody-22210.vf2snap \
  --set-ip 0x22210 \
  --set-reg g7=0x510980 --set-reg g8=0x512980 \
  --set-reg g13=0x514940 \
  --set-u32 0x510b24=0x100 \
  --set-u8  0x5111a0=0x01 \
  --set-u32 0x5149cc=0xffff \
  --until 0x225cc   -> 130 steps to 0x225cc
  --until 0x10dcc   -> 371 steps, 0x225cc x1
```

Minimized: `--set-ip`/g7/g8/`0x5149cc` droppable (park carries them);
essential are `0x510b24` bit 8 and `0x5111a0` byte. Live entry shape:
`g8+0x1a4 = 0`, `g7+0x1a4 = 0x100`, `g7+0x821 = 0`, `g8+0x19f = 0`
(no `0x18bd4` shortcut), `g7+0x822 = 0`, `g8+0x5b8 = 0`,
`g7+0x844 = 0`. Body: **240 steps** `0x225cc → 0x22294`, 4 nested
calls (`0x230d4` ×1, `0x23238` ×2, `0x1ab34` ×1).

## 2. Fixture (`tests/recovered/test_coli_225cc_live.c`)

ROM-backed differential (real maincpu + main_data): work RAM seeded
with the exact 100-byte initial-read set of the reference body memory
trace (every address read before written; rest zero), registers from
the live park, fresh procedure frame (same construction both sides,
so any frame approximation fails closed rather than silently).
Compares instruction counts plus full live state
(`vf2_i960_compare_live_state`: registers, CC/AC, frames, memory).

## 3. Fixes (all oracle-measured on this shape)

- **Half gate relaxed** (`hybrid.c`, long body): `g8+0x6d4 == 0xffff`
  was over-narrow; the half feeds only the `& 0xd9b0` mask (r5 dies
  at the `0x22b6c` join, verified through the join on this path).
  Live half `0x21e8` (mask `0x1b0`) flows through; `mask == 0`
  stays fail-closed.
- **Scanbit `be`-taken skip** (long body): with `r11 == 0`,
  `cmpibl` publishes EQUAL in both executors, so `be @ 0x22624` is
  always taken and the `0x22628` pack is unreachable
  (single-predecessor CFG). The pre-v0359 pack model counted 9
  phantom instructions and produced garbage `r9`; replaced with
  skip (`body += 3`, `r9 = 0`). Live trace executes
  `0x2261c,0x22620,0x22624,0x2265c`.
- **Float-tail `r9 == 0` admitted** (long body): measured live shape
  (`acc = 24.0`); ROM executes `divr` unconditionally and
  `0.0/24.0 == 0.0` under the host float semantics used by every
  other `mulr/divr` here. The differential compares bit-exact, so a
  future `0.0/0.0` shape stays fail-closed by measurement, not by
  the gate.
- **Final CC pinned** (long body tail): last compare is
  `cmpibge 0,r14 @ 0x23078` (`*(0x500028)` signed); replicated via a
  new nullable `cc_out` threaded through `coli_225cc_long_body` →
  `coli_225cc_body` → `vf2_hybrid_coli_225cc_execute` (applied with
  AC lockstep; midbody callers pass NULL, behavior unchanged).
- **`g1` poststate**: `mov 17,g1` precedes the `0x1ab34` call and the
  callee preserves it (live exit `g1 = 0x11`); threaded via nullable
  `g1_out` (sentinel `0xffffffff` = untouched).

## 4. Synthetic-pin updates (`test_native_runtime.c`)

The removed pack arm fires on every zero-RAM synthetic shape, so
their native-self-consistency pins move by exactly −9 with FIFO
floats recomputed from the corrected `r9 = 0.0` chain (all zeros on
those shapes — verified shape by shape, same code path as the live
proof): 249→240, 263→254, 251→242 (×2), 256→247, 149, 87, 275,
271, 224, 883→874, 884→875, stored `0xc8a3d709/0xc85a740c/0xc8da740c`
→ `0x00000000`. These remain regression pins (synthetic ROM, no
oracle); the ROM-backed proof for the model is the live fixture.

## 5. Validation

- `vf2_coli_225cc_live` + `_differential`: pass.
- Full `ctest`: 64/64 pass.
- No ROM/snap/trace content committed; scratch traces deleted.
