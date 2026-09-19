# host_render_views_v0376 — analysis views + legibility re-render (draft)

Status: **draft evidence note (host analysis)**. Not recovered C. Not a logo
or title witness. No CHANGELOG edit. No git commit in this slice.

Fail-closed logo naming retained: **no named mesh claimed**.

## 1. What this slice changed

Tool: `tools/python/render_mesh_host.py` (host analysis rasterizer only).

1. **New analysis views** (measured Work-RAM composition, NOT game camera):
   - `measured-display-scale`
   - `measured-display-iso`
2. **Optional `--transforms` / auto-load for `--view tgp`.**
   - Path probed: `out/attr-transform/measured_view.json`
   - Absent at run time → identity fallback, report `tgp_confidence=absent`
3. **Raster robustness for analysis legibility:**
   - Painter's algorithm (mean projected depth, farthest first) **always on**
     and combined with the existing z-buffer.
   - Backface cull optional via `--cull on|off`; **analysis default `off`**
     so winding cannot empty a mesh.
4. **Contact-sheet column bug fix** (pre-existing):
   - `draw_contact_sheet` used `max(len(r) for r in rows)` where `rows`
     elements are `(meta, cells)` 2-tuples, so every sheet was capped at
     **2 columns** regardless of requested views.
   - Fixed to `max(len(cells) for _meta, cells in rows)`.
   - Old sheets under `out/attr-render/contact_sheet.png` are therefore
     truncated to fit-ortho|iso-xy only; v0376 sheet has all five views.
5. **Filename/report labeling** for measured views uses
   `analysis_measured_display_scale` / `analysis_measured_display_iso`.

`apply_tgp_transform.py` unchanged API; still fail-closed identity when
`confidence=absent`.

## 2. Formula (ANALYSIS only)

```text
ANALYSIS composition of measured Work-RAM words
confidence: host — NOT recovered projection, NOT game camera

T = (6.0, 4.7, 18.5)
    source: display triple *(u32*)0x50084c +0x54/58/5c
            (v0375 park snapshots; display-state words)
S = 600.0
    source: camera scale globals 0x501084 / 0x501088
            (measured nonzero on some parks; 0 on boot-sel03)

p' = (p + T) * S

measured-display-scale: project p' with fit-ortho basis (X,Y,Z)
measured-display-iso:   project p' with iso-xy basis (ax=30, az=45)
```

Explicit non-claims (v0375, still true):

- TGP class **0x09 / 0b / 0c ABSENT** in FIFO protocol traces.
- Display triple is **display-state**, not a proven TGP 3x4 matrix.
- Uniform scale + translate under **auto-fit orthographic** does not change
  2D silhouette vs projecting the raw mesh with the same basis. The view is
  still useful as (a) explicit analysis labeling of measured host state,
  (b) a hook for future non-uniform matrices from `measured_view.json`,
  (c) absolute-depth composition for painter/z (relative order unchanged).

## 3. Run used for v0376 PNGs

```text
python tools/python/render_mesh_host.py \
  --id 0x5c7,0x1cb,0x33e,0x14f,0x150,0x08f,0x148,0xacd,0xacc,0x9e7,0x180 \
  --decode skip3 \
  --views fit-ortho,iso-xy,iso-xz,top,measured-display-iso \
  --out out/attr-render/v0376 --size 512 --cell 128 \
  --cull off --painter --rom-dir roms/vf2
```

- ROM-backed host decode only (poly-ROM table + float-link skip3).
- `tgp matrix source: identity-fallback confidence=absent`
- `out/attr-transform/measured_view.json`: **not present** at run time.
- Outputs are gitignored under `out/attr-render/v0376/`.

Contact sheet: `out/attr-render/v0376/contact_sheet.png` (804×1492 after fix).

Report: `out/attr-render/v0376/render_report.json`.

## 4. Coverage (z-buffer painted pixels @ 512) and legibility

| id | tris s3 | fit-ortho | iso-xy | iso-xz | top | meas-iso | best view |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 0x5c7 | 515 | 13550 | 8172 | 8569 | 1074 | 8174 | fit-ortho |
| 0x1cb | 384 | 933 | 3466 | 4065 | 10291 | 3452 | top |
| 0x33e | 384 | 935 | 3452 | 4051 | 10339 | 3415 | top |
| 0x14f | 49 | 13352 | 13321 | 11888 | 9819 | 13321 | measured-display-iso |
| 0x150 | 49 | 12590 | 11571 | 9533 | 9214 | 11571 | measured-display-iso |
| 0x08f | 257 | 1034 | 631 | 1132 | 1521 | 631 | top |
| 0x148 | 10 | 9752 | 8618 | 7140 | 1526 | 8632 | measured-display-iso |
| 0xacd | 221 | 19476 | 7006 | 11638 | 9650 | 6984 | fit-ortho |
| 0xacc | 219 | 19126 | 7080 | 7776 | 9208 | 7097 | fit-ortho |
| 0x9e7 | 261 | 16680 | 8071 | 7540 | 722 | 8052 | fit-ortho |
| 0x180 | 233 | 362 | 1973 | 3175 | 2940 | 1973 | iso-xz |

