# v0370 — Stores de `0x500030`, tail `0x4d25c` bit9 clear, thunk `0x7fc0`

## 1. Quem grava a phase (`0x00500030`) na ROM maincpu

Varredura `ldib/stib` absolutos (`c0783000`/`c2783000` + word
`0x00500030`): **91** sites. No corredor **selector-3** os relevantes são
o padrão terminal medido:

```text
ldib 0x00500030, r15
lda  1(r15), r15
stib r15, 0x00500030      ; phase = phase + 1
ret
```

| Site | Worker / papel |
| --- | --- |
| `0xac80` | **stib 0** em phase (reset) + flags task + `call 0x43fd0` + **sel++** em `0x50002a` |
| `0xacf8` | wrapper: ldib phase → `0x500031`, mask `0x500034`, `callx 0xaac4[phase]` |
| `0xafc4` / `0xb0cc` | avanço (phase3 family / countdown) |
| `0xb388` / `0xb3ec` / `0xb4e8` / `0xb57c` | fases 4–7 |
| `0xb9ac` / `0xba44` | phase8/9 |
| `0xbc04` / `0xbc58` | phase10/11 |
| `0xbcd8` / `0xbdbc` | phase12/13 |
| **`0xc3bc`** | **phase15** `phase+1` (após mask==0 / clusters) |
| **`0xc43c`** | **phase16** `phase+1` |
| **`0xc44c`** | **phase17** `phase=0` |
| `0xd354`+ | **outros selectors** (não sel3 attract) |

**Não existe** `stib phase+1` dentro do worker phase14 (`0xc0a4`).

## 2. Oracle: spin attract não grava phase

Memory-trace (`tools/python/measure_phase_writes.py`) a partir de
`all0-ready1` (sel=3, phase=0x0e, ready=0, nav=0), IP `0xa6c0`,
**80 000** passos:

| Endereço | Writes observadas |
| --- | --- |
| `0x500030` phase | **0** |
| `0x500031` | 1× `0x0e` (snapshot wrapper) |
| `0x500034` | 1× `0x00400000` (bit 14 = one-hot phase14) |
| `0x500068` | 2× (setbit 16 / limpeza) |
| `0x550000` ready | 0× |

IP final em spin objeto (`0x23f10` / família `0x4c7xx`). **Nenhum**
caminho natural neste park grava `phase+1`.

Thunk phase14 not-ready: **11** passos até `0x9444` (v0369). Corpo
`0x7fc0` (medido/static):

```text
r3=g0 (C-string), r4=g9 (tile dest), r5=1<<15
loop: r6=*r3++; if r6==0 ret; *r4++ = r6 | 0x8000
```

→ glyphs **`0x80xx` ASCII**, não malha 3D. Continuação do stream em
`0x9468` usa `bx` indireto; probe do corpo pode parar em
**unsupported instruction** (fronteira).

## 3. Final-status com board bit9 **limpo** → `0x4d25c`

ROM `0x4d25c` (`texture_status_tail`):

```text
ldob 0x0050002b, r15
if r15==0x0c or r15==0x0d:
    g9 = 0x010040e2 ; balx 0x9444
else:
    g9 = 0x010000e2 ; balx 0x9444
```

Park attract: `0x50002b=0x03` (mesmo valor do sel) → ramo **else**
`0x010000e2`.

| Probe (ctr0=ctr1=ctr2=0, ready=1) | Resultado oracle |
| --- | --- |
| board **bit9 set** (`0x8a00`) | 12 passos `0x4bf90→0x4bfdc`, ready **0** (v0368) |
| board **bit9 clear** | **13** passos até **`0x4d25c`**, ready **0** |
| bit9 clear, until `0x4bfdc` | **170** passos totais, ready **0** |
| body `0x4d25c` +4k (ready forçado 1) | ready **re-armado 1**; IP `0x4ceb8`; phase **0x0e** |

C `execute_texture_final_status_call`: counters 0 + bit9 clear →
`enter_procedure(0x4d25c)`, ready=0. C `execute_texture_status_tail`:
mode `0x0c`/`0x0d` **UNSUPPORTED**; senão thunk `g9=0x010000e2`.
Unit nova: `test_final_status_zero_counter_bit9_clear_calls_tail`.

## 4. Logo 3D

Ainda **fail-closed**. Stores de phase mapeados; spin attract com
ready=0 **não** avança phase; tail de vídeo/thunk é tile/texto
(`0x80xx`/`0x88xx`), não witness de malha SEGA.

## Tools

- `tools/python/measure_phase_writes.py`
- `tools/python/map_phase_stores.py`
- `tools/python/scan_phase_stores.py`

## Validação observada

- Unit nova final-status bit9-clear → exit `0x4d25c`, ready 0
  (`test_final_status_zero_counter_bit9_clear_calls_tail`).
- CTest Debug focado: **15/15 Passed** (orchestrator_*, native_runtime,
  player_270d4, texture_*, first_dispatch, hybrid_first, native_sixth,
  texture_bridge_differential).
- Logo 3D: **não** witness.

## Fronteiras irmãs

- Recuperação status-tail mode `0x0c`/`0x0d` (dest `0x010040e2`).
- Handoff thunk phase14 completo (`0x9444`/`0x9468` stream).
- Quem, no hardware real, grava phase fora destes sites (tarefa
  objeto/TGP) — **não** observado em 80k passos oracle.
- phase15 clusters especiais (v0369 fail-closed).
- `phase17_zero` CC (TEST MENU) — separada.
