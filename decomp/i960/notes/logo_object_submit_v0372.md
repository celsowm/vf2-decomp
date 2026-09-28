# v0372 — Alternative selectors + object-submit protocol (logo fail-closed)

## Status

**Named 3D logo / title mesh: NOT witnessed.** Fail-closed retained.

What *was* measured is the polygon **object-submit protocol** on the
attract path, plus a negative sweep of alternative frame selectors.
There is still **no ROM table, ASCII, or recovered-C evidence** that
any submitted object ID is a title/logo mesh.

## 1. Object-submit helper `0x00007c60`

### Protocol (oracle + ROM)

```text
gate: if *(u32*)0x50101c > *(u32*)0x501018 → ret
g0 = object_id
addr = 0x020e0004[g0*16]          ; main_data object table
record = ldq *(addr)              ; 16 bytes, 4 words
*(g10+0x10) = record.w0
stq record → FIFO via (g11)[g12]
g0 discarded as -1 after submit
```

Table record layout (measured from `out/maincpu` + main_data LOAD32_WORD):

| word | meaning (evidence) |
| --- | --- |
| w0 | value written to geo `0x800010` / FIFO (object word0) |
| w1 | secondary id/size-like field |
| w2 | if bit `0x00800000` set → `polygons.bin` offset `w2 & 0x7FFFFF` |
| w3 | size/pair-like (e.g. id `0x148` has `0x014a0164`) |

### Oracle pin (`vf2probe --set-ip 0x7c60`, park `long-29`)

Gate open: `0x501018=0x1000`, `0x50101c=0`.

| g0 | table reads | geo `0x800010` first write |
| ---: | --- | --- |
| `0x148` | `0x020e1484..90` | **`0x000b026a`** |
| `0x88` | `0x020e0884..90` | **`0x0008e6de`** |
| `0x985` | `0x020e5094..` (g0 may not stick across steps) | `0x004a7d26` |

`0xb026a` / `0x8e6de` match object-table **w0** for ids `0x148` / `0x88`.
This pins the parent-reported correlation.

### Who calls `0x7c60` (maincpu, dest=`ip+signed24`)

i960 `call` encoding measured: `dest = ip + signed24(disp)` (not ip+4).
**~130 sites**, clustered:

| cluster | n | notes |
| --- | ---: | --- |
| `0x190f0–0x19eb4` | 10 | early display/game_disp family |
| `0x20084–0x21bd8` | **54** | includes **`fa_pol_test` 0x21a00** calls `0x21a9c/0x21aac/0x21af0/0x21b30/0x21b50/0x21b70` with **test prim ids** `0x97d..0x986` |
| `0x2e4e4–0x31778` | **49** | display runtime, includes `display_command_emit` `0x310b8` / `0x311a4` |
| `0x337ec–0x33c0c`, `0x3b428–0x3b5ac`, `0x46d80+`, `0x56fa4+`, `0x634a8–0x63ed0`, `0x64794` | 1–8 each | other submitters |

No `call 0x21a00` and no `call 0xd380` in maincpu — both are
**task/scheduler entries** (`tasks.csv`: `fa_pol_test` state `0x500874`;
frame dispatch `0xa6f8[sel]`).

`display_command_emit` (`0x31040`, symbols.csv rom-verified) calls `0x7c60`
with `g0` from halfword table **`0x70cbc`**:
`ldos 0x70cbc / 0x70cbc[r9*2] → r10; lda (r10), g0; call 0x7c60`.
Halfwords observed: `0x0ee1,0x0eed,0x13fc,0x13fe,0x1400,0x1401,…`
These are **not** the attract ids `0x88/0x148`.

**Selector 9 link:** frame worker `0xd380` does `call 0x313d8` at `0xd38c`.
ROM `0x313d8` only tests `0x500700 & 0x00171730` and may `setbit 1` on
display-object flags `*(u32*)0x50084c + 0x40`, then ret. Nearby `0x31408`
dispatches on mode `0x500064` and can `call 0x31040` when mode==5.
So sel9 can **arm display emit**, which then submits objects — still
**without a name**.

