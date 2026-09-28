# v0377 P2 — TGP matrix / focus / projection on ALTERNATE ports

Status: **absent** (fail-closed).  
Does not relitigate FIFO/geo stream class-09/0b/0c (already absent v0375/v0376).  
Does not invent a camera. Does not promote hybrid.c / fa_camera to live projection.  
Do **not** git-commit this note together with snaps/traces/ROMs.

Canonical deliverable:

```text
out/attr-transform/measured_view.json
  confidence = "absent"
  matrix / focus_x / focus_y / geometry_mode = null
  ports_checked = [...]
  negative_evidence = {...}
```

Supporting artifacts (local, not for git):

```text
tools/python/measure_matrix_ports.py      # probe definitions + park dumps
tools/python/analyze_matrix_ports_p2.py   # ROM/parks/traces analyzer
out/attr-transform/*_v0377.json
out/attr-transform/probe_*_v0377.jsonl
```

---

## 1. P2 question

Is a **measured** TGP geometry matrix / focus / projection (or `geometry_mode`)
delivered on any port **other than** the already-absent FIFO/geo stream
class-09/0b/0c tags?

---

## 2. Model 2A map (from `include/vf2/model2a.h`)

| Region | Base | Size | Role in P2 |
| --- | --- | --- | --- |
| geometry (geo_w/geo_r) | `0x00800000` | `0x8000` | MAME geo command stream; matrix would be class-0b |
| copro port (FIFO) | `0x00880000` | `0x8000` | measured arith tags; `+0x4000` = `0x00884000` data path |
| buffer RAM | `0x00900000` | `0x80000` | aperture `0x0090e000` lives here |
| video/coprocessor ctl | `0x00980000` | `0x1000` | boot control, not matrix payload |
| work RAM | `0x00500000` | `0x100000` | display/camera/task state |

`src/hardware/tgp.c` models `geometry_matrix[16]`, focus, and
`geometry_mode` as **stream opcodes**:

| Class | Handler | Meaning in model |
| --- | --- | --- |
| `0x07` | `geometry_mode = command[1]` | mode |
| `0x09` | `focus_x/y` | focus |
| `0x0b` | `geometry_write_matrix` | 16-float matrix from 13 words |
| `0x0c` | matrix values `[12..14]` | translate |

Guest never delivered those classes in measured windows → model remains
identity passthrough. That is an **oracle/model** fact, not recovered camera.

---

## 3. Per-port absence table

| Port | Addr | Dynamic writes in v0377 probes | Measured content | Matrix? | Confidence |
| --- | --- | ---: | --- | --- | --- |
| aperture buffer | `0x0090e000[cursor]` | 3 (emit probe) | `(6.0f, 4.7f, 18.5f)` only | **no** | measured_non_matrix |
| display object | `*(u32*)0x50084c+0x00..0x90` | snap dump | triple at `+0x54/58/5c`; flags `+0x40`; damped `+0x60/64` | **no** | measured_non_matrix |
| camera task | live `g13` / `0x515400` | snap + camera probe | fields + FIFO readback `+0x5c/60` | **no** | measured_non_matrix |
| camera scale | `0x501084/88` | store IPs `0x1d34c/5c` | `600.0f` | **no** | measured_non_matrix |
| work-RAM 12-float window | `0x500000..0x520000` | park scan | **0** identity-like matrices; `0x50a0e0` = unit profile table | **no** | absent |
| TGP function port | `0x00880000` | **0** | ROM hits unaligned/data-like | **no** | absent |
| TGP upload / video ctl | `0x00980000` | **0** in probes | boot `lda@0x0f0c` then `st 0,0x0c(r4)` | **no** | absent |
| geo control | `0x00800000` | **0** matrix payload | object-submit may write g10-relative object word when g10=geo base | **no** | absent |
| geo program | `0x00804000` | **0** | **0** absolute immediates in maincpu.bin | **no** | absent |
| copro FIFO | `0x00884000` | 27 across traces | arith tags `0x0b001616` (class **0x16**), `0x12002424`, … + operands | **no** | measured_non_matrix |
| FIFO/geo class 09/0b/0c | stream | (v0375/v0376) | protocol false-class only | **no** | absent (not relitigated) |

