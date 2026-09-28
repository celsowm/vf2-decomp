# v0369 — Cauda sel3 no oracle + fail-closed phase14/15 medidos

## Resultado principal

Logo 3D **permanece fail-closed**. A cauda selector-3 foi medida no
oráculo a partir do park attract phase14 (`all0-ready1`, ready pinado
0 via `0x4bf90`).

### Phase14 not-ready (ROM `0xc0a4`)

| Estado | Resultado medido |
| --- | --- |
| ctr `[task+0x50]=0`, `0x550000!=1` | **11** instruções até **`0x00009444`** |
| thunk body +2000 passos | writes `0x1000ef4`, `0x10b800xx` (tile stream); phase **0x0e** |
| resume 400k com ready=0, nav=0 | phase **permanece 0x0e**; IP spin objeto `0x4c7xx`; nz_tex 10154 |

Worker ROM **não** grava `0x500030=phase+1` neste caminho.

Semântica `cmpdeco` medida: contador **vivo == 0** → branch ao ready
**sem** store (thunk se ready≠1); contador ≠0 → store `ctr-1` e ret.
C phase14 espelha isso e retorna **`VF2_ERROR_UNSUPPORTED`** apenas em
ctr==0 e ready≠1. Caminho ready==1 continua wait-ret.

### Workers 15–17 (forçados, mesmos gates medidos)

Park: `out/attr-fs/all0-ready1.vf2snap` (sel=3, phase14, ready=0).

| Caso probe | ph final | Efeito medido |
| --- | ---: | --- |
| worker `0xc268`, mask u16=1 | **0x10** | mask `0x00030000`, ctr+50=**128**, dtex=60 |
| worker `0xc414`, ctr=1 | **0x11** | dtex=38 |
| worker `0xc448` | **0x00** | wrap phase17→0 |
| flags tasks `0x530000`/`0x531000` | — | `0x84000000/02` — **bit5 clear** (gate phase15) |

Resume 400k a partir de phase15/17 continua em spin `0x4c7xx`; tiles
sem ASCII; geom snapshot 32k **não** muda (dtex≈295 no RAM de textura).

### Phase15 masks especiais (ROM `0xc268`)

ROM compara o u16 em `0x500028` com:

| Valor | Origem ROM | Cluster |
| --- | --- | --- |
| **0x700** | `7<<8` | 5× `call 0x8f1c` descriptors + `0x54318` |
| **0x540** | `21<<6` | `0x500094=1`, `call 0x54318` |
| **0x380** | `7<<7` | `0x500094=2`, `call 0x54318` |
| **0x1c0** | `7<<6` | `0x8ef0` fill + leituras main_data `0x023xxxxx` |

Descriptors main_data (`LOAD32_WORD`, base `0x02000000`):

| Addr | mode | rows×cols | payload (halfwords) |
| --- | ---: | --- | --- |
| `0x02a69cd2` | 1 | 7×26 | `083b,083c,…` (addend `0x8000` → blit **0x88xx**) |
| `0x02a69e4a` | 1 | 3×7 | `0008…2d08-3108` |
| `0x02a69ee6` | 1 | 3×15 | `0008` (vazio) |
| `0x02a69f4c` | 1 | 3×7 | `0008…1f08-2308` |
| `0x02a69f02` | — | glyph table legal | halfwords `08xx/08ex` (helper `0x8918`, não `0x8f1c`) |

Oracle mask **0x700**: ~**2739** tile writes; amostra em `0x01000124`
=`3b88,3c88,…` (LE `0x883b…`) — **glyph bank 0x88xx** decodificado:
`;<==>?@@@@@@AABCCDEFGHIJKL MNOPQRSTUVWXYZ…` (índices sequenciais,
**não** string SEGA nomeada). mask 0x380/0x540 limpam `0x01000700`
com `0x2000` (space) e set `0x500094`; `0x54318` (board60 bit28=0)
refaz fill. mask **0x1c0** lê `0x0230xxxx` (main_data alta).

C phase15 **não** implementa esses clusters. Fail-closed: se u16 mask
∈ {0x700,0x540,0x380,0x1c0} → `VF2_ERROR_UNSUPPORTED` sem decrementar.
Path mask=1 (avanço 15→16) permanece recuperado/pinado.

### TGP / geometria (diagnóstico resume-trace)

Durante parks attract phase14+:

- Geometry buffer (modelo): nonzero_words **106822**
- Copro port: **3797** words
- Geometry RAM `0x00800000`: **4137** words

Snapshot “geometry” 32k é janela de registradores — **não** é o stream
completo. Sem packet→mesh nomeada → **logo 3D fail-closed**.

## Tools

- `tools/python/force_attract_tail.py`
- `tools/python/measure_phase14_thunk.py`
- `tools/python/force_phase15_masks.py`
- `tools/python/dump_phase15_descriptors.py` (main_data LOAD32_WORD)
- `tools/python/dump_tile_glyphs.py` (banks 0x80/0x88/0x89)

## Validação (observada)

- Fail-closed C: `execute_selector3_phase14` not-ready (ctr==0, ready≠1);
  `execute_selector3_phase15` special masks `{0x700,0x540,0x380,0x1c0}`.
- Units em `tests/analysis/test_orchestrator_bridge.c` (UNSUPPORTED +
  mask intacto).
- CTest Debug focado desta fatia:
  - `vf2_orchestrator_bridge` **Passed**
  - `native_runtime`, `native_sixth`, `player_270d4`, `first_dispatch`,
    texture/* **Passed** (15/15 no subconjunto attract/texture/native)
  - `vf2_phase17_zero_differential` **Failed** (189 casos, `cpu-state`
    offset=1) — fronteira **separada** já documentada (CC phase17_zero
    em `cobr_cc_270d4_v0359` / `player_270d4_slot_pin_v0358`); o teste
    **não** referencia workers sel3 phase14/15.
- Logo 3D: **não** witness.

## Fronteiras irmãs (não misturar)

- Recuperação completa dos blits phase15 (descriptors + `0x8f1c`/`0x54318`).
- Thunk phase14 `0x9444` + handoff nativo.
- Quem grava `0x500030` no spin objeto `0x4c7xx` (avanço natural pós-14).
- Correlação FIFO→polygons/mesh nomeada.
- `phase17_zero` CC (TEST MENU) — não é sel3 phase17.
