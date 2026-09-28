# v0378 — TGP / object-table **w2** consumption outside i960 guest writes

Status: **fail-closed**.  
Measured guest **pointer** write of object-table w2 into geo RAM.  
**No** measured oracle signal that TGP consumes polygon ROM via that w2
outside i960 guest writes (no function-port kick, no upload, no distinctive
poly-body RAM residue, no platform render report from our TGP model).

Evidence-only note. No recovery C changes. No invented camera/mesh/logo.
No git commit. MAME stays **suggestion → oracle measures**.

---

## 1. Question

Attract/pol_test submit object-table records at `0x020e0004[id*16]`.
Field **w2** (word +8) carries a polygons-ROM word index with bit
`0x00800000` (measured: id `0x148` → `0x00840430` / index `0x40430`;
id `0x97d` → `0x0098a0b1` / index `0x18a0b1`).

Does the original hardware **consume** polygons ROM via that w2 outside
what the i960 guest itself writes? Candidate signals:

| Signal class | What would count |
| --- | --- |
| TGP microcode upload | writes to `0x00980000` / program words via FIFO under bit31 |
| Function port | writes to `0x00880000` (MAME `copro_function_port_w`) |
| Model 2A poly RAM / geo stream | class-0x01 object commands or poly-body words in geo/FIFO |
| Live RAM residue | poly word index or distinctive mesh words persisting after submit |
| Platform render report | our `vf2_tgp` report showing object/polygon commands from w2 |

---

## 2. MAME suggestions (all **unproven** until our oracle measures)

Source: `third_party/mame-model2-ref/model2.cpp` + `model2.h`,
`third_party/mame-mb86233/*` (BSD-3, analysis only).

| # | MAME statement | Tag |
| --- | --- | --- |
| M1 | `polygons` ROM region ~8 MiB "Models"; **not** mapped into i960 space | unproven / structural |
| M2 | i960 map has `main_data` at `0x02000000`; object-like tables live there | **measured** (our table `0x020e0004`) |
| M3 | `copro_data` banked into TGP when bank bit `0x800000` set — collision/height-like extra data, **not** the polygons region | unproven |
| M4 | Function port `0x00880000`: write packs `data&0x800fffff` with `func=((offset>>2)&0xff)<<23` into copro FIFO-in | unproven (0 writes measured) |
| M5 | Copro program upload: `0x00980000` bit31 → FIFO writes become `copro_tgp_program[]` | unproven (0 writes measured on attract) |
| M6 | `geo_ctl1_w` @ `0x00980008`: bit31 edge starts/boot geo upload; `geo_prg_w` @ `0x00804000` pushes command words to buffer RAM unless upload mode | unproven as **polygon consumption** |
| M7 | `geo_w` @ `0x00800000` packs function/class into geo buffer; handlers declared `geo_object_data`, `geo_polygon_data`, `geo_matrix_write`, … | unproven; **implementations not present** in our `third_party` slice (decls only) |
| M8 | TGP microcode (MB86234) computes transforms; host need not write matrix every object | unproven; our attract matrix ports already **ABSENT** (v0375–v0377) |
| M9 | Hypothesis: a video/TGP path later walks object address w2 into polygons ROM when rendering | **unproven — this is the gap** |

Clean-room rule: none of M1–M9 enter `src/recovered/` without oracle equality.

---

## 3. Our hardware model / recovered runtime boundary

### 3.1 What `tgp.c` already models

`src/hardware/tgp.c`:

- `vf2_tgp_attach_polygon_rom` / `vf2_tgp_read_polygon_word`: word index
  masked to power-of-two polygons size (`byte = word_index * 4`).
- `vf2_tgp_execute_geometry_stream` class `0x01`: `object_address = command[3]`;
  bit `0x00800000` → `polygon_rom`; base = `object_address & 0x7fffff`;
  `geometry_execute_object` float-link walks ROM/RAM via `geometry_source_read`.
- `vf2_tgp_write_function_port` / `vf2_tgp_upload_program_word`: FIFO/program
  model mirroring MAME packing (**host API**, not native guest path).

Unit tests: `tests/hardware/test_tgp.c` (`vf2_tgp_tests`) exercise these
against **synthetic** streams — not against attract parks.

### 3.2 Who calls `vf2_tgp_execute_geometry_stream`

**Only** `src/game/game.c` (host game path):

