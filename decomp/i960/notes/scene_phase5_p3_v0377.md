# v0377 — Attract phase5 multi-object scene (P3/P4, host analysis)

Status: **host analysis reconstruction from measured submits + FIFO protocol
tags**. Not recovered C. Not a CRT/framebuffer dump. **Named mesh/title/logo
witness: NOT claimed** (fail-closed logo boundary retained).

TGP matrix confidence remains **absent** (`out/attr-transform/measured_view.json`).
Composite layouts are labeled **`no_scene_matrix`** (grid / sequential host
assembly). No game camera is invented.

Git note: PNG/JSON outputs live under `out/attr-render/v0377/scene/` (covered
by `/out/` in `.gitignore`). Commit only tools + this note. **No CHANGELOG
edit in this slice. No git commit (parent integrates).**

## 1. Tools (this slice)

| Tool | Role |
| --- | --- |
| `tools/python/scene_decode_helper.py` | Stable decode surface + protocol-tag classifier/RGB hypothesis |
| `tools/python/build_phase5_timeline.py` | Stream FIFO/object-table trace → timeline JSONL |
| `tools/python/render_scene_attract.py` | Multi-object windows, contact strip, composites (P3 base) |
| `tools/python/render_phase5_scene_v0377.py` | **v0377 deliverable driver**: ordered timeline JSON, grid, temporal strips, wire compare, contact |

`py_compile` observed OK on: `scene_decode_helper.py`,
`build_phase5_timeline.py`, `render_scene_attract.py`,
`render_phase5_scene_v0377.py`, `render_mesh_host.py`,
`render_poly_objects.py`, `apply_tgp_transform.py`.

## 2. Measured timeline

Sources:

- Trace: `out/attr-long/fifo-phase5.jsonl` (~42.2 MB, streamed)
- Object table: `out/main_data.bin` / `roms/vf2`, TABLE `0x020E0004`,
  16-byte records, `word_index = w2 & 0x7fffff`, w0 written to geo `0x800010`
- Reverse map: `w0_to_id` size 5126, `by_id` 8192

| Metric | Count |
| --- | ---: |
| Timeline object_events | **336** |
| Object-table reads | **112** |
| geo/FIFO word0 submits | **224** |
| FIFO protocol tag events | **1089** (tightened classifier) |
| **Unique object ids** | **112** |
| Step range | 34584728 .. 34884727 |

### Measured first-submit id order (excerpt)

`0x750`, `0xf5d`, then odd-id attract family
`0x4d1,0x4d3,...,0x4eb,0x4f0..0x4fe`, `0x503`, attract `0x148`,
display family `0xd03-0xd11`, `0x088`/`0xa7a`, cluster `0xf68-0xf76`,
later attract mix `0x248,0x145,0x14a,0x152,...`, plus
`0x140` family, `0x298-0x2a4`, `0x8f7-0x906`, `0xe9d-0xea2`.

Full unique set: 112 ids (see `phase5_timeline.json` →
`first_submit_id_order`). All 112 decoded to ≥1 sanitized triangle under
skip3 (+ per-object mode fallback).

### Temporal co-occurrence (measured submit order, not CRT proof)

8 strips (`scene_strip_t00..t07.png`), 42 object-events each:

| Strip | Ids (representative) | Notes |
| --- | --- | --- |
| t00 | 0x750,0xf5d,0x4d1..0x4e7 | Early large + odd-id family |
| t01 | 0x4e9..0x503 | Family tail / transition |
| t02 | 0x148,0xd07..0xd0d | Attract + display family |
| t03 | 0xd05,0x088,0xa7a,0xf6c.. | Display + attract + cluster |
| t04 | 0xf68..0xf76,0x248,0x443 | Cluster + later mix |
| t05 | 0xa2a..,0x14d,0x147,0x8fd | Attract family |
| t06 | 0x905,0x3c6,0x2a2,0xe9e.. | Sparse mid/late |
| t07 | 0xe9d..,0x298.. | Late family block |

## 3. FIFO color / protocol tags

### Hypothesis (confidence=**protocol_tag** for EVERY use; NOT game palette)

Measured words follow `0xAABBCCDD` with:

- `BB ∈ {0x00, 0x80}` (protocol class marker in byte 2)
- often `CC == DD` (YY=ZZ)
- `AA` may be `0x00` (e.g. `0x00800101`)

Hypothesis:

