# v0375 — `render_mesh_host.py` (análise host de malhas polygon-ROM)

> **Host analysis only.** Não é recovery C, não é witness de logo/title,
> não é raster do jogo. Fórmulas de câmera/projeção são conveniência do
> analista. Geometria de decode espelha `src/hardware/tgp.c`
> (`geometry_execute_object` / `geometry_transform_point`) e nada além disso.

## 1. O que a tool faz

`tools/python/render_mesh_host.py` (novo) decodifica objetos da tabela
`0x020E0004` (main_data LOAD32_WORD, record 16 bytes, `word_index = w2 & 0x7fffff`,
poly byte = `word_index*4`) e rasteriza PNGs multi-view com z-buffer,
Lambert fixo `(0.3,0.5,1.0)` e overlay de wireframe 1px.

- Importa ROM build + constantes de `render_poly_objects.py` (legacy intacto).
- Decode dual: `skip3` = `geometry_mode&3 < 2` (skip 3 words após attr);
  `noskip` = skip 0. Update p0/p1 segue `tgp.c` casos 0/2/1/3
  (modo 0/2: `p1 = p3` se quad, senão `p2` — corrige viés do legacy).
- Views CLI: `fit-ortho`, `front`, `top`, `side`, `iso-xy`, `iso-xz`, `iso-yz`,
  `tgp` (matrix+focus via `--matrix`/`--transforms`; fallback identity rotulado),
  `persp` (host `x'=f*x/z_safe` olhando +Z — ANALYSIS, documentado no docstring).
- Saída: `out/attr-render/` (gitignored). PIL disponível (`PIL 11.1.0`) → PNG.
- Report compacto: `out/attr-render/render_report.json`.

Comando executado (ROM `roms/vf2`):

```powershell
& $env:MIMO_PYTHON tools\python\render_mesh_host.py `
  --id 0x88,0x140,0x144,0x148,0x149,0x14f,0x150,0x157,0x1cb,0x08f,0x180,0x97d,0x985,0x0ee1 `
  --all-large 25 --out out/attr-render --size 512 --cell 96 `
  --decode both `
  --views fit-ortho,front,top,side,iso-xy,iso-xz,iso-yz
```

## 2. Contagens medidas (tris)

Tabela de objetos presente para **todos** os ids pedidos (poly bit `0x00800000` set).

| id | w0 | w2 | word | byte | tris skip3 | tris noskip | nonfinite |
| ---: | --- | --- | --- | --- | ---: | ---: | --- |
| 0x088 | 0x0008e6de | 0x00813aec | 0x13aec | 0x4ebb0 | 4 | 8 | 0 |
| 0x140 | 0x000af42a | 0x0083f17c | 0x3f17c | 0xfc5f0 | 19 | 2 | 0 |
| 0x144 | 0x000afe0e | 0x0083fe82 | 0x3fe82 | 0xffa08 | 8 | 9 | 0 |
| 0x148 | 0x000b026a | 0x00840430 | 0x40430 | 0x1010c0 | 10 | 41 | 0 |
| 0x149 | 0x000b0cca | 0x00841219 | 0x41219 | 0x104864 | 6 | 3 | 0 |
| 0x14f | 0x000b152c | 0x00841d4b | 0x41d4b | 0x10752c | 49 | 2 | 0 |
| 0x150 | 0x000b19b4 | 0x00842332 | 0x42332 | 0x108cc8 | 49 | 6 | 0 |
| 0x157 | 0x000b2524 | 0x0084322f | 0x4322f | 0x10c8bc | 6 | 4 | 0 |
| 0x1cb | 0x000be1f6 | 0x0085284d | 0x5284d | 0x14a134 | **384** | 2 | 0 |
| 0x08f | 0x0008f5ee | 0x00814e8f | 0x14e8f | 0x53a3c | **257** | 7 | 0 |
| 0x180 | 0x000b6768 | 0x00848992 | 0x48992 | 0x122648 | **233** | 6 | 0 |
| 0x97d | 0x004a7606 | 0x0098a0b1 | 0x18a0b1 | 0x6282c4 | 2 | 4 | 0 |
| 0x985 | 0x004a7d26 | 0x0098a9c9 | 0x18a9c9 | 0x62a724 | 2 | 4 | 0 |
| 0x0ee1 | 0x0058658c | 0x00a0a2a0 | 0x20a2a0 | 0x828a80 | 5 | 2 | 0 |

Bate com notas v0372 para skip3: `0x1cb=384`, `0x08f=257`, `0x180=233`.
`nonfinite_rejected=0` em todos os ids medidos.

### Top 25 `--all-large` (scan id `0..0x2000`, rank por tris skip3)