- `game_render_native_geometry_ring` after scanning Model2A geometry words;
- `game_render_native_geometry` on captured `native_copro_words`;
- `vf2_game_submit_geometry` explicit host submit.

Grep of `src/recovered/` + `src/hardware/`: **no recovered native corridor
calls** `vf2_tgp_execute_geometry_stream`.

### 3.3 What the recovered native corridor does with objects

`src/recovered/polygon_object_submit.c` (helper `0x7c60`):

- reads gate `0x501018/0x50101c`;
- FIFO protocol `0x1a003434`;
- loads table record `0x020e0004[g0*16]` into r8–r11;
- writes **w0** to geo `g10+0x10` and **stq r8** (w0,w1,w2,w3) to geo ring;
- does **not** read polygons ROM; does **not** call TGP execute; does **not**
  write `0x880000` / `0x980000`.

`src/recovered/texture_bridge_geometry.c`:

- `execute_geometry_command_setup` / `execute_geometry_frame_commit` only
  push command/control words into geometry buffer — no polygon-ROM walk.

`src/hardware/model2a.c`:

- maps geometry / copro-port / copro-control as passive RAM;
- optional copro read/write callbacks; default store;
- **no** automatic TGP microcode that consumes w2.

### 3.4 Boundary (why host dumps stay crude)

```text
Measured guest:   table w2 pointer -> geo RAM
Missing:          any oracle-measured step that TGP/video then fetches
                  polygons[word_index] outside further i960 writes
Host tools:       decode polygons.bin offline at w2*4 (analysis only)
```

Therefore recovered runtime **never feeds geometry streams from object-table
w2**. Host mesh dumps are ROM-static hypotheses, not differential-proven TGP
consumption. Keep them analysis-only (`render_*.py`, `rank_poly_objects.py`).

---

## 4. Measured oracle signals (this slice)

Tools:

- `tools/python/trace_tgp_w2_consumption.py` (new, passive streaming)
- prior: `measure_pol_test_p1.py`, `analyze_pol_test_oracle.py`,
  `measure_matrix_ports.py`, `correlate_object_table.py`,
  `dump_attract_state.py`
- traces: `out/attr-long/fifo-phase5.jsonl`, `ready-trace.jsonl`,
  `out/attr-p1/pol_test_*.jsonl`, `out/attr-logo/long29-obj.jsonl`,
  `out/attr-v0372/pol7c60-id148b.jsonl`
- parks: `long-29`, `emit31040`, `pol_test_g7c60_97d`, `sixth-fresh`,
  `boot-sel03-fifo`, `all0-ready1`
- compact report: `out/attr-p1/tgp_w2_consumption_v0378.json`

### 4.1 Present / measured

| Signal | Result | Evidence |
| --- | --- | --- |
| Guest read object-table **w2** | **yes** | e.g. `0x020e148c`→`0x840430`; `0x020e97dc`→`0x98a0b1`; 62 table+8 reads in sampled traces |
| Guest write **w2 pointer** to geo | **yes** | `0x804008`/`0x800008` ← `0x840430` / `0x98a0b1` (helper stq path); 5 events in focused traces |
| Guest writes table w0/w1/w3 + protocol | **yes** | `oracle_decode_score.json` `geo_contains_w0/w1/w2=true` |
| FIFO protocol family | **yes** | `0x1a003434`, `0x00800101`, `0x03000606`, … |

### 4.2 Absent / not measured

| Signal | Result | Evidence |
| --- | --- | --- |
| Function port `0x00880000` writes | **0** | 117 jsonl / ~1.15M memory events; v0377 probes; all parks |
| Upload/ctl `0x00980000` / `0x00980008` writes in attract/pol_test windows | **0** payload | same traces + `port_absence_v0377.json` |
| Distinctive poly ROM body in **guest** geo/FIFO writes | **0** after source attribution | `oracle_decode_score`: `geo_contains_poly_body_floats=false` for all scored ids |
| Residual w2 index in park geometry/work RAM | **absent** | snap scan: no `0x840430`/`0x98a0b1`/`0x40430`/`0x18a0b1` in long-29 / pol_test / emit31040 / sixth / boot-sel03 / all0 |
| TGP matrix/focus/mode stream | **absent** | v0375–v0377 (not relitigated) |
| Platform render report proving object-ROM fetch | **absent** on native corridor | recovered path never calls `vf2_tgp_execute_geometry_stream` |

### 4.3 False positives (do **not** promote)

