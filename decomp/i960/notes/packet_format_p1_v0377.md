# packet_format_p1_v0377 — pol_test calibration + host decode hypothesis

**Status of this note:** evidence record (updated P1 pass). Host decode
improvements are **hypotheses**, not recovered C. Fail-closed until
oracle-pinned at a geometry **vertex-write** boundary (differential contract).

**Vertex-stream corroboration via i960 oracle writes: ABSENT/UNPROVEN.**
Helper `0x7c60` and task `0x21a00` write object-table records + FIFO protocol
words only. Polygon-ROM body floats (vertices/attr) are **not** observed on
geo/FIFO ports in measured probes.

---

## 1. MAME suggestions (read-only; file:line)

Vendored dump does **not** contain the polygon packet handlers. Suggestions
below are from headers, FIFO/geo plumbing, and TGP core comments only.

| Source | Suggestion |
| --- | --- |
| `third_party/mame-model2-ref/model2.h:279-301` | `geo_process_command` dispatch **names**: `geo_object_data`, `geo_direct_data`, `geo_polygon_data`, `geo_mode`, `geo_focal_distance`, `geo_matrix_write`, `geo_translate_write`, `geo_end`, `geo_lod`, … — naming only; **bodies absent** from third_party dump. |
| `model2.h:282-286` | Separate `geo_object_data` vs `geo_polygon_data` handlers → object address command is distinct from raw polygon payload commands. |
| `model2.h:303-306` | `geo_parse_np_ns` / `geo_parse_np_s` / `geo_parse_nn_ns` / `geo_parse_nn_s` draw paths — **bodies absent**. |
| `model2.h:312-313` | `model2_3d_process_polygon<NumVerts>` template — suggests vertex count is a template parameter; **bodies absent**. |
| `model2.cpp:624-632` | `copro_fifo_w`: if `coproctl & 0x80000000` then TGP program load; else push word into copro FIFO. Matches i960 write of protocol words to `0x00884000`. |
| `model2.cpp:750` | TGP data ROM `copro_data` mapped at `0x00800000`–`0x009fffff`. |
| `model2.cpp:822-896` | `geo_w` packs **function** `((address>>4)&0x3f)<<23` into the pushed word; `data & 0x80000000` is jump/tag form `r = (data & 0x800fffff) \| func<<23`. Function 1 + address bits can encode eye-mode. |
| `model2.cpp:856-880` | Geo RAM upload: control window `address < 0x1000`; start address at `0x1008`; read cursor at `0x3008`. |
| `model2.h:830-839` | `polygon_rom` + `polygon_ram0/1[0x8000]` — ROM vs two RAM banks is a real hardware split. |
| `mb86233.cpp:20-33` | TGP RAM banks at `0x000-0x0ff` / `0x200-0x3ff`; Sega programs activate **IEEE float** mode. |
| `mb86233.cpp:24-28` | FIFO at register `0x400` via bank adder; external routes for Model 2 FIFOs. |
| `third_party/README.md:39-46` | Project notes (already in-tree): function port `0x00880000` packs function in bits 23–28; FIFO `0x00884000`; bank `0x800000` → copro_data ROM. |

**Not suggested by MAME dump:** polygon-ROM float-link attr bitfields,
skip-3 words, quad bit, or `geometry_mode`. Those remain **our** hypotheses
from `src/hardware/tgp.c` + poly-ROM hex.

---

## 2. Disasm: `fa_pol_test` task `0x21a00` (MEASURED)

Source: `build/Debug/vf2i960.exe function roms/vf2 0x00021a00`.
Scheduler: `decomp/i960/tasks.csv` `fa_pol_test` entry `0x00021a00`.

### 2.1 Control bytes (Work RAM)

| Addr | Role |
| --- | --- |
| `0x00530150` | mode byte `r14`. `r14 <= 2` → path A; else path B. Path A: `r14==3` selects id `0x985` else `0x986` in the count loop. |
| `0x0053014c` | count byte. Path A: submits of `0x985`/`0x986`. Path B: submits of `0x97f`. |
| `0x0053014d` | Path B only: submits of `0x97e`. |
| `0x0053014e` | Path B only: submits of `0x97d`. |

