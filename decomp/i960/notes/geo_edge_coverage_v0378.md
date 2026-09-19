# v0378 — Cobertura medida de edges guest i960 em paths geo/TGP

Status: **partial / medido** (fail-closed).  
Unidade de cobertura: **edge guest `ip_before -> ip_after`**, não coverage C do host.  
Não relitiga sideband-ports matrix `0x880000/0x980000/0x800000` (negativo v0377 P2 permanece).  
Não promove classes TGP `0x07/09/0b/0c` nem inventa semântica de stream.  
Não git-commit deste note junto com snaps/traces/ROMs.

Entregável canônico:

```text
out/attr-geo-edges/
  edge_list.json          # lista de edges medidos + ranking
  summary.json            # contagens + top new
  static_rank.json        # ranking estático (calls 0x7c60/0x7f24)
  mine_baseline.json      # mineração de traces existentes
  probe_geo_ip_inventory.json
  probe_*.jsonl / *.meta.json
  snippet_*.json          # janelas memory-trace minimizadas
  snippets.json
tools/python/explore_geo_edges.py
```

---

## 1. Pergunta

Existem **edges guest medidos** que escrevem geo RAM / FIFO **além** dos sites
de submit já conhecidos (helper `0x7c60`, palette `0x7f24`, task `fa_pol_test`
`0x21a00`, emit `0x31040`, camera `0x1d4xx`)?

Foco: paths que **podem** empurrar payload de geometria/TGP
(classes `0x01/02/05/07/09/0b/0c` ou escritas multi-word em geo RAM),
sem reabrir o negativo de matrix ports.

---

## 2. Estático (ROM maincpu)

Ferramenta: `tools/python/explore_geo_edges.py static`.

Encoding de call medido no oráculo/disasm: `target = ip + sign_extend(imm24)`
(palavra em `0x21a9c = 0x09fe61c4 -> 0x7c60`).

| Item | Contagem |
| --- | ---: |
| `call 0x7c60` (submit objeto) | **140** |
| `call 0x7f24` (palette → geo RAM) | **18** |
| imediato ROM `0x800010/0x804000/0x800000` | 1 (stores usam `g10`-rel, não abs) |

Clusters de `call 0x7c60` (proximidade medida, `functions.csv` coarse):

| Cluster | Exemplos de IP | Atribuição `functions.csv` |
| --- | --- | --- |
| `0x190xx–0x197xx` | `0x190f0`, `0x1918c`, `0x193c8`, … | `main_texture_orchestrator_call` `0xa030–0x4bcd4` (bloco coarse) |
| `0x202xx–0x214xx` | task/pol_test/emit | fora de ranges finos |
| `0x30xxx` / emit | `0x310b8` (notas v0372) | coarse |

Helper body `0x7c60–0x7d14` cai em `frame_shadow_verify` `0x530–0x9ffc`
(**nome do CSV não é semântica de submit** — não renomear).

---

## 3. Probes controlados (medidos)

Parks: `out/attr-long/long-29.vf2snap`, `long-14.vf2snap`,
`out/attr-fs/all0-ready1.vf2snap`, `out/sixth-fresh.vf2snap`.

Receitas (todas com `--memory-trace --trace`; **nunca** `--max-steps 0`):

| Caso | Sets | IP | Until | max-steps |
| --- | --- | --- | --- | --- |
| `pol_*_mode{0..3}` | `0x501018=0x1000`, `0x50101c=0`, `0x530150=mode`, counts `0x53014c/d/e=2` | `0x21a00` | `0x21be8` | 8000 |
| `h7c60_*_{985,97d,148,ee1}` | gate open; `g0=id`, `g1=0/1` | `0x7c60` | `0x7d14` | 4000 |
| `h7f24_*` | `g0=0x501400` | `0x7f24` | `0x7f64` | 2000 |
| `attr_l29_phase*` | `0x500704=0`, `0x550000=0`, cds/masks | `0xa6c0`/`0xa748` | — | 12000 |

**Nota de halt:** probes `h7c60_*` com `until 0x7d14` **não** param no ret do
helper (`0x7d10` salta para o caller; `0x7d14` é o slot seguinte no corpo).
Halt observado = `maximum steps`. Isso **expandiu** a janela: o run continua
no caller e alcança vizinhos geo — útil para edges, não é freeze.

**pol_test modes 0–3:** traces idênticas (325 insns, stop `0x21be8`); o set
`0x530150` + counts=2 **não** divergiu o conjunto de IPs de escrita geo nestes
parks. Fail-closed: não afirmar semântica de mode a partir daqui.