| Hit | Why not TGP w2 consumption |
| --- | --- |
| Geo writes class-0x01 (`(w>>23)&0x1f==1`), e.g. `0x00ac1502`, `0x00f8013f`, `0x00800000` | These are **object-table w0-like integers** / register bases whose bits collide with class 0x01 — same false-class family as v0375/0376 protocol filter |
| Work-RAM / FIFO `0x3e4ccccd` / `0xbe4ccccd` (±0.2f) | Common IEEE constants; also appear in many meshes **and** work state |
| `0x3ce075f7` at geo `0x804000` and buffer-ram | Appears **727×** in polygons ROM **and** **6×** in main_data; guest **read source** measured at `0x020096a4`/`0x020096b0` (main_data float table next to `0x00ac1502`), **not** polygons ROM |
| Snap work-RAM ±0.2f at `0x520a50…`, `0x5010a8` | Not sourced from polygons; no w2 correlation |

### 4.4 Oracle comparison already scored

`out/attr-p1/oracle_decode_score.json` `poly_vs_oracle`:

```text
geo_contains_w0/w1/w2 = true
geo_contains_poly_body_floats = false
fifo_contains_poly_body_floats = false
verdict = "geo/FIFO writes are table-record + protocol words, NOT
           polygon-ROM vertex/attr floats"
```

Pol_test 0x97d poly body at index `0x18a0b1` starts
`(-0.2, 0, 0.2, -0.2, 0, -0.2, attr 0xe1001601, …)` — **not** present as a
guest-written sequence in geo/FIFO (only pointer `0x98a0b1` is).

---

## 5. Fail-closed verdict

```text
MEASURED:  i960 guest publishes object-table w2 (polygon word index +
           0x00800000) into geo RAM via helper 0x7c60 / stq path.
ABSENT:    any measured TGP consumption of polygons ROM via that w2
           outside further i960 guest writes, on all parks/traces checked.
BOUNDARY:  recovered runtime does not feed vf2_tgp_execute_geometry_stream
           from native object submits; host dumps stay offline ROM decodes.
```

**Do not** treat:

- w2 presence in geo RAM as TGP execution proof;
- class-0x01 bit collisions on w0 as object commands;
- MAME `geo_object_data` / `polygon_rom_mask` as recovered semantics;
- offline mesh renders as named logo/camera evidence.

---

## 6. Reproduce

```sh
python tools/python/trace_tgp_w2_consumption.py \
  out/attr-long/fifo-phase5.jsonl \
  out/attr-p1/pol_test_g7c60_97d.jsonl \
  out/attr-v0372/pol7c60-id148b.jsonl \
  out/attr-logo/long29-obj.jsonl \
  --snaps out/attr-long/long-29.vf2snap \
          out/attr-p1/pol_test_g7c60_97d.vf2snap \
          out/attr-logo/emit31040.vf2snap \
          out/sixth-fresh.vf2snap \
  --json out/attr-p1/tgp_w2_consumption_v0378.json
```

Prior artifacts (not git): `out/attr-p1/oracle_decode_score.json`,
`out/attr-transform/port_absence_v0377.json`,
`out/attr-transform/measured_view_v0377.json`.

---

## 7. Next measured levers (not implemented here)

1. **Boot / non-attract parks** with `--memory-trace` on `0x880000`/`0x980000`
   — attract may simply never boot TGP program; other modes might.
2. Capture **full geometry ring after pol_test** with probe until a later IP;
   look for **non-guest** writes into geo/buffer (would require an observer
   that sees coprocessor-side stores — our current memory-trace is
   i960-guest-shaped; document if that filter exists).
3. If MAME-style geo handlers appear in a fuller reference dump, use them
   only as **hypothesis generators** for opcode lengths; still prove with
   oracle.
4. Extend `frontier.py` ranking with “w2 published but no consumption
   witness” as an explicit frontier class.
5. Do **not** implement logo/camera recovery from offline mesh PNGs.

---

## 8. Validation observed

- `python -m py_compile tools/python/trace_tgp_w2_consumption.py`: OK.
- Analyzer run on 5 traces + 4 snaps: guest w2 pointer writes present;
  function/upload ports 0; live w2 in snaps false; distinctive geo hit
  attributed to main_data source.
- Broad streaming scan (117 files): `0x880000` / `0x980000` writes = 0.
- No `src/recovered` / executor / CHANGELOG changes in this slice.
- ROM-backed differential: **not applicable** (no new recovery semantics).
- **No git commit.**