**Overall:** `measured_tgp_matrix_on_alternate_port = false`,
`measured_focus_on_alternate_port = false`,
`measured_geometry_mode_on_alternate_port = false`,
`confidence = absent`.

---

## 4. Aperture consumer — measured

### 4.1 `display_command_emit` @ `0x31040`

Dynamic probe (correct freeze, **not** `--max-steps 0`):

```text
vf2probe --snapshot out/attr-logo/emit31040.vf2snap
  --set-ip 0x00031040 --until 0x000310cc --max-steps 256
  --memory-trace --trace
```

Measured writes:

| Step IP | Addr | Value | Meaning |
| --- | --- | --- | --- |
| `0x31068` | `0x0090e0ec` | `0x40c00000` (6.0f) | display+0x54 |
| `0x31078` | `0x0090e0f0` | `0x40966666` (4.7f) | display+0x58 |
| `0x31088` | `0x0090e0f4` | `0x41940000` (18.5f) | display+0x5c |
| `0x31094` | `0x005001e4` | `0xf8` (was `0xec`) | cursor += 12 after 3 stores |

FIFO protocol words also written via `(g11)[g12]`: `0x00800101`, `0x37806f6f`.  
**Zero** writes to `0x00800000–0x0088ffff`, `0x00980000`, or any 12-float object.

### 4.2 Object-submit helper @ `0x7c60`

Called from `0x310b8`. Measured:

- FIFO protocol `0x1a003434`;
- object-table load via `lda 0x020e0004[g0*16]` then `ldq`;
- `st r8, 0x10(g10)` / `stq r8, (g10)[g12]` — **object word**, not matrix;
- in the logo park, `g10≈0` so effective addresses landed at low g10-relative slots (`0x12/0x03/0x07/0x0b/0x0f`), not geo `0x800010`.

Xrefs to `0x7c60`: many object-submit callers (`0x19xxx`, `0x20xxx`, `0x30xxx`, …).  
None measured writing 12 floats to geo/TGP/copro.

### 4.3 Other aperture writers (ROM)

- `0x0090e000` appears as an immediate/displacement **266** times in maincpu.bin.
- Cluster example `@0xa400`: same pattern — `ld 0x5001e4`, `st value,0x0090e000(r15)`, `stob cursor`, FIFO tags `0x36006c6c` / `0x36806d6d` / `0x37006e6e` / `0x37806f6f`.
- `vf2i960 xrefs 0x0090e000` returns **0** absolute refs because stores use the displacement form.
- Recovered C also writes aperture (`hybrid.c`, `native_runtime.c`, `texture_bridge_match.c`, pose-diag base `0x0090e000`) — protocol/object/pose buffer semantics. **No measured consumer** converts that buffer into a 3x4/4x4 TGP matrix.

**Conclusion:** aperture consumers/serializers handle a **protocol packet**
(display triple + tags + object words). They do **not** upload transformed
vertices or a TGP matrix.

---

## 5. Camera path after `0x1d4c4` — measured consumption

### 5.1 Full path store inventory (`0x1d320 → 0x1d660`)

Probe: `--set-ip 0x1d320 --until 0x1d660 --max-steps 4000 --memory-trace` on
`long-29.vf2snap`. Halt fail-closed at unsupported `ip=0x1f5a4` after 2931
steps; **233** writes before halt.

| Class | Count | Content |
| --- | ---: | --- |
| work-RAM | 84 | task/profile/scale fields |
| copro FIFO `0x00884000` | 24 | arith/protocol tags + operands |
| other | 125 | tables / backup / non-IEEE |
| `0x00880000` / `0x00980000` / `0x0090e000` / geo `0x800000`/`0x804000` | **0** | — |