- high `AA:BB` is a **protocol tag** (e.g. `0x1480`, `0x1a00`, `0x0080`)
- `CC==DD` is an **intensity** (`r=g=0x29` for `0x14802929` as a byte reading)
- host analysis fill: hue seeded from `tag_high`, value from intensity
- **never** claimed as CRT RGB / recovered palette

Classifier rejects IEEE-float lookalikes (`0x3f800000`, `0x80000000`) whose
low two bytes are 0.

### Color-tag table (measured counts from phase5 trace)

| Word | Tag high | YY/ZZ | Analysis RGB (host) | n | Confidence |
| --- | --- | --- | --- | ---: | --- |
| `0x1a003434` | 0x1a00 | 0x34/0x34 | (129,91,49) | 507 | protocol_tag |
| `0x00800101` | 0x0080 | 0x01/0x01 | (57,55,24) | 165 | protocol_tag |
| `0x01000202` | 0x0100 | 0x02/0x02 | (27,58,24) | 159 | protocol_tag |
| `0x1b803737` | 0x1b80 | 0x37/0x37 | (50,105,133) | 156 | protocol_tag |
| `0x23004646` | 0x2300 | 0x46/0x46 | (154,58,122) | 90 | protocol_tag |
| `0x20804141` | 0x2080 | 0x41/0x41 | (147,116,56) | 72 | protocol_tag |
| `0x14802929` | 0x1480 | 0x29/0x29 | (43,113,62) | 54 | protocol_tag |
| `0x25004a4a` | 0x2500 | 0x4a/0x4a | (60,153,160) | 54 | protocol_tag |
| `0x37806f6f` | 0x3780 | 0x6f/0x6f | (133,212,80) | 42 | protocol_tag |
| `0x21004242` | 0x2100 | 0x42/0x42 | (93,148,56) | 18 | protocol_tag |
| `0x17802f2f` | 0x1780 | 0x2f/0x2f | (56,122,46) | 9 | protocol_tag |
| `0x03000606` | 0x0300 | 0x06/0x06 | (64,24,40) | 3 | protocol_tag |
| `0x09801313` | 0x0980 | 0x13/0x13 | (82,31,45) | 3 | protocol_tag |
| `0x36006c6c` | 0x3600 | 0x6c/0x6c | (182,79,207) | 3 | protocol_tag |
| `0x34806969` | 0x3480 | 0x69/0x69 | (77,203,77) | 0* | protocol_tag |
| `0x35806b6b` | 0x3580 | 0x6b/0x6b | (78,95,206) | 0* | protocol_tag |
| `0x33806767` | 0x3380 | 0x67/0x67 | (200,92,76) | 0* | protocol_tag |
| `0x1c803939` | 0x1c80 | 0x39/0x39 | (136,51,119) | 0* | protocol_tag |
| `0x01800303` | 0x0180 | 0x03/0x03 | (24,60,52) | 0* | protocol_tag |

`n=0*` = listed in the measured FIFO protocol family from handoff/prior
traces; not frequent in `fifo-phase5.jsonl` itself. Table also written to
`phase5_timeline.json` → `color_tag_table`.

### Id ↔ tag correlation (honest limitation)

Dominant nearby-tag histogram among 112 ids:

- `0x01000202` → 34 ids
- `0x1a003434` → 27 ids
- `0x1b803737` / `0x14802929` → 18 each
- `0x37806f6f` → 13
- rare: `0x00800101`, `0x09801313`

**Finding:** near-step FIFO tags are coarse ambient protocol words, not a
unique per-object palette. Many ids share one dominant tag. For multi-object
legibility the renderer blends **70% protocol-tag fill + 30% per-id analysis
palette tint**, still labeled `confidence=protocol_tag` / source
`protocol_tag:0x........+id_tint`. Uncorrelated ids would use muted gray/blue
(`muted_default_uncorrelated`); in this run every id had ≥1 nearby tag.

## 4. v0377 outputs

Primary deliverables (gitignored):

```text
out/attr-render/v0377/scene/phase5_timeline.json
out/attr-render/v0377/scene/phase5_timeline.jsonl
out/attr-render/v0377/scene/scene_grid_ids.png
out/attr-render/v0377/scene/scene_grid_ids_wire.png
out/attr-render/v0377/scene/scene_compare_color_vs_wire.png
out/attr-render/v0377/scene/scene_strip_t00.png .. scene_strip_t07.png
out/attr-render/v0377/scene/scene_strip_wire_t00.png
out/attr-render/v0377/scene/scene_compare_t00_color_vs_wire.png
out/attr-render/v0377/scene/scene_contact.png
out/attr-render/v0377/scene/phase5_scene_report.json
```

