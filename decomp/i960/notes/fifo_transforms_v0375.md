# v0375 — FIFO/TGP transform extraction (Phase B, fail-closed)

Status: **TGP 3x4 matrix / focus / geometry_mode stream commands: ABSENT**
in the measured attract/boot FIFO and geo traces after protocol false-class
filtering. **Named 3D logo: NOT witnessed.** Fail-closed retained.

This note is evidence-only. No recovery C changes. No invented camera.

## 1. Sources streamed

| Trace | JSONL lines | FIFO writes | geo-port writes |
| --- | ---: | ---: | ---: |
| `out/attr-long/fifo-phase5.jsonl` | 449891 | 2438 | 481 |
| `out/attr-long/fifo-attract.jsonl` | 206448 | 1685 | 195 |
| `out/attr-v0372/boot-sel03-fifo.jsonl` | 50449 | **0** | **0** |
| `out/attr-v0372/boot-sel09-fifo.jsonl` | 52612 | 1408 | 126 |
| `out/attr-v0372/boot-sel09-long.jsonl` | 155182 | 1408 | 126 |
| `out/attr-logo/emit31040.jsonl` | 88 | 0 | 0 |

Machine-readable extract: **`out/attr-render/transforms_phase5.json`**.

Tools:

- `tools/python/extract_fifo_transforms.py` — streaming JSONL classifier
- `tools/python/apply_tgp_transform.py` — stable `apply_transform(points, matrix16, focus_x, focus_y)`

## 2. Aggregate memory-event counts (all traces)

| Metric | Count |
| --- | ---: |
| FIFO writes `0x884000-0x885fff` | 6939 |
| geo RAM writes `0x800000-0x80ffff` | 2034 |
| geo-port writes `0x800010`/`0x804000` | 928 |
| object-table reads `0x020e0xxx` | 133 |
| aperture cursor `0x5001e4` R/W events | 171 |
| unique object-table w0 values written | 123 |
| object write events (w0 match) | 268 |
| table-read events | 132 |

## 3. TGP class-07/09/0b/0c — measured ABSENT as stream commands

Class bits on FIFO/geo **write word0** (`class = (word >> 23) & 0x1f`)
show many collisions. After excluding FIFO protocol/color immediates:

| Class | Raw bit hits | Protocol false-class | Remaining non-protocol |
| --- | ---: | ---: | ---: |
| 0x07 mode | 94 | 72 | 22 (IEEE-like, see §3.1) |
| 0x09 focus | 412 | 412 | **0** |
| 0x0b matrix | 38 | 38 | **0** |
| 0x0c translate | 46 | 46 | **0** |

Protocol family (from `logo_object_submit_v0372` / `attract_poly_objects_v0372`,
confirmed in these traces): `0x00800101`, `0x03000606`, `0x1a003434`,
`0x14802929`, `0x33806767`, `0x34806969`, `0x35806b6b`, `0x36006c6c`, …
Pattern `0x??80YYZZ` / `0x??00YYZZ` with repeated color bytes. **Not** TGP
`geometry_mode` / focus / matrix opcodes.

Packed-stream parse of the same FIFO/geo-port word samples also found
**0** class-07/09/0b/0c commands (parse errors on protocol words, as expected).

`live_guess` in `transforms_phase5.json`:

```json
{
  "geometry_mode": null,
  "focus_x": null,
  "focus_y": null,
  "matrix": null,
  "confidence": "absent"
}
```

`apply_tgp_transform.py` therefore passthroughs identity / focus 1.0 and
labels `confidence=absent` — **not** a recovered camera.

### 3.1 Residual class-07 bit collisions (not accepted as mode)

22 FIFO writes with word0:

| word0 | IEEE | n |
| --- | ---: | ---: |
| `0x43850000` | +266.0f | 19 |
| `0xc3850000` | −266.0f | 2 |
| `0xc3850666` | ≈ −266.02f | 1 |

These are float immediates whose bits collide with class 0x07. No measured
`command[1]` mode word follows them in the same FIFO window (`mode=null`).
**Not accepted as `geometry_mode`.**

### 3.2 Geo-port value dump (alternate hypothesis — parent analysis)

First measured words at geo port stream (from phase5-class traces):

```text
0x00000003 0x00000001 0x00000000 0x000000c9
0x00040401 0xbc902de0 0xbc11d14e 0x00ac1502
0x3cf69446 0x3cf69446 0x00ac1502 0x3ce075f7
0x3ce075f7 0x00ac1502 0x3ce147ae 0x3ce147ae
```

Interpretation stays fail-closed: mixed control/count words and small IEEE
floats consistent with mesh/prim packets, **not** a class-09/0b camera packet.

FIFO sample head (protocol + float immediates):

```text
0x31006262 0x00000000 0x3f9478ea …
0x33806767 0x00003a00 0x00800101 0x03000606
```

`0x3f9478ea` ≈ 1.164f appears as a FIFO immediate — **measured**, but not
packaged as TGP class-09 focus words.