### 2.2 Path A (`0x21a1c`, mode ≤ 2)

```text
0x21a28  lda 0x00501400, g0
0x21a30  call 0x00007f24                 ; palette helper (see 2.4)
0x21a34  FIFO immediates via st (g11)[g12]:
           0x00800101, 0x01800303, 0x03000606,
           0x3e9eb852, 0x3e428f5c, 0x3f9eb852
0x21a7c  ldob 0x0053014c, r3
0x21a84  cmpobe 0, r3, 0x21ab8           ; skip loop if count==0
loop 0x21a88:
  ldob 0x00530150, r14
  cmpobe 3, r14, 0x21aa4
  0x21a94  mov 0, g1 ; lda 0x00000986, g0 ; call 0x7c60
  0x21aa4  mov 0, g1 ; lda 0x00000985, g0 ; call 0x7c60
  cmpdeco 1, r3, r3 ; bl loop
0x21ab8  FIFO: 0x03000606, 0x00000000, 0xbec7ae14, 0x00000000
0x21ae8  mov 0, g1 ; lda 0x00000985, g0 ; call 0x7c60
0x21af4  FIFO: 0x01000202
0x21b00  ret
```

**Every pol_test call uses `g1=0`** → helper skips the `0x5010d0` accum path.

### 2.3 Path B (`0x21b04`, mode > 2)

```text
0x21b04  lda 0x00501400, g0 ; call 0x7f24
0x21b10  FIFO: 0x00800101
0x21b1c  ldob 0x0053014c → loop call 0x7c60 g0=0x0000097f g1=0
0x21b3c  ldob 0x0053014d → loop call 0x7c60 g0=0x0000097e g1=0
0x21b5c  ldob 0x0053014e → loop call 0x7c60 g0=0x0000097d g1=0
0x21b7c  FIFO: 0x01000202, 0x00800101, 0x01800303, 0x03000606,
           0x3f428f5c, 0xbf3851ec, 0x401f5c29
0x21bd0  mov 0, g1 ; lda 0x0000097d, g0 ; call 0x7c60
0x21bdc  FIFO: 0x01000202
0x21be8  ret
```

### 2.4 Helper `0x7f24` (palette-like; MEASURED disasm)

```text
0x7f24  st g12, 0x30(g10)
0x7f28  ld 0x005013f0, r7
0x7f30  lda 0x0000017f, r6
0x7f34  mov 6, r8
loop 0x7f38:
  ldis (g0), r3 ; subi r3, r6, r3
  ldis 0x2(g0), r4 ; shli 16, r4, r4
  addi r3, r4, r4 ; addi r7, r4, r4
  st r4, (g10)[g12]
  addo 4, g0, g0
  cmpdeco 1, r8, r8 ; bl loop
0x7f60  ret
```

Reads 6 packed halfword pairs from `g0` (task passes `0x00501400`), writes 6
words to `(g10)[g12]` (geo RAM). Live path-A geo RAM head matches:
`0x0000007f`, `0x01f001ff`, `0x00f8013f`×4. Semantic name (texture/color
index) is **unproven**.

### 2.5 Helper `0x7c60` body (MEASURED; recovered C exists)

```text
0x7c60  ld 0x00501018, r4 ; ld 0x0050101c, r5 ; cmpobg r5,r4,0x7d10
0x7c74  mov 0, r15 ; st r15, 0xb0(g10)          ; geo clear slot
0x7c7c  lda 0x1a003434, r15 ; st r15, (g11)[g12] ; FIFO protocol
0x7c88  ld 0x2008(g10), r15 ; lda 0x30(r15), r14 ; st r14, 0x1008(g10)
0x7c9c  st r15, (g11)[g12] ; ld (g11)[g12], r15 ; echo
0x7ca4  ld/addo/st 0x00501010                  ; submit counter
0x7cb8  lda 0x020e0004[g0*16], g0 ; ldq (g0), r8  ; r8..r11 = w0..w3
0x7cc4  shro 16, r11, r3 ; lda 0xffff, r4 ; and r4,r11,r11
0x7cd4  cmpobe 0, g1, 0x7cf0                   ; pol_test always takes this
0x7cf0  ld 0x0050101c, r4 ; addo r11, r4, r4 ; st r4, 0x0050101c
0x7d04  subo 1, 0, r11                         ; r11 = -1
0x7d08  st r8, 0x10(g10)                       ; geo 0x800010 ← w0
0x7d0c  stq r8, (g10)[g12]                     ; geo RAM ← w0,w1,w2,r11(-1)
0x7d10  ret
```