Earlier P3 base (same analysis lineage, different naming) remains under
`out/attr-render/scene/`.

### Camera / placement (analysis, not recovery)

- View default **iso-xz** (host basis shared with `render_mesh_host`).
- Decode default **skip3** (tgp.c `geometry_mode&3 < 2` path).
- Layout **`no_scene_matrix` / host_centered_grid**: AABB-centered meshes in a
  grid by median extent. **No measured translate** (TGP matrix absent).
- Extreme decode coords rejected beyond `scene_max_abs` (default 1e4); mode
  fallback skip3↔noskip when primary decode is extreme.

### Grid sheet

| File | Objects | Tris/coverage |
| --- | ---: | --- |
| `scene_grid_ids.png` | 112 | cov 639707 px @1600 |
| `scene_grid_ids_wire.png` | 112 | same geometry, muted wire/shaded |

All-id grid is dominated by a few large-extent meshes (green plates from
`0x750`/`0x248`/`0xf6x` family members under protocol+id tint). Mid-size
objects are more readable in **temporal strips** and **contact** sheet.

## 5. Legibility vs single-object dumps

**Before (single-object, e.g. `render_mesh_host` / `single_id_*.png`):**

- one mesh per PNG;
- tiny skip3 meshes (e.g. `0x088` only a few tris) often near-blank;
- no temporal co-occurrence; no protocol-tag identity across submits.

**After (multi-object v0377):**

- `scene_strip_t00..t07.png` show **measured co-submitted ids** under one
  analysis camera; t02/t03 (display `0xd0x` + attract `0x088`/`0x148`) are
  the most color-separable;
- `scene_contact.png` gives a single analyst sheet for submit order;
- `scene_compare_color_vs_wire.png` shows colors **help family separation**
  when dominant tags differ, but are **not game-accurate** — wire/shaded is
  better for silhouette of large plates; colored fills are better for
  spotting multi-object clusters;
- protocol-tag color is useful as **labeled analysis identity**, not as a
  recovered palette claim.

**Conclusion:** multi-object reconstruction is more legible than single-object
crude dumps for submit co-occurrence and family structure. It remains host
analysis with fail-closed logo/matrix boundaries.

## 6. Fail-closed boundary

1. **No named mesh / no logo / no title claim.**
2. **No CRT/framebuffer claim** — these PNGs are not attract screen output.
3. **No recovered camera** — TGP matrix confidence=absent; grid offsets are
   analysis placement (`no_scene_matrix`).
4. **FIFO tags are protocol words**, not proven RGB; every color use is
   `confidence=protocol_tag`.
5. Composite = host assembly of measured poly-ROM decodes + measured id order.
6. Extreme decode modes stay rejected/sanitized; unknown paths fail closed.
7. Float-like FIFO words (`0x3f800000`, `0x80000000`) are excluded from the
   color family.

## 7. Parks / further traces

Available locally:

- `out/attr-long/long-*.vf2snap` (including `long-29.vf2snap`)
- `out/attr-long/fifo-attract.jsonl`, `out/attr-long/fifo-phase5.jsonl`
- `out/attr-v0372/boot-sel09-fifo.jsonl` and related boot-sel FIFO traces

Re-run:

```sh
python tools/python/build_phase5_timeline.py \
  --trace out/attr-long/fifo-phase5.jsonl \
  --output out/attr-render/v0377/scene/phase5_timeline.jsonl

python tools/python/render_phase5_scene_v0377.py \
  --out out/attr-render/v0377/scene \
  --timeline out/attr-render/v0377/scene/phase5_timeline.jsonl \
  --view iso-xz --decode skip3 --max-strips 8
```

If a parallel agent later measures a TGP matrix into
`out/attr-transform/measured_view.json`, re-run with measured transforms
instead of `no_scene_matrix` grid — do not invent offsets in the meantime.

## 8. Validation observed

- `py_compile` OK on new + related tools (this slice).
- Timeline rebuilt from measured `fifo-phase5.jsonl` + ROM-backed
  `out/main_data.bin` / `roms/vf2`.
- All 112 unique timeline ids decoded to sanitized meshes.
- v0377 PNG/JSON deliverables written under `out/attr-render/v0377/scene/`.
- Color vs wire side-by-side produced for legibility comparison.
- No recovery/executor C change in this slice.
- No CHANGELOG edit. No git commit (parent integrates).
- ROM-backed CTest not required for pure host tooling; not run in this slice.