## 2. Attract phase5 object IDs (measured)

Trace `out/attr-long/fifo-phase5.jsonl` + `tools/python/correlate_object_table.py`:

- table reads `0x020e1484` (id `0x148`), `0x020e0884` (id `0x88`), `0x020e1454` (id `0x145`), …
- geo/FIFO writes of table **w0**: `0xb026a`→`0x148`, `0x8e6de`→`0x88`, `0xaffb2`→`0x145`, plus `0x140/144/149/14f/150/157` family
- FIFO also carries **color/transform immediates** that appear in maincpu
  (`0x14802929`, `0x1c803939`, `0x03000606`, `0x09801313`, floats `0x3e…/0x3f…`).
  `0x14802929` is an **immediate at `0x16fa8/0x176f0/…`**, **not** object id `0x148`.
  hybrid.c / tests already treat `0x14802929` as a FIFO protocol constant.

Object-table / polygons.bin facts (`tools/python/dump_poly_objects.py`):

- id `0x148` → poly_off `0x40430`, w3 `0x014a0164`
- id `0x88` → poly_off `0x13aec`
- ids `0x97d–0x986` → poly_off `0x18a0b1+`, tiny prims (2 tris in software raster)

## 3. Name evidence search (negative)

| source | SEGA/LOGO/TITLE/VIRTUA ASCII |
| --- | --- |
| maincpu | only legal warning `SEGA ENTERPRISES,LTD.` @ `0xaaad` (v0360, **not** 3D) |
| main_data (`out/main_data.bin`) | **none**. Hits are `VF2`, `CGT`, fighter names `DRAGON_SU_*`, `R_RUF_*` |
| polygons.bin | **none** |
| recovered C / notes / symbols.csv | **no** object-id → title/logo name mapping |
| tile planes this campaign | spaces, `0x89xx` SEGA-warning glyphs (sel0 draw), `0x88xx` sequential font bank (sel15), residual `t4e jef` at status-tail dest `0x010000e2` |

`t4e jef` = `0x8074 0x8034 0x8065 0x806a 0x8065 0x8066` at `0x010000e2`.
Same destination as status-tail common blit. **Residual/garbage tiles,
not a title string.**

## 4. Software raster / mesh dump platform

Yes, host-side only:

- `src/hardware/tgp.c` `geometry_execute_object` — float-link decode,
  command class `0x01` takes `command[3]` as object_address with bit
  `0x00800000` = polygons ROM, base `& 0x7FFFFF`.
- `tools/python/render_poly_objects.py` dumps PPM triangles from those
  ROM offsets using the same decode (analysis, not a recovery claim).

Measured this campaign (`out/attr-logo/obj_*.ppm`):

| id | tris | bounds (approx) |
| ---: | ---: | --- |
| `0x97d`/`0x985` | 2 | unit quads (~0.2) — test prims |
| `0x88` | 4 | ~1 |
| `0x148` | 10 | ~0.5 |
| `0x14f`/`0x150` | 49 each | mirrored pairs, ~0.07 extent |
| `0x140` | 19 | y bound glitch `-5.9e5` (decode mode-dependent) |

Meshes are **unnamed**. Rendering them does **not** create a logo witness.

## 5. Alternative selector oracle sweep

Coherent sel write: u8 `0x50002A` + u8 `0x50002B` + u32
`0x500028 = (mode<<24)|(sel<<16)|mask`.

### From phase14 park `all0-ready1` (20–40k @ `0xa6c0`, ready=0, nav=0)