Recovered C: `src/recovered/polygon_object_submit.c`
(`vf2_recovered_polygon_object_submit`). This note does **not** change it.

---

## 3. Controlled oracle probes (MEASURED)

Park: `out/attr-long/long-29.vf2snap`.
Probe: `build/Debug/vf2probe.exe`. Tools: `tools/python/probe_pol_test_iso.py`.

### 3.1 Isolated helper `0x7c60` (ids `0x97d`..`0x986`)

Recipe (gate open; park-live `g10/g11/g12`; `g1=0`):

```text
--set-ip 0x00007c60 --until 0x00007d14
--set-reg g0=<id> --set-reg g1=0
--set-u32 0x00501018=0x1000 --set-u32 0x0050101c=0x0
--max-steps 2500 --memory-trace
```

Halt: `maximum steps` at post-helper continuation (expected; ret address is
park-stale). Helper-window writes before that halt are valid successful Model
2A accesses.

**Per-id helper-window geo sequence (all ids share this shape):**

| Port | 0x97d | 0x97e | 0x97f | 0x980..0x984 | 0x985 | 0x986 |
| --- | --- | --- | --- | --- | --- | --- |
| `0x00800010` | `0x004a7606` | `0x004a7616` | `0x004a766e` | table w0 | `0x004a7d26` | `0x004a7d36` |
| `0x00804000` | `0x004a7606` | `0x004a7616` | … | table w0 | `0x004a7d26` | `0x004a7d36` |
| `0x00804004` | `0x00157db4` | `0x00157db8` | … | table w1 | `0x00158134` | `0x00158138` |
| `0x00804008` | `0x0098a0b1` | `0x0098a0c6` | … | table w2 | `0x0098a9c9` | `0x0098a9de` |
| `0x0080400c` | `0xffffffff` | `0xffffffff` | `0xffffffff` | `0xffffffff` | `0xffffffff` | `0xffffffff` |

**FIFO first write (every isolated helper):** `0x00884000 ← 0x1a003434`.
Then `0x0` (pointer echo slot on this park path). Park continuation may emit
color-like words (`0x0d001a1a`, `0x12002424`, …) **after** the helper window —
not part of the per-submit protocol core.

Table reads confirm `0x020e0004[id*16]` matches the geo values exactly.

### 3.2 Clean task path-A probes (gold protocol)

`tools/python/probe_pol_test_iso.py`:

| Probe | Mutations | Halt | Helper ids observed |
| --- | --- | --- | --- |
| `task_pathA_986` | `0x530150=2`, `0x53014c=1` | `stop address ip=0x21b00` run=162 | first submit **0x986**, final **0x985** |
| `task_pathA_985` | `0x530150=3`, `0x53014c=1` | `stop address ip=0x21b00` run=161 | both submits **0x985** |

**Measured FIFO sequence (path A, both probes):**

```text
0x00800101
0x01800303
0x03000606
0x3e9eb852          ; f32 ≈ 0.3100
0x3e428f5c          ; f32 ≈ 0.1900
0x3f9eb852          ; f32 ≈ 1.2400
--- helper 0x7c60 (submit) ---
0x1a003434
0x00000000
--- helper 0x7c60 (final 0x985) after interstitial ---
0x03000606
0x00000000
0xbec7ae14          ; f32 ≈ -0.3860
0x00000000
0x1a003434
0x00000000
0x01000202
```

**Measured geo RAM `0x00804000` head (path A):** `0x0000007f`, `0x01f001ff`,
`0x00f8013f`×4 from `0x7f24`, then table words for each submit.

Path-B probes ran past ret into other corridors (`maximum steps`); early
window still shows the same path-A prelude because `0x530150` was mutated
after restore but park-era bytes may win on some runs — treat path A stop-
address probes as the clean protocol gold. Path B immediates remain available
from **disasm** (section 2.3), which is measured ROM.

