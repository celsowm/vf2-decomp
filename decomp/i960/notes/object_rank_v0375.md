# object_rank v0375 — ranking host de meshes polygon (Phase C)

## 1. Objetivo e fail-closed

Rankear entradas da tabela de objetos (`0x020e0004`, 16 B/record) por
utilidade para **inspeção visual**, com métricas medidas de decode host.

- **Não** é recovery de semântica de jogo.
- **Não** é witness nomeado de logo/title/SEGA — meshes permanecem **sem nome**.
- Projeção host é ortográfica simples; matrix/câmera do jogo não aplicadas
  (exceto quando `render_mesh_host.py` recebe matrix medida via CLI).

## 2. Metodologia

1. Reconstruir `main_img` + `poly` a partir de `MAIN_PAIRS`/`POLY_PAIRS`
   (`tools/python/render_poly_objects.py`), ROM dir `roms/vf2`.
2. Varredura ids `0x000–0xFFF` onde `w2` tem bit polygon-ROM `0x00800000`
   ou índice poly != 0.
3. Decode float-link em dois modos:
   - **skip3**: alinhado a `src/hardware/tgp.c` `geometry_execute_object`
     (quando `geometry_mode & 3 < 2` descarta 3 words antes de p2) —
     **caminho primário de score** para polygon-ROM;
   - **noskip**: hipótese `geometry_mode & 3 >= 2` (diagnóstico).
4. Métricas por id: `tris_skip`, `tris_noskip`, `tris_primary`,
   `mode_class`, `stability = 1 - |ta-tb|/max`, bounds/extent,
   `finite_ratio` (stream), área (soma ½|e1×e2|), pack `w3`.
5. `mode_class` (host diagnostic):
   - `skip3_dominant` — skip3 denso, noskip colapsa (comum em poly-ROM);
   - `noskip_dominant` — o oposto;
   - `both_agree` — contagens altas e próximas;
   - `both_dense_disagree` — ambos densos mas divergem.
6. Score: prioriza **tris_primary (skip3)** alto; estabilidade é bônus
   *soft* (não zera malhas densas skip3); exige `finite_ratio` e extent
   utilizáveis; `w3!=0` e bit ROM somam; bônus se id em família medida.
7. Degenerados (0–2 tris) → **calib**, score 0 — não são arte.
8. Top art: `render_mesh_host.py --id …` quando presente; strips
   multi-view embutidos (iso|front|top) em `out/attr-render/rank/`.

## 3. Contagens medidas

- scanned_with_poly: **4096**
- art_candidate(+extent_flagged): **3451**
- calib (<=2 tris): **597**
- flagged (attract/pol_test/display_70cbc): **68**
- skip3_dominant com tris>=20: **1215**
- both_agree com tris>=8: **27**
- both_dense_disagree: **43**
- Pillow PNG: **True**

## 4. Top art (por score)

| id | score | tris s/n | mode | stab | extent | w3 | flags | category |
| ---: | ---: | ---: | --- | ---: | ---: | ---: | --- | --- |
| `0x5c7` | 0.837 | 515/3 | skip3_dominant | 0.01 | 48.43 | `0x02140294` | — | art_candidate |
| `0x33e` | 0.732 | 384/2 | skip3_dominant | 0.01 | 11.95 | `0x00c000c1` | — | art_candidate |
| `0x1cb` | 0.699 | 384/2 | skip3_dominant | 0.01 | 1.195 | `0x00c000c1` | — | art_candidate |
| `0x341` | 0.674 | 298/2 | skip3_dominant | 0.01 | 16 | `0x00940096` | — | art_candidate |
| `0x32a` | 0.666 | 287/2 | skip3_dominant | 0.01 | 32.41 | `0x036303b6` | — | art_candidate |
| `0x285` | 0.654 | 280/2 | skip3_dominant | 0.01 | 12 | `0x008c008d` | — | art_candidate |
| `0x286` | 0.654 | 280/2 | skip3_dominant | 0.01 | 12 | `0x008c008d` | — | art_candidate |
| `0x33f` | 0.654 | 280/2 | skip3_dominant | 0.01 | 12 | `0x008c008d` | — | art_candidate |
| `0x9e7` | 0.647 | 261/2 | skip3_dominant | 0.01 | 38.64 | `0x00e4011c` | — | art_candidate |
| `0x180` | 0.627 | 233/6 | skip3_dominant | 0.03 | 202.8 | `0x013f0161` | — | art_candidate |
| `0xacd` | 0.618 | 221/6 | skip3_dominant | 0.03 | 97 | `0x006c006f` | — | art_candidate |
| `0xacc` | 0.617 | 219/6 | skip3_dominant | 0.03 | 104.9 | `0x006c006e` | — | art_candidate |
| `0x288` | 0.610 | 224/2 | skip3_dominant | 0.01 | 12 | `0x006c0071` | — | art_candidate |
| `0x08f` | 0.606 | 257/7 | skip3_dominant | 0.03 | 0.9285 | `0x007600bb` | — | art_candidate |
| `0x903` | 0.606 | 257/7 | skip3_dominant | 0.03 | 0.9285 | `0x007600bb` | — | art_candidate |
| `0x090` | 0.599 | 251/2 | skip3_dominant | 0.01 | 0.9285 | `0x007600bb` | — | art_candidate |