| rank | id | tris skip3 | word | w2 |
| ---: | ---: | ---: | --- | --- |
| 1 | 0x5c7 | 515 | 0x1189f5 | 0x009189f5 |
| 2 | 0x1cb | 384 | 0x5284d | 0x0085284d |
| 3 | 0x33e | 384 | 0x8ff4d | 0x0088ff4d |
| 4 | 0x341 | 298 | 0x90f68 | 0x00890f68 |
| 5 | 0x32a | 287 | 0x8c89d | 0x0088c89d |
| 6–8 | 0x285 / 0x286 / 0x33f | 280 | distintos | w3 comum `0x008c008d` |
| 9 | 0x9e7 | 261 | 0x19258d | 0x0099258d |
| 10–11 | 0x08f / 0x903 | 257 | distintos | w3 comum `0x007600bb` |
| 12–14 | 0x53d / 0x090 / 0x904 | 253–251 | distintos | mesma família w3 |
| 15 | 0x180 | 233 | 0x48992 | 0x00848992 |
| 16–18 | 0x288 / 0xacd / 0xacc | 224–219 | distintos | cluster `0x006c006x` |
| 19 | 0x1308 | 205 | 0x29a890 | 0x00a9a890 |
| 20–25 | 0x051,0xad1,0x071,0xad0,0x018,0x2e0 | 190–170 | — | — |

Famílias w3 repetidas (evidência de malhas parentes, **não** de nome):
`0x007600bb` → `0x08f/0x903/0x090/0x904/0x53d`;
`0x00c000c1` → `0x1cb/0x33e`;
`0x008c008d` → `0x285/0x286/0x33f`.

## 3. Diferença de decode

- Para a **maioria** dos objetos poly-ROM densos, `skip3` é o mesh coerente
  (dezenas–centenas de tris). `noskip` frequentemente colapsa a **2 tris**
  (terminação/alinhamento errado na stream de floats).
- Exceções onde `noskip` tem **mais** tris que `skip3` (primitivos pequenos /
  headers): `0x148` (41>10), `0x088` (8>4), `0x97d/0x985` (4>2), `0x144` (9>8).
- Contatos visuais: várias malhas grandes skip3 são **quase planares** em XY
  (`fit-ortho`/`front` quase linhas) e só legíveis em `top`/`side`/`iso-*`.
  Isso é observação de análise, não semântica de câmera do jogo.

## 4. Arquivos (não commitar PNG/ROM)

- Tool: `tools/python/render_mesh_host.py`
- Legacy preservado: `tools/python/render_poly_objects.py`
- Report: `out/attr-render/render_report.json`
- Contact sheet: `out/attr-render/contact_sheet.png`
- Full-size por id/view/decode: `out/attr-render/id_<hex>_<decode>_<view>_512.png`

Exemplos de melhor view (skip3, 512px) após score corrigido:

| id | best view | arquivo |
| ---: | --- | --- |
| 0x5c7 | iso-xz | `out/attr-render/id_5c7_skip3_iso-xz_512.png` |
| 0x1cb | top | `out/attr-render/id_1cb_skip3_top_512.png` |
| 0x1cb | iso-xy (também gerado) | `out/attr-render/id_1cb_skip3_iso-xy_512.png` |
| 0x08f | iso-yz | `out/attr-render/id_08f_skip3_iso-yz_512.png` |
| 0x148 | front | `out/attr-render/id_148_skip3_front_512.png` |
| 0x180 | iso-xz | `out/attr-render/id_180_skip3_iso-xz_512.png` |

## 5. Fail-closed / limites

- **Não** nomear meshes `0x88/0x140–0x157/0x1cb/...` como logo/title/SEGA.
- **Não** tratar PNGs host como witness visual nomeado.
- `--view tgp` sem matrix medida = fallback identity **rotulado**; Phase B
  pode alimentar `--transforms` JSON quando houver matrix/focus medidos.
- `--view persp` e as câmeras iso/front/top/side são **ANALYSIS**, não recovery.
- Saídas em `out/attr-render/` e qualquer dump ROM derivado **não vão para git**.
- Sem mudança em `src/recovered`, `tgp.c`, `CHANGELOG.md`.
- Sem `git commit` nesta fatia (parent commita tool + nota).

## 6. Validação observada

- Tool executou com ROM legal local; PIL 11.1.0; 36 objetos no report.
- `render_poly_objects.py` ainda importa (legacy intacto).
- Contagens skip3 de `0x1cb/0x08f/0x180` coerentes com v0372.
- CTest/recovery C: **não aplicável** nesta fatia (tools+nota apenas).
