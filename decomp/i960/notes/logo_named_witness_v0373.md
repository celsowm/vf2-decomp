# v0373 — Natural attract object-id attribution via `0x7c60` / `0x020e0004`

## Status

**Named title/logo correlation: NOT witnessed.** Fail-closed retained.

This slice *does* measure which guest IPs submit which object-table IDs on the
**natural attract** path, and how that path differs from `display_command_emit`
(`0x31040` / halfword table `0x70cbc`) and TEST.

## 1. Protocol pin (ROM disasm, oracle)

Helper `0x00007c60` (maincpu):

```text
gate:  *(u32*)0x50101c > *(u32*)0x501018 → ret @ 0x7d10
fifo:  lda 0x1a003434 → st (g11)[g12]          @ 0x7c7c / 0x7c84
index: lda 0x020e0004[g0*16], g0               @ 0x7cb8
load:  ldq (g0), r8                            @ 0x7cc0
write: st  r8, 0x10(g10)                       @ 0x7d08   ; geo 0x800010
       stq r8, (g10)[g12]                      @ 0x7d0c   ; FIFO path
ret:   0x7d10
```

Object-table record (main_data LOAD32_WORD, base `0x020e0004`, 16 B/id):

| word | measured role |
| --- | --- |
| w0 | geo/FIFO word0 |
| w1 | secondary field |
| w2 | `bit0x00800000` → polygons.bin word index `(w2 & 0x7fffff)` |
| w3 | size/pair-like |

## 2. Natural attract phase5 — writes attributed to `ip_before`

Source: `out/attr-long/fifo-phase5.jsonl` (memory events correlated by
`step` → `step.ip_before`, AGENTS.md contract).

**Geo/FIFO writes whose value equals a known object-table w0** (fuller 5126-w0
index via `tools/python/attribute_object_submit_ips.py`):

| `ip_before` | insn | n |
| --- | --- | ---: |
| `0x00007d0c` | `stq r8, (g10)[g12]` (helper FIFO) | 220 |
| `0x00007d08` | `st r8, 0x10(g10)` (helper geo) | 110 |
| `0x00019684` | `stq r12, (g10)[g12]` (inlined sibling) | 4 |
| `0x00019680` | `st r12, 0x10(g10)` (inlined sibling) | 2 |

**IDs written (measured, not exhaustive of every helper entry):**

Attract family (v0372 parent set + neighbors): `0x088`, `0x143`, `0x144`,
`0x145`, `0x146`, `0x147`, `0x148`, `0x149`, `0x14a`, `0x14c`, `0x14d`,
`0x151`, `0x152` — also table-read/written in phase5 windows.

Broader helper submissions in the same phase5 trace (each geo+FIFO pair):
`0x4d1–0x4eb` (odd stride), `0x4f0–0x4fe`, `0x503`, `0x750`, `0xf5d`,
`0xd06–0xd11`. High-id family is **numeric only** — still no name binding.

Caveat: FIFO protocol terminator `0xffffffff` can false-match a table w0 if
present; do not treat raw `0xffffffff` writes as object-id evidence.

Matching table w0 examples:

| id | w0 | w2 | poly_off |
| ---: | --- | --- | --- |
| `0x088` | `0x0008e6de` | `0x00813aec` | `0x13aec` |
| `0x148` | `0x000b026a` | `0x00840430` | `0x40430` |
| `0x144` | `0x000afe0e` | `0x0083fe82` | `0x3fe82` |
| `0x145` | `0x000affb2` | `0x008400bd` | `0x400bd` |

Table *reads* inside the helper (`ip_before=0x00007cc0`) also index a broader
id set in phase5 / long-29 natural windows: `0x143/144/145/148`, plus families
`0x4d1–0x4eb`, `0x509–0x538`, `0x749/750`, `0xd04–0xd11`, `0xf5d`.
A second table-read IP `0x0001962c` is an **inlined sibling** submit
(`ldq (g0), r12` / `stq r12, …` then `call 0x7b18`), not `0x7c60`.

## 3. Who loads `g0` — call edges into `0x7c60`

`ip_after == 0x00007c60` in the same phase5 trace (92 edges):