## 5. Famílias medidas (ids, não semântica)

- `attract_family`: `0x88`, `0x140–0x157` (phase5 object-table reads)
- `pol_test`: `0x97d–0x986` (primitives pequenos; calib)
- `display_70cbc`: `0xee1` e vizinhos/halfwords medidos (`0x0eed`, `0x13fc`…)

## 6. Estabilidade de decode (mode_class)

- mode_class `skip3_dominant`: **1765**
- mode_class `partial_agree`: **1458**
- mode_class `calib_tiny`: **597**
- mode_class `both_agree`: **220**
- mode_class `both_dense_disagree`: **43**
- mode_class `noskip_dominant`: **13**

### Densas sob skip3 (caminho tgp.c) — candidatas a inspeção

- `0x5c7` skip3=515 noskip=3 ext=48.43 score=0.837 flags=[]
- `0x1cb` skip3=384 noskip=2 ext=1.195 score=0.699 flags=[]
- `0x33e` skip3=384 noskip=2 ext=11.95 score=0.732 flags=[]
- `0x341` skip3=298 noskip=2 ext=16 score=0.674 flags=[]
- `0x32a` skip3=287 noskip=2 ext=32.41 score=0.666 flags=[]
- `0x285` skip3=280 noskip=2 ext=12 score=0.654 flags=[]
- `0x286` skip3=280 noskip=2 ext=12 score=0.654 flags=[]
- `0x33f` skip3=280 noskip=2 ext=12 score=0.654 flags=[]
- `0x9e7` skip3=261 noskip=2 ext=38.64 score=0.647 flags=[]
- `0x08f` skip3=257 noskip=7 ext=0.9285 score=0.606 flags=[]
- `0x903` skip3=257 noskip=7 ext=0.9285 score=0.606 flags=[]
- `0x090` skip3=251 noskip=2 ext=0.9285 score=0.599 flags=[]

### Ambos os modos densos mas divergem (não promover sem novo evidence)

- `0x98a` skip3=120 noskip=13 stab=0.11
- `0x316` skip3=110 noskip=19 stab=0.17
- `0x087` skip3=98 noskip=22 stab=0.22
- `0x089` skip3=85 noskip=15 stab=0.18
- `0x08a` skip3=67 noskip=16 stab=0.24
- `0x04c` skip3=57 noskip=12 stab=0.21
- `0x8c3` skip3=57 noskip=12 stab=0.21
- `0x1d3` skip3=55 noskip=13 stab=0.24

## 7. Top findings (não-nomeados)

- `0x5c7`: score=0.837; alta densidade (515 tris); skip3-dominante (515/3; caminho tgp.c); coords finitas; extent=48.43 (utilizável); w3=0x02140294; polygon-ROM
- `0x33e`: score=0.732; alta densidade (384 tris); skip3-dominante (384/2; caminho tgp.c); coords finitas; extent=11.95 (utilizável); w3=0x00c000c1; polygon-ROM
- `0x1cb`: score=0.699; alta densidade (384 tris); skip3-dominante (384/2; caminho tgp.c); coords finitas; extent=1.195 (utilizável); w3=0x00c000c1; polygon-ROM
- `0x341`: score=0.674; alta densidade (298 tris); skip3-dominante (298/2; caminho tgp.c); coords finitas; extent=16 (utilizável); w3=0x00940096; polygon-ROM
- `0x32a`: score=0.666; alta densidade (287 tris); skip3-dominante (287/2; caminho tgp.c); coords finitas; extent=32.41 (utilizável); w3=0x036303b6; polygon-ROM
- `0x285`: score=0.654; alta densidade (280 tris); skip3-dominante (280/2; caminho tgp.c); coords finitas; extent=12 (utilizável); w3=0x008c008d; polygon-ROM
- `0x286`: score=0.654; alta densidade (280 tris); skip3-dominante (280/2; caminho tgp.c); coords finitas; extent=12 (utilizável); w3=0x008c008d; polygon-ROM
- `0x33f`: score=0.654; alta densidade (280 tris); skip3-dominante (280/2; caminho tgp.c); coords finitas; extent=12 (utilizável); w3=0x008c008d; polygon-ROM

