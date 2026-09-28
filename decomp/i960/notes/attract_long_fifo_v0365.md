# v0365 — Attract longo: fases 3→8, FIFO TGP diferencial, fail-closed logo 3D

## Campanha (autônoma)

Receita: `resume-trace` com `0x500704=0` (gate `0xa748`) a partir de
`sega-after-cd` / parks `attr-nav` / `attr-long`, oráculo de referência
com COBR `teste` (v0364).

### Progressão medida (sel permanece 3, **não** TEST MENU)

| Marco | phase3 | cd `0x500024` | nz texture 64k | tex0 hash |
| --- | --- | --- | --- | --- |
| pós-`teste` f00–f02 | 3 | 256..253 | 0→**4360** | muda |
| drenagem natural (~240 frames) | 3→**4** em cd=0 | 1/frame | 4360 | estabiliza `469556ed` |
| scout cd=1 | 4→**5** (cd=2926) | 1/frame | 4360 | estável |
| scout cd=1 em p5 | 5→6→**7→8** | 0 | 4360→**10079→10154** | muda a cada frame |
| ready `0x550000=1` | permanece **8** | 0 | ~10154 | anima |

- IP na fase 8: cluster **`0x4c7xx–0x4cfa0`** (corpo de objeto/task —
  mesmo tipo de caminho que passou por `teste` em `0x19024`).
- Tiles 0x80xx/0x89xx: **vazios** em todo o attract medido (sem texto
  `SEGA` / `VIRTUA` / TEST).
- Geometria ~40–41 words nz; hash muda na fase 8; **não** decodificado
  como malha única.

### FIFO TGP diferencial (memory-trace)

| Park | Janela | FIFO writes `0x884000` | reads | nz tex64k |
| --- | --- | --- | --- | --- |
| Attract `navclear-f01` → `0x19024` | ~400k passos | **1157** | 270 | 0→upload |
| Attract fase 5 → `0x23f20` | ~300k | **2016** | 586 | 4360 |
| TEST `sixth-fresh` → `0xa6c0` | ~400k | **9** | 3 | **0** |

Function codes (bits 23–28 do word LE) mais frequentes no attract:
`0, 1, 2, 6, 15, 19, 41, 45, 52, 53, 55, 57, 58, 59, 60, 61, 63`
(com amostras IEEE-like). TEST quase não toca o FIFO.

Também writes geo `0x804000+` no attract (program/stream), ausentes no
TEST.

Tools: `tools/python/attract_long_campaign.py`,
`analyze_attract_witness.py`; parser `out/scan_fifo_trace.py`
(não commitado — scratch).

## Fail-closed (fronteira do logo 3D)

Com evidência desta campanha:

1. O attract de jogo **é alcançável** (nav gate + `teste` + fases 3–8).
2. O TGP **é alimentado** muito mais que no TEST (FIFO/texture).
3. **Não** há witness de malha/logo 3D único:
   - sem tile ASCII/`0x89xx` de SEGA/title;
   - sem packet FIFO correlacionado a mesh nomeada;
   - texture nz cresce, mas sem dump pixel/objeto que prove “logo SEGA”.
4. Fase 8 fica em loop de objeto; fases 9–17 do sel3 **não** foram
   atingidas naturalmente nesta máquina/park (precisariam de outros
   gates de task/timeline — medir, não forçar inventando).

Não se recupera C de logo 3D sem esse witness.

## Validação observada desta fatia

- Sem mudança de semântica recovery/executor em **v0365** (tools+docs).
- Executor `teste` (v0364) permanece: `vf2_tests` all passed;
  focused CTest **7/7** observado antes desta fatia; re-executado no
  commit se houver rebuild — senão citar o resultado já observado em
  `b611c82`.

Parks locais: `out/attr-long/*`, `out/attr-long/fifo-*.jsonl` (não git).