| call site | n | g0 source (ROM disasm) |
| --- | ---: | --- |
| `0x000190f0` | 32 | `ldob g7+4, g1; lda (r3), g0` — object-state pointer |
| `0x00061cc8` | 30 | `ld 4[r10], r3; lda (r3), g0` — object-list walker, stride +24, loop `r9--` |
| `0x00021134` | 26 | `ldos 0x6ee38[r3*2]` or alt `0x6fe38[r3*2]` → `r12`; skip `0xffff`; `lda (r12), g0` |
| `0x000208e4` | 1 | `ldos 0x6eebc[r15*2] → r7; lda (r7), g0` |
| `0x0002edfc` | 1 | `lda (r4), g0` |
| `0x000193c8` | 1 | early display family |
| `0x000202d4` | 1 | `0x20xxx` cluster |

Indexed sibling at `0x0001918c`:
`r4 = *(*(g7+0x190)+0x44)[r8*4]; lda (r4), g0; call 0x7c60`.

**Attract phase5 IDs are not ROM immediates.** Aligned maincpu search finds
no `lda #0x148/#0x144/#0x145/#0x14f/#0x151`. Apparent `0x88` hits at
`0x15d10` etc. are `ldos 0x88(g6), g0` **displacements**, not object-id
constants. Natural attract `g0` values are loaded from **runtime object
records** / halfword tables that feed those records.

Early `0x20xxx` cluster (`fa_pol_test`-family / game_disp) uses other
immediates (`0x12a7`, `0x12a9`, `0x985`, `0x986`, parent-known
`0x3000/0x6a6/…`) — **not** the attract phase5 family.

## 4. `display_command_emit` `0x31040` / table `0x70cbc`

ROM path (`symbols.csv`: `display_command_emit`):

```text
0x31040  ld  0x0050084c, r3
0x3104c  bbs 1, *(r3+0x40), 0x310cc   ; alt → display_transform_update 0x311b8
0x310a8  ldos 0x00070cbc, r10
0x310b0  mov  0, g1
0x310b4  lda  (r10), g0
0x310b8  call 0x00007c60
```

`0x70cbc` halfwords (maincpu dump, first 40):

`0x0ee1, 0x0eed, 0x13fc, 0x13fe, 0x1400, 0x1401, 0x0ee3, … 0x0efd, 0x0eff,
0x13fa, 0x007d, 0x007e, 0x01f9, 0x0205, …`

- `0x0ee1` is a **valid object id** (8192-entry table): w0=`0x0058658c`,
  w2=`0x00a0a2a0`, w3=`0x002b002f`.
- Overlap `0x70cbc` halfwords ∩ attract phase5 ids `{0x88,0x143..0x152}`:
  **empty**.
- Display family w0 values (`0x0058658c`, …) never appear as attract phase5
  geo writes.

### Oracle: forced `--set-ip 0x31040` (`0x500064=5`)

| park | path | measured object id | geo w0 |
| --- | --- | --- | --- |
| `out/attr-long/long-29.vf2snap` | attract | **`0xee1`** @ `0x020eee14` | (trace stop / helper partial) |
| `out/attr-phases/s05-frame.vf2snap` | attract ph5 | **`0xee1`** | call site `0x310b8` |
| `out/sixth-fresh.vf2snap` | **TEST** | **`0xee1`** | **`0x0058658c`** @ `0x800010` and `0x804000` |

So `display_command_emit` submits the **same** `0x70cbc[0]` object id on
attract and TEST when that path is armed. It is **not** the attract phase5
submitter and **not** a title/logo name path.

`0x313d8` (ROM): tests `0x500700 & 0x00171730`; if equal, `setbit 1` on
`*(u32*)(*(u32*)0x50084c + 0x40)` then `ret`. Nearby `0x31408` dispatches
`0x500064`; mode **5** → `call 0x31040`. This arms display emit; it does not
name objects.

Natural park windows at `0xa6c0` / object spin with `nav=0` show **0**
attract-family geo writes and **0** `0x7c60` call edges on TEST
(`fifo-test.jsonl`: 0 edges). Attract natural long-29 *does* table-read
`0x143/144/145/148` (and other families) inside `0x7cc0`.

## 5. Other ROM halfword object-id tables (dumped, not named)

| maincpu addr | first entries | measured consumer |
| --- | --- | --- |
| `0x70cbc` | `0xee1,0xeed,0x13fc,0x13fe,0x1400,0x1401,…` | `display_command_emit` `0x310a8` |
| `0x6ee38` | `0x48b,0x48d,0x48f,0x491,…` | `0x21104` walker → `0x21134` |
| `0x6fe38` | `0xa38,0xa3a,0xa3c,…` | `0x21118` alt when `g7+4` bit6 and `0x5000df` bit0 |
| `0x6eebc` | `0x753,0x754,0x755,0x756,0x13f4,0x32c,0xffff…` | `0x208b4` |
| `0x6ee98` | `0x752,0x255,0xffff…` | `0x20xxx` |