**TEST vs attract:** `sixth-fresh` escreve geo só no helper + família
`0x2ee8/0x2eec/0x2f50` (10 writes). Attract `long-29` resume alcança **74–77**
IPs geo (loop de objetos vivo).

---

## 4. IPs guest que escrevem geo (medidos)

Inventário completo: `out/attr-geo-edges/probe_geo_ip_inventory.json`.

### 4.1 Já conhecidos (baseline)

| IP | Papel medido |
| --- | --- |
| `0x7c74/7c7c/7c94/7d08/7d0c` | helper `0x7c60`: proto `0x1a003434`, `st w0,0x10(g10)`, `stq w0..w3` |
| `0x7f24/0x7f50` | palette: 6 words `(g10)[g12]` |
| `0x2ee8/0x2eec/0x2f50` | geometry-table helpers (warm geometry, v0138) |
| `0x21a00..0x21be8` | `fa_pol_test` path A/B (FIFO proto + calls `0x7c60`) |
| `0x31040..0x310c8` | `display_command_emit` + call `0x7c60` |
| `0x1d4xx` | camera FIFO arith (não matrix) |

### 4.2 NOVOS edges com escrita geo (fora do corpo `0x7c60`/`0x7f24`)

| Edge / IP | Escritas medidas | Classificação fail-closed |
| --- | --- | --- |
| **`0x19680 -> 0x19684` / `0x19684 -> 0x19688`** | mesmo `step`: `0x804000=w0`, `0x804004=w1`, `0x804008=w2`, `0x80400c=0xffffffff` | **SIM: push multi-word geo RAM** com forma `w0/w1/w2/-1` (idêntica a `stq` do helper). Guest IP **fora** de `0x7c60`. Amostra: `0x004b54b0,0x00162304,0x00000104,0xffffffff`. ROM: `st r12,0x10(g10)` @ `0x19680`; `stq r12,(g10)[g12]` @ `0x19684`; depois `call 0x7b18` |
| `0x7b1c -> 0x7b20`, `0x7b20 -> 0x7b24` | `0x800070=0`; `(g10)[g12]=1/2/3` | Helper `0x7b18` (`st r3,0x70(g10)`; `st g0,(g10)[g12]`; `ret`). **Controle geo**, não mesh multi-word. Heurística de stream superestimou |
| `0x20774 / 0x20790` | `0xb0(g10)`, `0x1008(g10)` + FIFO `0x1a003434` | Sequência **irmã inline** do protocolo do helper `0x7c60` (mesma word de proto) na família `0x20xxx`. Não é `call 0x7c60`. Payload poly **não** witness aqui |
| `0x1dc2c / 0x1dc34 / 0x1dc3c / 0x1dc58 / 0x1dc5c` + cauda `0x1dcxx–0x1e5xx` | multi-word `(g10)[g12]` + slots `g10+0x90/0x160` + FIFO `0x03000606` (família pol_test) | Escritas geo RAM medidas; protocolo FIFO pol_test-like. **Não** class TGP matrix |
| `0x19220 / 0x19268 / 0x19294` | no mesmo corpo `0x19008–0x198c0` | Loop de objetos attract: proto FIFO `0x800101`/`0x1b803737` + `call 0x7c60` + path inline `0x1968x` |
| FIFO `0x18e1c/0x18e34/0x19038/0x19044/...` | `0x884000+` multi-word | Protocolo/arith copro (classe medida ≠ matrix) |

### 4.3 Contagens agregadas (`edge_list.json`)

| Métrica | Valor |
| --- | ---: |
| Edges totais com write geo/FIFO | **384** |
| Edges fora baseline estático fixo | **366** (inclui cauda `0x1dcxx` e FIFO) |
| Candidatos stream-push (heurística) | **3** |
| Candidatos **reclassificados** como geo-stream push multi-word | **1** (`0x19684`) |
| Classes TGP `0x07/09/0b/0c` witness | **0** |

---

## 5. Snippet minimizado — push geo-stream candidato

`out/attr-geo-edges/snippet_*.json` / `snippets.json`.

Caso **`0x19684`** (park `long-29`, `attr_l29_geo_gate_resume`):

```text
step 54111041  ip 0x19684->0x19688
  write 0x00804000 = 0x004b54b0   ; w0
  write 0x00804004 = 0x00162304   ; w1
  write 0x00804008 = 0x00000104   ; w2
  write 0x0080400c = 0xffffffff   ; r11=-1
step 54113681  ip 0x19684->0x19688
  write 0x00804000 = 0x004b4f7e
  write 0x00804004 = 0x00161e14
  write 0x00804008 = 0x00000004
  write 0x0080400c = 0xffffffff
```

ROM no IP (disasm medido):

