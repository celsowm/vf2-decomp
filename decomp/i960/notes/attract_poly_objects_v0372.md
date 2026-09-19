# v0372 — Objetos polygon no attract (tabela `0x020e0004`) + render host

## 1. Breakthrough medido (oracle)

Memory-trace do attract **phase5** (`out/attr-long/fifo-phase5.jsonl`):

| Evidência | Valor |
| --- | --- |
| Leituras da tabela de objetos | `0x020e1484`… → **id 0x148**; `0x020e0884` → **id 0x088**; `0x020e1454` → **id 0x145** |
| Writes geo/FIFO com word0 da tabela | `0x800010`/`0x804000` = **`0x000b026a`** (id **0x148**), `0x0008e6de` (id **0x088**), `0x000afe0e` (0x144), `0x000affb2` (0x145), `0x000b0cca` (0x149), `0x000b0e66` (0x14a), `0x000b1134` (0x14c), `0x000b12c0` (0x14d), `0x000b1e3c` (0x151), `0x000b1ff0` (0x152)… |
| FIFO command words (família pol_test) | `0x800101`, `0x1800303`, `0x3000606`, `0x1a003434` — **idênticos** aos do `fa_pol_test` `0x21a00` |

Helper **`0x7c60`** (medido no oráculo com `fa_pol_test`):

- escreve `0x1a003434` no FIFO `(g11)[g12]` (`g11` = copro FIFO `0x884000`);
- lê a tabela `0x020e0004[g0*16]` (4 words);
- envia word0/w1/w2 para geo RAM `0x800010` / `0x804000+`;
- IDs do pol_test: `0x97d–0x986` (primitives **pequenos**, w3=`0x00010002`).

## 2. Tabela de objetos (main_data, LOAD32_WORD)

Base `0x020e0004`, 16 bytes/objeto:

```text
w0  ponteiro/RAM-like     w1  ponteiro
w2  endereço TGP          w3  dimensões (par)
```

- `w2 & 0x00800000 != 0` → **polygon ROM** (coerente com `src/hardware/tgp.c:892-896`);
- `vf2_tgp_read_polygon_word` indexa em **words**: `byte = (w2 & 0x7fffff) * 4`.

Exemplos (attract phase5):

| id | w0 | w2 | poly word | w3 |
| ---: | ---: | ---: | ---: | ---: |
| **0x148** | `0x000b026a` | `0x00840430` | `0x40430` | **`0x014a0164`** (maior da família) |
| 0x088 | `0x0008e6de` | `0x00813aec` | `0x13aec` | `0x00fd0112` |
| 0x144 | `0x000afe0e` | `0x0083fe82` | `0x3fe82` | `0x00340039` |
| 0x97d (pol_test) | `0x004a7606` | `0x0098a0b1` | `0x18a0b1` | `0x00010002` |

## 3. ROM ASCII

Busca em main_data + polygons.bin: **sem** `SEGA`/`LOGO`/`TITLE`.  
Presentes: `VF2`, `VF2_YA`, `DRAGON_SU_NA`, `R_RUF_051` (strings de lutadores/dados — não prova logo).

## 4. Render host (análise, não recovery)

Tools: `tools/python/render_poly_objects.py`,
`scan_render_all_objects.py` (decode alinhado a `tgp.c`
`geometry_execute_object` com word_index; projeção ortográfica simples).

- Malhas com **dezenas a centenas de triângulos** existem na tabela
  (ex. id `0x1cb` 384 tris, `0x08f` 257, `0x180` 233 no modo skip-3-words).
- Imagens em `out/attr-logo/*.png`: fragmentos 3D **sem** forma legível de
  logo SEGA (câmera/projeção do jogo não aplicada; packet format ainda
  parcialmente hipotético — `tgp.h` avisa que o renderer é boundary).
- **Não** é witness nomeado de logo 3D.

## 5. Fail-closed / fronteira

- Attract **submete objetos polygon ROM medidos** via tabela + FIFO/geo.
- Nome “logo SEGA 3D” **não** está provado (sem string, sem mesh visual
  inequívoca, sem packet único de title).
- Recovery C de logo: **não** implementar sem witness nomeado.

Próximos levers:

1. Quem no attract/game_disp chama `0x7c60` / indexa `0x020e0004` com quais IDs;
2. Aplicar matrix/câmera medidos (`geometry_mode`, focus) no render host;
3. Correlacionar IDs maiores (fora da família 0x140) com faça display/title;
4. Differential C do helper `0x7c60` / pol_test já medido no oracle.

## Validação observada

- Sem mudança de semântica recovery/executor nesta fatia (tools+notes).
- Focused CTest: ver saída no commit (esperado: mesmos 15/15 se não houve C).
- Logo 3D nomeado: **não** witness.

## Tools

- `measure_pol_test_logo.py`
- `correlate_object_table.py`
- `dump_poly_objects.py`
- `render_poly_objects.py`
- `scan_render_all_objects.py`
- `classify_tgp_packets.py`
