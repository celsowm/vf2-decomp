# v0382 — coli `0x22404` live first-contact stale slot (body 77)

**Status:** recovered C + ROM-backed differential green; committed.
Reopens `fa_coli_22404_body77_v0358.md` (deferred for COBR-CC
calibration, now settled by v0380/v0381).

## 1. Witness

First `0x22404` call of the `coli-live-midbody-g01` trace
(`out/coli-live-midbody-g01.jsonl`, trace idx 29..106): **78 steps**
`0x22404 → ret 0x225b0` (body 77 + ret), `g0 = 1`. Reproduce the entry
from the midbody park (`out/coli-midbody-22210.vf2snap` + the v0381
recipe) with `--until 0x22404` (29 steps), then `--until 0x2222c`
(78 steps).

Live entry shape: slot 0, old snap (`g13+0x8c`) `0xffff` (stale) vs
snap (`g7+0x1a8`) 0, flags (`g7+0x1a4`) `0x100` (bit 8), pending
(`g13+0x90`) 0, thresholds (`g7+0x1aa` / `g7+0x808`) 0/0,
`field_5b8` bit 0 clear, index (`g7+0x820`) 1, table (`g13+0x4c`)
`0x221e8`, exclude (`g8+0x6dc`) 0, delta (`g8+0x26`) 0, coords 0.
ROM/main_data reads (served by real images): `0x000232e3 = 0x00`
(`fifo_sel`, so triple from `0x0090fb80` buffer-RAM stub),
`0x02007ace = 0x00000008` (mask bit 3 → one scan hit).

## 2. Delta vs the covered equal-snapshot shape (v0303 body 72)

Disassembly: `cmpobe r3, r4, 0x22434` at `0x2242c` (equal → `0x22434`,
else fall to `bal 0x225bc`). The stale slot falls through: `bal` +
pending-clear helper (`ldos/clrbit/stos/bx`, 4 insns) rejoin at
`0x22434`. Cost +5 vs the equal path (22 → 27 prologue); every later
section is identical (verified insn by insn: loop 8, `andnot` 4,
pending-set 4, `fifo_sel` 3, `ldt` 2, prefix 4, `cmpibe` 2, header 2,
coords 9, triple 3+3, FIFO header 2, `stt` 1, `g9` 1 = 50 both).
All other C gates hold unchanged on the live shape (pending clear,
thresholds, `5b8` bit 0, slot 0).

Fail-closed siblings: stale+empty (unmeasured — the live stale shape
is non-empty) and stale slot 1 (unmeasured — live stale is slot 0).
The `0x223bc` helper bit-0-set chain (`lda 0x139/0x93/...` dispatch
on snap) stays closed via the existing `field_5b8 & 1` gate.

## 3. Oracle fix: `bo`/`bno` after scanbit (`src/i960/executor_arch.c`)

The v0382 fixture initially spun in the scan loop on the reference
side (`bno` at `0x22468` never taken with `cc = NONE`, `r5 = 31`).
Root cause: the e0b9cf3 wrapper unconditionally re-decides `bo`/`bno`
from stale `AC & 7`. That override is correct after integer compares
(`cc` LESS/EQUAL/GREATER with AC in lockstep: integer results are
always ordered), but `scanbit`/`spanbit` never touch AC — legacy
already decides exactly (`bno` taken only on a miss, `cc OVERFLOW`
on hit / `NONE` on miss). The park only worked by AC luck.

Fix: apply the AC-based override only when `compare_result` is
LESS/EQUAL/GREATER (arch-managed integer-compare domain); leave the
`OVERFLOW`/`NONE` scanbit domain to legacy. Full suite 66/66 with no
pin changes (no existing test depended on the stale-AC behavior);
ASan/UBSan green on the touched suites plus phase17 202/202.

## 4. Native changes (`src/recovered/hybrid.c`, `coli_22404_body`)

- Prologue 22 (equal) / 27 (stale, `+5 bal+helper rejoin`).
- `stale && slot != 0` → `VF2_ERROR_UNSUPPORTED` (unmeasured).
- `result == 0 && stale` → `VF2_ERROR_UNSUPPORTED` (unmeasured).
- Final `CC = EQUAL` + AC lockstep on the proven stale-nonempty
  path only (last compare `cmpibe` on `delta == 0`, taken);
  equal-path CC stays unpinned until its own fixture. The pending
  slot-clear write on the stale path is value-preserving under the
  entry gate (slot bit already clear), so both paths rejoin exactly.

## 5. Tests

- `test_native_runtime.c`: live-77 unit (78 total, `g0 = 1`,
  pending bit set, result `0x21e8`) + stale+empty negative
  (`VF2_ERROR_UNSUPPORTED`); restores `0x820`/table leftovers so the
  133/79 siblings keep their pins. Equal-path counts (13/30/73/134)
  unchanged — verified, not just assumed.
- New ROM-backed `vf2_coli_22404_live` (+ `_differential`): seed =
  exact initial-read set of the reference body trace, `enter_procedure`
  convention both sides, 78-step lockstep + full live-state equality.
- Full `ctest` 66/66; Clang ASan/UBSan green on
  `vf2_coli_22404_live`, `vf2_native_runtime` (both modes),
  `vf2_coli_225cc_live`, `vf2_phase17_zero` (202/202).

## 6. Next (explicitly deferred)

Whole-tail composition (v0358 step 4): the midbody v0304 branch
(first-hit/second-warm) routes the live combination through the
resolver to the LONG body (bit 3 clear + scan 0), but hardcodes the
compact 5/5 call/ret counts and passes NULL `cc`/`g1`. Measured span:
380 steps (371 post-v0381/v0382), 12 call-insns, 10 rets, single
`0x225cc` long from `0x22290` (trace idx 129) returning `0x22294 →
0x10dcc`. Needs: long-aware counts + `cc_out`/`g1_out` threading in
that branch, a `0x22210 → 0x10dcc` tail fixture, and the task-gate
triple (still fail-closed at 9214/9528/9393).