## 4. Object submissions (measured, numeric only)

Helper `0x7c60` protocol (existing v0372/v0373 evidence) is reconfirmed:

- table base `0x020e0004`, 16 B/id;
- writes w0 to geo `0x800010` and `0x804000` (and FIFO `stq`).

IDs with measured geo-port writes in these traces include families:

| id | w0 | notes |
| --- | --- | --- |
| `0x750` | `0x00488b06` | |
| `0x4f0`–`0x4fe` odd stride | `0x001b6ac4`… | high-id family |
| `0x503` | `0x001b7742` | |
| `0xf5d` | `0x00590b1e` | |
| `0x4d1`,`0x4d3`,… | `0x001b616a`… | |
| `0x1402` | `0x005fd124` | |

Attract-family ids `0x088`/`0x148`/`0x140–0x152` remain in prior v0372 notes;
this extract’s write window is dominated by the high-id helper submissions
above. **No name binding.** Do not claim logo.

## 5. Park-state snapshot dumps (snap parser, read-only)

Parks: `out/attr-long/long-29.vf2snap`,
`out/attr-v0372/boot-sel03-fifo.vf2snap`,
`out/attr-logo/emit31040.vf2snap`.

| Field | long-29 | boot-sel03 | emit31040 |
| --- | --- | --- | --- |
| guest IP | `0x00022498` | `0x00002d38` | `0x000310c8` |
| aperture cursor `0x5001e4` | `0x00000074` | `0x00000030` | `0x000000ec` |
| snap geometry region len | 32768 | 32768 | 32768 |
| geometry nz words | 41 | 37 | 41 |
| `0x50084c` display ptr | `0x00515d00` | `0x00515d00` | `0x00515d00` |
| display +0x54/58/5c | **6.0 / 4.7 / 18.5** | same | same |
| display bits | `0x40c00000 / 0x40966666 / 0x41940000` | same | same |
| display flags +0x40 | `0x00000000` | `0` | `0` |
| display secondary +0x60 | `0x00000000` | `0` | `0` |
| camera scale `0x501084` | `0x44160000` (=600.0f) | `0x00000000` | `0x44160000` |
| camera scale `0x501088` | `0x44160000` (=600.0f) | `0x00000000` | `0x44160000` |

Display triple **(6.0f, 4.7f, 18.5f)** exactly matches the ROM-measured
constants in `display_runtime_followup_v0026` /
`display_transform_defaults` (`0x40c00000`, `0x40966666`, `0x41940000`).
This is **Work-RAM display-state**, not a TGP 3x4 matrix.

Camera scale globals 600.0f at `0x501084`/`0x501088` are consistent with
`camera_initialization.md` (those addresses named as camera scale). Value is
**measured in snap work RAM**, not copied from `src/recovered/hybrid.c`
math into a host raster.

Geometry region (snap “geometry” 32k window) is **control/pointer-like**,
not a matrix/focus stream. long-29 head:

```text
0x80000000 … 0x0008942e 0x00000004 … 0x00008808 0x00000088
```

(emit31040 similar with `0x00080660` instead of `0x0008942e`.)
Consistent with prior note: snapshot geometry is a register/program window,
not the full TGP command FIFO.

## 6. Probe caution

`build/Debug/vf2probe.exe --max-steps 0 --read-u32 …` **did not freeze**
at restore: observed `run_instructions` in the 10^4–10^6 range on these
parks. Final `--read-u32` values are therefore **post-execution**, not park
state. Park-state dumps above come from `dump_attract_state.py` snap parsing
only. No probe `--set-*` / `--output-snapshot` was used; no mutation claimed.

## 7. Fail-closed / Phase A guidance

1. **Do not invent a camera** from hybrid `fa_camera` C or display defaults
   for host raster of attract poly objects.
2. Usable **measured** host inputs today:
   - object IDs + polygon ROM offsets (v0372 table);
   - geo-port/FIFO packet hex dumps (§3.2);
   - display triple `(6.0, 4.7, 18.5)` at measured object `0x515d00+0x54`
     (display-state, not TGP matrix);
   - camera scale globals `0x501084/0x501088 = 600.0f` where nonzero.
3. `apply_transform(points, matrix16, focus_x, focus_y)` is ready; until a
   **measured** TGP matrix/focus exists it must stay identity passthrough
   with `confidence=absent`.
4. Logo naming remains unsupported.
5. Next evidence levers (not implemented here): correlate `0x7c60` callers
   that might also emit class-0b/09 words into **other** ports; dump more
   complete consecutive FIFO windows around helper returns; measure whether
   TGP microcode produces matrix state only inside copro/geo registers not
   visible as i960 memory writes.

## 8. Validation observed

- `py_compile` clean on both new tools.
- `apply_tgp_transform.py` CLI: `confidence=absent`, projected points =
  inputs (documented passthrough).
- No `src/recovered` / `CHANGELOG` / git commit in this slice.
- ROM-backed differential: not required (no recovery semantics changed).