None of these tables **name** objects. They are numeric id lists. None contain
the attract phase5 family `0x088/0x143–0x152` as their primary first entries.

## 6. Title/logo / string binding — negative

| source | SEGA / LOGO / TITLE / VIRTUA |
| --- | --- |
| maincpu | only `SEGA ENTERPRISES,LTD.` @ **`0x0000aaad`** (legal warning; 2D; v0360) |
| main_data (`out/main_data.bin`) | **none** |
| polygons.bin | **none** (v0372) |
| ASCII in `0x020e0000–0x02120000` (object-table region) | **none** |
| fighter/data strings | `VF2`, `VF2_YA`, `DRAGON_SU_NA/YA`, `JACKY_*`, `R_RUF_*` @ `0x021980e8`, `0x0219842a`, `0x02ff371b` — **data names, not logo** |
| maincpu/main_data pointers to those name guests | **0 hits** |
| attract w0 refs (`0x8e6de`, `0xb026a`, …) | only their own table slots (`0x020e0884`, `0x020e1484`, …); nearest fighter-name file offset is **~748 700 bytes away** |
| `symbols.csv` / `functions.csv` / recovered C | **no** object-id → title/logo symbol |

**No measured path links an attract-submitted id (`0x88/0x14x/…`) to a named
title, logo, or fighter string.**

## 7. Fail-closed

- Do **not** name ids `0x088/0x140–0x157` as logo/title meshes.
- Do **not** treat display-table ids (`0x0ee1` family / `0x70cbc`) as attract
  title objects.
- Do **not** treat FIFO immediates (`0x1a003434`, `0x14802929`, …) as object
  ids.
- Do **not** treat fighter ASCII (`VF2_YA`, `DRAGON_SU_*`) as 3D logo labels
  for these object ids without a measured pointer/link.
- Legal SEGA warning remains the only named SEGA visual (2D text).
- **Logo 3D recovery stays fail-closed** until a witness binds an id or poly
  offset to a title/logo symbol, or an oracle path draws unique title glyphs
  correlated to a measured object id.

## 8. Parks / commands (analysis only)

```text
parks:
  out/attr-long/long-29.vf2snap
  out/attr-long/fifo-phase5.jsonl
  out/attr-long/fifo-attract.jsonl
  out/attr-long/fifo-test.jsonl
  out/attr-phases/s05-frame.vf2snap
  out/attr-fs/all0-ready1.vf2snap
  out/sixth-fresh.vf2snap

object-submit pin (v0372 recipe still valid):
  vf2probe --rom-dir roms/vf2 --snapshot out/attr-long/long-29.vf2snap \
    --set-ip 0x7c60 --until 0x7d14 \
    --set-reg g0=0x148 --set-reg g11=0x884000 \
    --set-u32 0x501018=0x1000 --set-u32 0x50101c=0 --memory-trace

display emit (measured id 0xee1):
  vf2probe --rom-dir roms/vf2 --snapshot out/sixth-fresh.vf2snap \
    --set-ip 0x31040 --set-u32 0x00500064=5 --until 0x311b8 \
    --max-steps 20000 --trace --memory-trace
```

Scratch analyzers (not git evidence): `out/attr-v0373/attribute_*.py`.
Compact JSON reports: `out/attr-v0373/attribution_report.json`,
`part2_summary.json`, `part3_report.json`.

## 9. Validation

- **No** recovered C / executor / oracle semantics change this slice
  (tools + notes only).
- Unit gate observed: `build/Debug/vf2_tests.exe` → `All vf2-decomp tests
  passed.`
- ROM-backed differential **not** re-run (no C delta; prior gates remain last
  observed).
- Parks/traces under `out/attr-v0373/` — **not for git**.

## 10. Next levers (measured)

1. Dump runtime object records at the `0x61cc8` walker base (`r10` list) on a
   phase5 park — still numeric ids, but shows attract object-instance source.
2. Taint/edge around `0x190f0` / `0x61cc8` g0 loads (does any bit of the
   record depend on a display/title flag?).
3. Keep searching for a **name table** that indexes object ids (none in
   maincpu/main_data/polygons today).
4. Do not implement logo C without a named witness.