### 3.3 Poly ROM vs oracle geo/FIFO (CRITICAL)

For every pol_test id `0x97d..0x986`:

```text
geo/FIFO writes contain:  table w0, w1, w2, r11=-1, protocol words, palette pack
geo/FIFO writes do NOT contain: poly-ROM vertex floats, attr words, skip3 slots
verdict: table-record + protocol only  →  vertex stream ABSENT on i960 bus
```

Example `0x97d` poly-ROM head (MEASURED ROM dump, **not** an oracle write):

```text
[0..5]  p0=(-0.2,0,0.2) p1=(-0.2,0,-0.2)
[6]     attr=0xe1001601
[7..9]  (0,-1,0)
[10..12] p2=(0.2,0,0.2)
[13..15] p3=(0.2,0,-0.2)
[16]    attr=0x00000000 → end
[17..20] 0x00040401,0,0,0 pad
```

None of `0xbe4ccccd`, `0xe1001601`, `0xbf800000` appear as helper geo/FIFO
writes. **TGP/geo processor is expected to consume polygon ROM via w2 address;
that consumer is outside the i960 oracle.**

---

## 4. Object table + w3 family (MEASURED)

Live table (park long-29 + ROM `out/main_data.bin`):

| id | w0 | w1 | w2 | w3 | poly word | span to next |
| ---: | --- | --- | --- | --- | --- | ---: |
| 0x97d | `0x004a7606` | `0x00157db4` | `0x0098a0b1` | `0x00010002` | `0x18a0b1` | 21 |
| 0x97e | `0x004a7616` | `0x00157db8` | `0x0098a0c6` | `0x000a000b` | `0x18a0c6` | 111 |
| 0x97f | `0x004a766e` | `0x00157de0` | `0x0098a135` | `0x00640065` | `0x18a135` | 1011 |
| 0x980 | `0x004a7986` | `0x00157f70` | `0x0098a528` | `0x00010002` | `0x18a528` | 21 |
| 0x981 | `0x004a7996` | `0x00157f74` | `0x0098a53d` | `0x00010002` | `0x18a53d` | 21 |
| 0x982 | `0x004a79a6` | `0x00157f78` | `0x0098a552` | `0x00010002` | `0x18a552` | 21 |
| 0x983 | `0x004a79b6` | `0x00157f7c` | `0x0098a567` | `0x000a000b` | `0x18a567` | 111 |
| 0x984 | `0x004a7a0e` | `0x00157fa4` | `0x0098a5d6` | `0x00640065` | `0x18a5d6` | 1011 |
| 0x985 | `0x004a7d26` | `0x00158134` | `0x0098a9c9` | `0x00010002` | `0x18a9c9` | 21 |
| 0x986 | `0x004a7d36` | `0x00158138` | `0x0098a9de` | `0x00010002` | `0x18a9de` | (no next pol_test) |

**w3 family (pol_test only):** `w3 = ((n-1)<<16) | n` for
`n ∈ {2, 11, 101, …}`. Span deltas `21 / 111 / 1011` align with
`6 header + body + 1 end + pad` under skip3 quads/tris.

**Attract w3 is NOT this family:** e.g. `0x148 w3=0x014a0164`,
`0x14f w3=0x00700097`, `0x08f w3=0x007600bb`. Semantics **unproven**.

---

## 5. Hypothesis table (bitfield → meaning)

Full machine-readable table: `out/attr-p1/hypothesis_table.json`.