## 8. Calib / prims (não-arte)

- `0x000` tris=2/2 w3=0x00010002 flags=[]
- `0x004` tris=2/2 w3=0x00010002 flags=[]
- `0x006` tris=2/2 w3=0x00010002 flags=[]
- `0x008` tris=1/1 w3=0x000c000d flags=[]
- `0x00e` tris=1/1 w3=0x00280029 flags=[]
- `0x016` tris=1/1 w3=0x01230135 flags=[]
- `0x026` tris=1/1 w3=0x002b002c flags=[]
- `0x02b` tris=1/1 w3=0x0038003a flags=[]
- `0x031` tris=2/2 w3=0x00330039 flags=[]
- `0x032` tris=2/2 w3=0x00010002 flags=[]

## 9. Renders produzidos

- `0x5c7` tool=render_mesh_host tris=515 flags=[] best=skip3 -> `out/attr-render/rank/id_5c7_skip3_iso-xy_384.png`
- `0x33e` tool=render_mesh_host tris=384 flags=[] best=skip3 -> `out/attr-render/rank/id_33e_skip3_iso-xy_384.png`
- `0x1cb` tool=render_mesh_host tris=384 flags=[] best=skip3 -> `out/attr-render/rank/id_1cb_skip3_iso-xy_384.png`
- `0x341` tool=render_mesh_host tris=298 flags=[] best=skip3 -> `out/attr-render/rank/id_341_skip3_iso-xy_384.png`
- `0x32a` tool=render_mesh_host tris=287 flags=[] best=skip3 -> `out/attr-render/rank/id_32a_skip3_iso-xy_384.png`
- `0x285` tool=render_mesh_host tris=280 flags=[] best=skip3 -> `out/attr-render/rank/id_285_skip3_iso-xy_384.png`
- `0x286` tool=render_mesh_host tris=280 flags=[] best=skip3 -> `out/attr-render/rank/id_286_skip3_iso-xy_384.png`
- `0x33f` tool=render_mesh_host tris=280 flags=[] best=skip3 -> `out/attr-render/rank/id_33f_skip3_iso-xy_384.png`
- `0x9e7` tool=render_mesh_host tris=261 flags=[] best=skip3 -> `out/attr-render/rank/id_9e7_skip3_iso-xy_384.png`
- `0x180` tool=render_mesh_host tris=233 flags=[] best=skip3 -> `out/attr-render/rank/id_180_skip3_iso-xy_384.png`
- `0xacd` tool=render_mesh_host tris=221 flags=[] best=skip3 -> `out/attr-render/rank/id_acd_skip3_iso-xy_384.png`
- `0xacc` tool=render_mesh_host tris=219 flags=[] best=skip3 -> `out/attr-render/rank/id_acc_skip3_iso-xy_384.png`
- `0x088` tool=rank_builtin tris=4 flags=['attract_family'] best= -> `—`
- `0x148` tool=rank_builtin tris=10 flags=['attract_family'] best= -> `—`
- `0x97d` tool=rank_builtin tris=4 flags=['pol_test'] best= -> `—`
- `0xee1` tool=rank_builtin tris=5 flags=['display_70cbc'] best= -> `—`

## 10. Artefatos

- `out\attr-render\object_rank.json`
- `out\attr-render\object_rank.csv`
- `out\attr-render\rank_top24.md`
- `out/attr-render/rank/` (PNG/PPM multi-view + render_mesh_host)
- `out/attr-render/rank_manifest.json`

## 11. Fronteira explícita

- Rankings e PNGs são **análise host** de offsets medidos — não prova
  que um id é um mesh de título/logo.
- Malhas **sem nome**. `display_70cbc` / `attract_family` são rótulos de
  **família de id medida**, não semântica de mesh.
- `skip3_dominant` indica que o pacote se decodifica melhor no caminho
  `tgp.c` (mode&3<2); **não** implica que noskip esteja errado no jogo
  (geometry_mode pode variar por cena).
- Sem commit; sem escrita em `src/` ou `CHANGELOG.md`.

## Tools

- `tools/python/rank_poly_objects.py`
- `tools/python/render_poly_objects.py` (pairs + decode base)
- `tools/python/render_mesh_host.py` (multi-view PNG host; paralelo)
- `tools/python/scan_render_all_objects.py` (noskip reference)