| forced sel | result |
| ---: | --- |
| 0 | SEGA-warning draw path (tile writes 5100, glyph reads `0x02a69fxx`); sel→1; **no FIFO** |
| 1,2,4,5,8,10,11 | self-branch/object spin `0x4c9d8`; residual tiles only |
| 6 | tile plane fill `0x01004000`; **no FIFO** |
| 7 | tile writes; sel→8 |
| 9 | stays sel9; ph jumps to 0x0f; **no FIFO** from this park |
| 12,13,14,15,16,18,19 | spin / tile blits; **no FIFO** |
| mode `0x0c/0x0d/0x00/0x10/0x11` with sel=3 | frame path **overwrites** `0x50002B` back to sel; same spin; **no FIFO** |
| board bit9 set vs clear on sel3 ph14 | no logo delta |

### From boot park `park-after-irq` (40k, ready=0, nav=0)

| forced sel | FIFO writes | notes |
| ---: | ---: | --- |
| **9 only** | **1408** | geo_w 408 @ `0x804000+`; funcs 0,41,57,60,61,6,63,53,15,58…; top vals `0x14802929`, `0x1c803939`, `0x03000606`; tiles **empty**; nz_tex 0 |
| 0,1,2,3,4,5,6,7,8,10,12,14–19 | **0** | sel0/3/6/7/12/15/16 do tile work only |
| sel9 @ 120k | 1408 | saturates; still no named tiles |

Function-port `0x880000` and upload `0x980000`: **0 writes** in all
v0372 windows.

## 6. Fail-closed / do not invent

- Do **not** name ids `0x88/0x140–0x157` as logo/title.
- Do **not** treat FIFO immediates (`0x14802929` …) as object ids.
- Do **not** treat `0x70cbc` display ids (`0x0ee1`…) as attract title meshes.
- Tile `t4e jef` / `0x88xx` font bank are **not** logo glyphs.
- SEGA legal warning (v0360, `0x89xx` + ROM string `0xaaad`) remains the
  only named SEGA visual, and it is **2D legal text**, not a 3D mesh.

A future **named** witness would need one of:

1. ROM ASCII / table that binds an object id or poly offset to a title/logo
   symbol (none found); or
2. an oracle path that draws unique title glyphs on tile/texture and is
   correlated to a measured object id; or
3. recovered C already naming such a field (not present — keep `field_xxx`
   style).

## 7. Tools (analysis only, no recovered-C change)

- `tools/python/logo_alt_selector_campaign.py`
- `tools/python/logo_alt_deep_classify.py`
- `tools/python/logo_alt_boot_fifo_sweep.py`
- `tools/python/find_object_submit_sites.py`
- (pre-existing, reused) `correlate_object_table.py`, `dump_poly_objects.py`,
  `measure_pol_test_logo.py`, `render_poly_objects.py`

## 8. Validation

- **No** recovered C / executor change this slice.
- Focused CTest **not re-run** (no C delta; prior gates remain the
  last observed results in v0369–v0371 notes).
- Parks/traces under `out/attr-v0372/`, `out/attr-logo/` — **not for git**.

## 9. Next levers (measured, not speculative)

1. Attribute the **0x20xxx** `call 0x7c60` cluster (54 sites) to
   game_disp / attract object workers; record which g0 ids each site uses
   on the natural attract spin (not forced sel).
2. Dump `0x70cbc` as pointers vs ids — resolve `ldos` halfwords to the
   actual object-id words via a controlled `--set-ip 0x31040` probe.
3. Persist DuckDB/Parquet over phase5-sized traces to list **every**
   object id submitted during natural attract phase3–8 vs TEST.
4. Keep logo recovery fail-closed until name evidence exists.

## Parks / commands

```text
park: out/attr-fs/all0-ready1.vf2snap, out/park-after-irq.vf2snap,
      out/attr-long/long-29.vf2snap
trace: out/attr-long/fifo-phase5.jsonl
summaries: out/attr-v0372/summary.json, boot_fifo_sweep.json
object submit pin:
  vf2probe --snapshot long-29 --set-ip 0x7c60 --until 0x7d14 \
    --set-reg g0=0x148 --set-reg g11=0x884000 \
    --set-u32 0x501018=0x1000 --set-u32 0x50101c=0 --memory-trace
```