### Before / after observations (host PNG legibility)

**Painter/z-buffer/cull:** For these skip3 meshes the z-buffer alone already
produced correct occlusion; enabling painter mean-depth sort produced
**byte-identical PNGs** for shared view names vs the prior `out/attr-render/`
run (e.g. `id_5c7_skip3_iso-xy_512.png` 22429 B both). Meshes were never
empty because of winding — cull was already effectively off. The real
legibility lever is **view choice**, not empty-mesh recovery.

**View choice (dominant effect):**

| id | fit-ortho (often “empty-looking”) | better views |
| --- | --- | --- |
| 0x5c7 | edge-on thin band; structure hard to read | **iso-xz** multi-tier stage/arena-like mesh is legible; top is nearly empty (1074 px) |
| 0x1cb / 0x33e | almost flat line (933/935 px) | **iso-xz** and **top** reveal a dense checkered diamond plane (best legibility) |
| 0x14f / 0x150 | already solid | iso-xy / measured-display-iso keep a clear triangular fan / flag-like solid |
| 0x08f | low coverage all axes | **iso-xz** elongated blade; still sparse — thin geometry, not a raster bug |
| 0x148 | fit-ortho/iso-xy solid | attract-family multi-plane mesh readable; iso-xz also good |
| 0xacd / 0xacc | fit-ortho dense box-like | iso-xz shows layered shell / X-cross structure |
| 0x9e7 | fit-ortho solid | **iso-xz** shows vertical pillars with base bulbs — good silhouette |
| 0x180 | fit-ortho nearly empty (362 px) | **iso-xz** (3175 px) elongated craft/fin-like form |

**measured-display-iso vs iso-xy:** PNG nonbg counts match to rounding
(e.g. 0x1cb: both 34660 nonbg; 0x5c7: both 50328). Expected under uniform
`p'=(p+T)*S` + auto-fit. Labeling is the primary deliverability gain; a real
silhouette change requires a measured non-uniform TGP matrix
(`measured_view.json` / `--transforms`).

**Old contact sheets:** Prior `out/attr-render/contact_sheet.png` only showed
`skip3|fit-ortho` and `skip3|iso-xy` because of the 2-tuple column bug. The
v0376 sheet shows all five requested columns, which makes iso-xz/top/
measured-display-iso immediately comparable.

## 5. Best PNGs for inspection (absolute paths)

```
out/attr-render/v0376/contact_sheet.png
out/attr-render/v0376/id_5c7_skip3_iso-xz_512.png
out/attr-render/v0376/id_1cb_skip3_top_512.png
out/attr-render/v0376/id_33e_skip3_top_512.png
out/attr-render/v0376/id_14f_skip3_iso-xy_512.png
out/attr-render/v0376/id_14f_skip3_analysis_measured_display_iso_512.png
out/attr-render/v0376/id_150_skip3_iso-xy_512.png
out/attr-render/v0376/id_148_skip3_fit-ortho_512.png
out/attr-render/v0376/id_acd_skip3_iso-xz_512.png
out/attr-render/v0376/id_9e7_skip3_iso-xz_512.png
out/attr-render/v0376/id_180_skip3_iso-xz_512.png
out/attr-render/v0376/id_08f_skip3_iso-xz_512.png
out/attr-render/v0376/id_1cb_skip3_analysis_measured_display_iso_512.png
```

## 6. Fail-closed / naming

- **Logo/title: NOT witnessed.** No semantic mesh names in filenames or
  report. Ids remain numeric (`0x5c7`, `0x1cb`, …).
- Measured display words are host **analysis inputs**, not recovered camera.
- `--view tgp` stays identity when no measured matrix file exists
  (`confidence=absent`).
- Do not treat fit-ortho low coverage as “mesh empty” — prefer iso-xz/top
  before concluding decode failure.

## 7. Validation observed

- `py_compile` clean on `tools/python/render_mesh_host.py`.
- `apply_tgp_transform.py` left unchanged; absent-matrix path remains
  fail-closed.
- v0376 full re-render completed: 11 ids × skip3 × 5 views + contact sheet.
- Contact sheet dimensions 804×1492 after column-count fix.
- No `src/recovered` changes. No CHANGELOG edit. No git commit.
- ROM-backed game differential: not applicable (host analysis tooling only).