### 5.2 What happens after `st task+0x60` @ `0x1d4c4`

Disasm (`vf2i960 disasm 0x1d458 120`) + probe
`--set-ip 0x1d4c4 --until 0x1d660`:

| IP | Action | Destination |
| --- | --- | --- |
| `0x1d4c8` | `ldob task+0x40` mode | register |
| `0x1d4cc` | `ld 0x0006e2e4[r3*4]` | callx table |
| `0x1d4d4` | `callx (r4)` | mode continuation |
| `0x1d4d8` | `ldt task+0x18` | g0.. |
| `0x1d4e4..` | FIFO `0x12002424` + operands | copro arith |
| `0x1d514` | `st` | task+0x1c |
| `0x1d544` / `0x1d5c4` | `st`/`stis` | `0x0050a014` / `0x0050a024` |
| `0x1d5dc..0x1d5f4` | `st 0` | `0x00500174..180` |
| `0x1d610..0x1d634` | `ld 0x0050a0e0[mode]` → `st` | `0x0050109c` / `0x005010a0` |
| `0x1d640..` | `stos 0` | `0x005010e8/ea` weights |

**No** store to geo `0x008000xx` matrix registers, TGP function port, or a
12-float matrix object.

### 5.3 Who writes `223.2f` / `172.8f`?

| Pattern | ROM `lda` sites | Dynamic store IPs | Other writers |
| --- | --- | --- | --- |
| `0x435f3333` (223.2f) | **only** `0x0001d48c` | FIFO push then `0x0001d498` → task+0x5c | **none measured** |
| `0x432ccccd` (172.8f) | **only** `0x0001d4b8` | FIFO push then `0x0001d4c4` → task+0x60 | **none measured** |

Path (fail-closed classification):

```text
lda 0x0b001616          ; copro arith tag, class bits = 0x16 — NOT class 0x0b matrix
st  FIFO
ld  0x501084/88         ; 600.0f scale
st  FIFO
lda 223.2f / 172.8f     ; operand
st  FIFO
ld  FIFO                ; readback
st  task+0x5c / +0x60   ; camera task scratch
```

Recovered `camera_viewport.c` names `CAMERA_SCRATCH_ADDRESS =
VF2_COPRO_PORT_BASE+0x4000 = 0x00884000`. That is the **FIFO data port**,
not a TGP matrix register. Do **not** promote `+0x5c/+0x60` to `focus_x/y`.

### 5.4 Display transform update @ `0x311b8`

Alternate emit branch. Damps/lerps the display triple, `stt` back to
`display+0x54`, may `stt display+0x60`, FIFO protocol `0x13802727` + operands.
Still Work-RAM display-state. Not TGP matrix delivery.

---

## 6. Snap-parser dumps (identity-like 12-float scan)

Parks scanned: `long-29`, `emit31040`, `boot-sel03-fifo`, `boot-sel09-fifo`,
`all0-ready1`, `sixth-fresh`.

Regions scanned per park: work-RAM `0x500000..0x520000`, geometry snap region,
copro-port snap region, buffer-ram snap region.

Result:

- `any_identity_like_matrix = false` in `out/attr-transform/summary_v0377.json`
- Rejected false positive: `0x0050a0e0` twelve consecutive `1.0f` — unit
  **profile table** indexed by mode (`0x50a0e0[mode]` at `0x1d610`), not identity 3x4.
- Display object (long-29 `0x515d00`): triple at `+0x54/58/5c`; zeros elsewhere;
  no 12-float block.
- Camera task nonzero fields: vectors/mode/fields + FIFO readback scratch — not matrix.
- Aperture window is Model2A; not present as a snap region. Values come from
  memory-trace (section 4).

---

## 7. ROM static search — camera scratch bits / ports

