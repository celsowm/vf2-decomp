# pol_test_path_a_v0378 — recovered C for fa_pol_test path A + helper 0x7f24

**Status:** recovered C + unit/ROM-backed pins (parent integrates; no git commit here).

Evidence base:
- `decomp/i960/notes/packet_format_p1_v0377.md`
- `out/attr-p1/pol_test_task_pathA_986.jsonl` / `_985.jsonl`
- ROM disasm via `build/Debug/vf2i960.exe function roms/vf2 0x00021a00` and `0x00007f24`

---

## 1. Gate correction (measured)

Disasm:

```text
0x21a00  ldob     0x00530150, r14
0x21a08  cmpoble  2, r14, 0x00021a1c   ; path A
0x21a0c  ... b 0x00021b04              ; path B
```

i960 `cmpoble src1, src2` branches when `src1 <= src2`
(same operand order as measured `cmpobg r5, r4` in helper 0x7c60 →
`r5 > r4`). Therefore:

```text
path A when mode >= 2
path B when mode <  2
```

Probe confirmation:
- `task_pathA_986` mode=2 count=1 → stop `0x21b00`, run=**162**, first submit 0x986
- `task_pathA_985` mode=3 count=1 → stop `0x21b00`, run=**161**, both submits 0x985

The 162→161 delta is exactly the path-A `b 0x21ab0` skipped when the
loop takes the mode==3 arm. Mode=3 **is** path A. The v0377 note phrase
"mode ≤ 2 → path A" is a misread of `cmpoble 2, r14` and is corrected here.

Recovered C fail-closed:
- `mode >= 2` → path A body
- `mode < 2` → `VF2_ERROR_UNSUPPORTED` (path B not recovered)

Loop id (measured): `mode==3` → object `0x985`, else `0x986`. Final
submit is always `0x985`. Every pol_test call uses `g1=0`.

---

## 2. Helper 0x7f24 (body recovered)

```text
0x7f24  st  g12, 0x30(g10)
0x7f28  ld  0x005013f0, r7
0x7f30  lda 0x0000017f, r6
0x7f34  mov 6, r8
loop:
  ldis (g0), r3        ; sext16
  subi r3, r6, r3      ; r3 = 0x17f - lo
  ldis 2(g0), r4       ; sext16
  shli 16, r4, r4
  addi r3, r4, r4
  addi r7, r4, r4
  st   r4, (g10)[g12]  ; same geo port each iteration
  addo 4, g0, g0
  cmpdeco 1, r8, r8
  bl loop
0x7f60  ret
```

Pinned source (long-29 park, bytes LE):

```text
0x5013f0 = 0x00000080
0x501400 halfwords:
  0x0180, 0x0000
  0x0000, 0x01f0
  0x00c0, 0x00f8   (x4)
```

Measured geo-port writes (`g10=0x00800000`, `g12=0x4000` → `0x00804000`
on the park; unit tests use `g12=0` → `0x00800000`):

```text
0x0000007f
0x01f001ff
0x00f8013f
0x00f8013f
0x00f8013f
0x00f8013f
```

All six words hit the **same** address `(g10)[g12]`. Semantics of the
pack remain **unproven** (no texture/color name claimed).

C: `vf2_recovered_polygon_palette_pack_7f24`.

---

## 3. Path A FIFO gold (count=1, geo pointer 0)

Oracle memory-trace + recovered C (both sides pin these 15 words):

```text
0x00800101
0x01800303
0x03000606
0x3e9eb852
0x3e428f5c
0x3f9eb852
0x1a003434          ; helper 0x7c60 submit (loop id)
0x00000000          ; helper pointer echo *(g10+0x2008)
0x03000606          ; task interstitial
0x00000000
0xbec7ae14
0x00000000
0x1a003434          ; helper 0x7c60 final 0x985
0x00000000
0x01000202
```

count=0 gold (loop skipped):

```text
0x00800101 0x01800303 0x03000606 0x3e9eb852 0x3e428f5c 0x3f9eb852
0x03000606 0x00000000 0xbec7ae14 0x00000000
0x1a003434 0x00000000
0x01000202
```

Helper 0x7c60 is **reused** via `vf2_recovered_polygon_object_submit`
(not copied). Object table pins unchanged:
`0x986 → w0 0x004a7d36`, `0x985 → w0 0x004a7d26` (ROM main_data).

---

## 4. Instruction pins (recovered == reference)

| Case | Native recovered C | Reference i960 to `return_address` |
| --- | ---: | ---: |
| mode=2 count=1 | 163 | 163 |
| mode=3 count=1 | 162 | 162 |
| mode=2 count=0 | 127 | 127 |

Budget for mode=2 count=1: gate 2 + cont 2 + call/palette (2+65) +
prelude 12 + count 2 + loop (2+1+3+28+2) + interstitial 8 +
final (3+28) + close 2 + ret 1 = **163**.

---

## 5. Fail-closed leftovers

- Path B (mode 0/1) body ids `0x97f/0x97e/0x97d` and path-B FIFO
  immediates (`0x3f428f5c`, `0xbf3851ec`, `0x401f5c29`, …) remain
  **unsupported** (disasm-only gold).
- Path C / logo / named mesh: still unwitnessed; not claimed.
- Palette pack **semantic** meaning: unproven words only.
- Polygon-ROM vertex stream: still outside the i960 oracle (v0377).
- Native-runtime scheduler dispatch of `fa_pol_test` is **not** wired in
  this slice; the recovered function is a standalone boundary body, same
  pattern as `vf2_recovered_polygon_object_submit`.
- Isolated 0x7c60 probes still halt on park continuation (`max steps`);
  not required for this path-A pin.

---

## 6. Code / tests (parent integrates)

- `src/recovered/pol_test_path_a.c`
- `include/vf2/recovered.h` (declarations)
- `tests/recovered/test_pol_test_path_a.c`
- `CMakeLists.txt` (`vf2_pol_test_path_a` + `_differential`)

CTest observed (MSVC Debug):

```text
vf2_pol_test_path_a ...................... Passed
vf2_pol_test_path_a_differential ......... Passed
vf2_polygon_object_submit ................ Passed
vf2_polygon_object_submit_differential ... Passed
vf2_tests ................................ Passed
vf2_orchestrator_limits/scan/gates/bridge  Passed
vf2_object_handlers_differential ......... Passed
```

No git commit. No ROM/snap/trace content in the tree from this slice.