| Bitfield / word | Meaning | Tag |
| --- | --- | --- |
| `w2 & 0x7fffff` | poly-ROM word_index | **measured** |
| `w2 & 0x00800000` | polygon_rom bank | **measured + tgp.c** |
| table `w0` | geo word0; stored to `0x800010` and `0x804000` | **measured** |
| table `w1` | geo RAM `0x804004` | **measured** |
| table `w2` | poly address word `0x00800000\|word_index` | **measured** |
| table `w3` pol_test | `((n-1)<<16)\|n` count pair | **measured_family** |
| table `w3` attract | not `(n-1,n)` | **unproven** |
| helper `r11` after `ldq` | `w3`; lo used for gate budget; `subo 1,0,r11` → `-1` at `stq` | **measured** |
| poly attr `(attr&3)==0` | end marker | **hypothesis from tgp.c** |
| poly attr `attr&1` | quad selector | **hypothesis from tgp.c + hex** |
| poly `(attr>>8)&3` | p0/p1 link update mode | **hypothesis from tgp.c** |
| poly attr high bits | texture/color/flags (`0xe1001601`) | **unproven** |
| 3 words after attr | skip3 slot when `geometry_mode&3<2`; looks like normals on 0x97d | **hypothesis from tgp.c + hex** |
| FIFO `0x00800101` | protocol; class bits `0x01` align with tgp.c object-data case **name** | **measured_word + suggested_by_mame_class_name** |
| FIFO `0x01000202` | protocol; class `0x02` aligns with tgp.c direct-data case **name** | **measured_word + suggested_by_mame_class_name** |
| FIFO `0x1a003434` | helper submit protocol; class unproven | **measured_word + unproven_semantics** |
| FIFO `0x01800303` / `0x03000606` | protocol family; bodies unproven | **measured_word + unproven_semantics** |
| FIFO floats `0x3e9eb852…` / `0x3f428f5c…` | task immediates; not poly verts | **measured_words + unproven_semantics** |
| geo RAM palette pack `0x7f/0x1f001ff/0xf8013f` | from helper `0x7f24` | **measured_words + unproven_semantics** |
| MAME `geo_w` bits 23–28 / bit31 | host geo control packing | **suggested_by_mame** |
| MAME `geo_polygon_data` bodies | polygon command bitfields | **absent_in_vendored_mame** |
| `geometry_mode&3<2` → skip 3 | float-link payload slot count | **existing_boundary_model (tgp.c)** — not upgraded here |

---

## 6. Decode scoring vs oracle (MATCH / MISMATCH)

Tool: `tools/python/analyze_pol_test_oracle.py`
Output: `out/attr-p1/oracle_decode_score.json`

### 6.1 What the oracle can and cannot pin

| Observable | Pins vertex count? | Notes |
| --- | --- | --- |
| helper geo/FIFO writes | **No** | table + protocol only |
| poly-ROM span to next id | **Partial** | includes pad after end marker |
| pol_test `w3` lo | **Partial** | family hint; attract w3 differs |
| poly-ROM hex on 0x97d | **Yes for tiny prim shape** | ±0.2 XZ quad under skip3 + attr&1 |
| MAME handler bodies | **No** | absent |

### 6.2 Decoder variants (scored)

| Rank | Mode | Oracle-corr. score | 0x97d attrs | 0x97d links | 0x97d words vs span=21 | Verdict |
| ---: | --- | ---: | --- | ---: | --- | --- |
| 1 | **`skip3_float_link`** | **0.684** | `[0xe1001601]` | **1** | 17 + 4 pad → span MATCH | **chosen host hypothesis** |
| 2 | `skip3_end_attr0` / `skip3_fixed_link` | 0.684 | same walk on 0x97d | 1 | same | tied on pol_test tiny; less justified vs tgp.c |
| 6 | `skip3_quad_bit2` | 0.274 | `[0xe1001601, 0x3e4ccccd]` | 2 | false span via float-as-attr | **MISMATCH** |
| 7 | `noskip_float_link` | 0.273 | float-as-attr | 2 | false span | **MISMATCH** |

**Critical scoring note:** modes that match span by treating IEEE floats
(`0x3e4ccccd` = 0.2) as attr words are **false positives**. Hex gold for
0x97d is a **single** attr `0xe1001601`, one quad link, 2 tris, ±0.2 XZ.

### 6.3 Chosen decode

```text
chosen_mode = skip3_float_link
corroboration = poly-ROM hex + tgp.c geometry_execute_object path + w3 family span
oracle vertex-write pin = ABSENT/UNPROVEN
promotion to recovered C = NOT ALLOWED by this note
```

On pol_test family, skip3_float_link:

| id | w3 lo | decode links | tris | end | note |
| --- | ---: | ---: | ---: | --- | --- |
| 0x97d | 2 | 1 | 2 | end_marker | hex gold quad |
| 0x97e | 11 | 10 | 20 | end_marker | near w3 lo; slight walk over-attr risk remains |
| 0x97f | 101 | ~101 family | (see json) | end_marker | large prim |
| 0x985/0x986 | 2 | 1 | 2 | end_marker | same shape family as 0x97d |