| Pattern | Hits | Classification |
| --- | --- | --- |
| `223.2f` `0x435f3333` | `0x1d48c` only (`lda` imm) | camera FIFO operand |
| `172.8f` `0x432ccccd` | `0x1d4b8` only (`lda` imm) | camera FIFO operand |
| `600.0f` `0x44160000` | `0x1d348`, `0x1d358` lda; `0x6e2b8/bc` data | camera scale |
| `0x0b001616` | `0xa620`, `0x1d474`, `0x1d4a0`, `0x4e428..` | copro arith tags |
| `6.0f` | `0x31010`, `0x311ec`, `0x1d778`, … | display defaults / camera gate divisor |
| `0x0090e000` | 266 displacement sites | aperture protocol writers |
| `0x00880000` | 3 unaligned/data-like | not function-port stores |
| `0x00980000` | `lda@0x0f0c`, `lda@0x2ec8` | boot control |
| `0x00804000` / `0x00884000` as abs imm | **0** | FIFO/geo accessed via base registers |

Boot `@0x0f0c`: `lda 0x00980000,r4 ; st 0,0x0c(r4)` — control clear, not payload.

---

## 8. Fail-closed rules (parent must honor)

1. **Do not** promote `(6.0, 4.7, 18.5)` to a view/projection matrix.
2. **Do not** promote FIFO word `0x0b001616` to a TGP matrix opcode
   (class bits = `0x16`).
3. **Do not** promote camera-task `223.2f` / `172.8f` to `focus_x` / `focus_y`.
4. **Do not** promote `0x50a0e0` (12×`1.0f`) to identity matrix.
5. **Do not** invent projection from `src/recovered/hybrid.c` / `fa_camera` /
   `camera_viewport.c`. Recovered effects measured at the oracle stay
   **effects only**.
6. `apply_tgp_transform` stays **identity passthrough** until a live measured
   matrix+focus port exists.
7. Instrumentation remains passive; do not mutate oracle behavior.
8. Do not commit snaps, large traces, or ROM-derived dumps.

---

## 9. Freeze recipe used (for reproducibility)

```text
# Correct: set-ip + until
vf2probe --rom-dir roms/vf2 \
  --snapshot out/attr-logo/emit31040.vf2snap \
  --set-ip 0x00031040 --until 0x000310cc --max-steps 256 \
  --memory-trace --trace

vf2probe --rom-dir roms/vf2 \
  --snapshot out/attr-long/long-29.vf2snap \
  --set-ip 0x0001d4c4 --until 0x0001d660 --max-steps 2000 \
  --memory-trace
```

**Do not use `--max-steps 0` as freeze** (already measured: it does not freeze).

---

## 10. Validation observed

- `python tools/python/analyze_matrix_ports_p2.py` →
  `confidence=absent`, `matrix=null`, `any_identity_like_matrix=false`,
  `geo_writes_count_all_traces=27` (all copro FIFO arith/protocol),
  `aperture_writes_count_all_traces=3` (display triple only).
- Emit-from-`0x31040` probe: aperture triple confirmed; geo/TGP ports untouched.
- Camera post-`0x1d4c4` probe: work + FIFO only; no geo matrix stores.
- Park 12-float scans: no accepted matrix candidates.
- ROM scan: `223.2f`/`172.8f` only at camera path lda sites.
- No changes to `src/recovered`, executor, oracle, or `CHANGELOG`.
- ROM-backed differential for new recovery semantics: **not applicable**
  (no new native semantics; absence evidence only).
- **No git commit** (parent integrates).

---

## 11. Findings worth promoting

- Alternate-port matrix delivery is **closed negative** for attract/long/logo
  parks: no measured 3x4/4x4 + focus outside already-absent stream classes.
- Aperture is a **protocol buffer** (display triple + tags + object words),
  not a matrix upload port.
- Camera `+0x5c/+0x60` are FIFO arith **task scratch**, not focus.
- `tgp.c` already knows how to *consume* class-07/09/0b/0c if the guest ever
  delivers them; until then identity is the only evidence-backed transform.
- Next high-value work is still **guest edge coverage** of paths that *might*
  push stream classes, not more sideband port hunting on these parks.