```text
0x1967c  subo 1, 0, r15           ; -1
0x19680  st r12, 0x10(g10)        ; geo slot ← w0
0x19684  stq r12, (g10)[g12]      ; geo RAM ← w0,w1,w2,-1
0x19688  mov 1, g0
0x1968c  call 0x00007b18          ; helper controle geo
```

Interpretação fail-closed:

- É **submit de objeto/record para geo RAM** na mesma forma medida do helper
  `0x7c60` (`stq` + `w3` anulado para `-1`).
- **Não** é witness de vértice transformado nem de matrix `0x0b` / focus `0x09`
  / mode `0x07` / translate `0x0c`.
- Vértices poly continuam **fora** do oráculo guest via `w2` (v0377).
- Não renomear campos (`w0/w1/w2`) sem evidência adicional.

Caso `0x7b1c/0x7b20`: writes de **controle** (valores 0–3) — não promover.

---

## 6. Ranking vs `decomp/i960/functions.csv`

`functions.csv` tem ranges **coarse** (92 entradas). Atribuições observadas:

| Edge novo | recovered range | Status CSV |
| --- | --- | --- |
| `0x19684` | `0xa030–0x4bcd4` `main_texture_orchestrator_call` | recovered-control-block (**não** prova semântica do stq) |
| `0x7b1c` | `0x530–0x9ffc` `frame_shadow_verify` | recovered-observed-branch (nome ≠ submit) |
| `0x20774` | fora de range fino listado | **unattributed** no CSV atual |
| `0x1dc2c` | fora / coarse | unattributed fino |

Top edges para recovery (prioridade medida):

1. **`0x19680/0x19684`** — sibling inline de object-submit geo RAM (prioridade).
2. **`0x20774`** — proto `0x1a003434` inline na família `0x20xxx`.
3. **`0x1dc2c` family** — multi-word geo RAM + proto pol_test `0x03000606`.
4. **`0x7b18`** — helper de controle geo pós-submit (pequeno, recuperável).
5. Corpo `0x19008–0x198c0` — loop de objetos attract (chama `0x7c60` **e** path inline).

---

## 7. O que **não** foi encontrado (fail-closed)

- **0** writes guest medidos para classes TGP matrix/focus/mode/translate
  (`0x07/09/0b/0c`) em FIFO `0x884000` ou geo `0x800000/0x804000`.
- Ports `0x880000` / `0x980000` / matrix `0x800000`: continuam **absent**
  nestes parks/probes (v0377 P2 não relitigado; reconfirmado por ausência).
- pol_test `0x530150` modes 0–3 com counts=2: **sem** novo edge geo.
- `attr_all0_nav0`: 0 geo/FIFO writes em 12k steps (halt max-steps).
- Nenhum path witness de “classe 0x01/02/05” como packet de stream provado —
  apenas records objeto `w0/w1/w2/-1` e protocolos FIFO conhecidos.

---

## 8. Outputs / tools

```text
tools/python/explore_geo_edges.py
out/attr-geo-edges/edge_list.json
out/attr-geo-edges/summary.json
out/attr-geo-edges/static_rank.json
out/attr-geo-edges/mine_baseline.json
out/attr-geo-edges/probe_geo_ip_inventory.json
out/attr-geo-edges/snippets.json
out/attr-geo-edges/snippet_00019684_00019688.json
decomp/i960/notes/geo_edge_coverage_v0378.md   # este arquivo
```

Traces/parks/snaps: **não git**.

---

## 9. Próximos passos (evidência, não semântica inventada)

1. Minimizar witness `0x19684` com `minimize_case.py`-style: mutar só `g0/g1`
   e table id; provar que `w0/w1/w2` vêm de `0x020e0004[g0*16]` ou de outro
   record (medir load antes do `stq`).
2. Atribuir callers do corpo `0x19008` (scheduler / object index `r8` vs `r9`).
3. Trace `0x20774` e `0x1dc2c` isolados (`--set-ip` + `--until` no `ret`
   **do caller**, não `0x7d14`).
4. Correlacionar `w2` destes records com poly-ROM (mesmo método v0372) —
   só então falar em classe de primitive; até lá permanece record object.
5. Manter matrix/focus/mode **absent** até packet classificado surgir em
   memory-trace com edge guest associado.

---

## 10. Validação

- Sem mudança de semântica C recovery/executor nesta fatia (tools + notes + out/).
- Probes: `vf2probe` `build/Debug/vf2probe.exe`, ROM `roms/vf2`.
- Python: `$env:MIMO_PYTHON`.
- Gates CTest/sanitizer: **não executados** nesta fatia de análise (sem change C).
- Política: evidence-first; fail-closed; coverage = edge guest; sem commit.