---

## 7. Calibration PNGs (host hypothesis)

Rendered via `tools/python/render_p1.py` → `render_mesh_host.py --decode skip3_float_link`:

```text
out/attr-render/v0377/p1/
  id_97d_skip3_{fit-ortho,front,top,side,iso-*}_512.png
  id_985_skip3_*.png
  id_148_skip3_*.png
  id_14f_skip3_*.png
  id_08f_skip3_*.png
  render_p1_manifest.json
  render_report.json
```

Counts (skip3_float_link): `0x97d`/`0x985` = 2 tris; `0x148` = 10 tris;
`0x14f` = 49 tris; `0x08f` = 257 tris.

These are **oracle-corroborated offsets + host hypothesis meshes** — not
game-accurate camera output and not a named logo/mesh.

---

## 8. Fail-closed boundary

```text
MAME names/handlers          → SUGGESTIONS only (bodies not in dump)
oracle 0x7c60 / 0x21a00      → MEASURED (table + FIFO + palette pack)
poly ROM hex at word_index   → MEASURED (ROM bytes)
host decode ranking          → HYPOTHESIS (tools/python + PNG)
src/hardware/tgp.c           → existing boundary model; NOT upgraded here
recovered C vertex stream    → NOT claimed; oracle pin ABSENT
```

Do **not** copy MAME handlers into `src/recovered/`. Do **not** rename
meshes to logo/SEGA. Do **not** treat PNG geometry as recovered semantics.

To promote a decode mode to recovered C:

1. pin a geometry-boundary snapshot where TGP/host must emit the **same
   vertex/tri stream** for a pol_test id (not merely table words);
2. run reference executor vs recovered path differentially at that boundary;
3. only then change `src/hardware/tgp.c` / recovered callers.

---

## 9. Residual unknowns (explicit)

- MAME `geo_polygon_data` / `geo_object_data` / `geo_parse_*` **bodies** not
  vendored — no polygon command bitfield quote beyond names.
- Attr high bits (`0xe1001601`, `0xd0a81502`, `0x01281601`, …) texture /
  color / flag split is **unproven**.
- skip3 slot semantics (normal vs unused vs light) — hex looks normal-like
  on 0x97d `(0,-1,0)` but **unproven** generally.
- Attract `w3` (`0x148=0x014a0164`) bounds/size meaning is **open**.
- FIFO protocol class bodies (`0x03`, `0x06`, `0x14`, color-like family) are
  hex-only.
- Helper `0x7f24` palette pack semantics **unproven**.
- Isolated helper probes halt at park continuation (`maximum steps`), not a
  clean ret-to-`0x7d14`; writes before halt remain valid Model 2A accesses.
- Path-B task probes leaked past ret into other corridors; use disasm +
  path-A gold for protocol; do not treat late path-B writes as pol_test-only.
- `skip3_float_link` on dense attract ids remains a **host** mesh hypothesis.
- True TGP-side vertex emission is outside the i960 oracle — needs a TGP/geo
  boundary observation (currently ABSENT).

---

## 10. Tools / evidence touched this pass

**Code (parent integrates; no git commit here):**

- `tools/python/analyze_pol_test_oracle.py` (new)
- `tools/python/probe_pol_test_iso.py` (new)
- `tools/python/decode_packet_hypotheses.py` (fail-closed note)
- `tools/python/render_p1.py` (output `out/attr-render/v0377/p1/`)
- `decomp/i960/notes/packet_format_p1_v0377.md` (this file)

**Evidence (not for git):**

- `out/attr-p1/pol_test_iso_*.jsonl`, `pol_test_task_pathA_*.jsonl`
- `out/attr-p1/oracle_decode_score.json`
- `out/attr-p1/hypothesis_table.json`
- `out/attr-p1/iso_probe_run.json`
- `out/attr-render/v0377/p1/*.png`

No `src/recovered/` change. No git commit (parent integrates).
No invented mesh names. No ROM/snaps/traces in git.
