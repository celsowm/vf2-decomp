# v0376 — Onde vivem geometry_mode / matriz 3x4 / focus / câmera (attract/TGP)

Status: **medido**.  
Falha fechada mantida: **NÃO** foi medida matriz TGP 3x4 nem `focus_x/y` nem
`geometry_mode` como comandos de stream FIFO/geo. O que **existe** em Work-RAM
é o triplo de display e a escala de câmera — com IPs de store medidos.

Este documento é apenas evidência. Não altera C recuperada. Não inventa câmera.
Não é commit.

Ferramenta reproduzível: `tools/python/measure_tgp_state.py`  
Saídas compactas: `out/attr-transform/*.json`

---

## 1. Pergunta

Onde o estado de transformação usado (ou supostamente usado) para attract/TGP
realmente vive, dado que v0375 **provou ausência** dos opcodes TGP de classe
`0x07/0x09/0x0b/0x0c` no tráfego FIFO/geo filtrado?

---

## 2. Tabela de presença/ausência

| Estado | Endereços | Valores medidos (long-29 / emit31040) | Store IP / caminho | Confiança |
| --- | --- | --- | --- | --- |
| Triplo display (x,y,z) | `*(u32*)0x50084c + 0x54/58/5c` | `0x40c00000` 6.0f / `0x40966666` 4.7f / `0x41940000` 18.5f; ptr=`0x00515d00` | `0x00031024` `stt r8,0x54(r4)` em `display_transform_defaults@0x31004` | **measured** |
| Secundário display | display+`0x60/64/68` | `0` | `0x00031034` `stt r12,0x60(r4)` | **measured** |
| Aperture cursor | `0x005001e4` | long-29 `0x74`; emit31040 `0xec`; boot-sel03 `0x30`; boot-sel09 `0x98`; all0-ready1 `0xe0`; sixth-fresh `0x30` | `0x00031094` `stob` em `display_command_emit@0x31040` | **measured** |
| Escala de câmera | `0x00501084`, `0x00501088` | `0x44160000` (600.0f) em long-29/emit31040; **0** em boot-sel03 | `0x0001d34c` / `0x0001d35c` em `fa_camera_initialize_prefix@0x1d320` | **measured** |
| Câmera task +0x5c/+0x60 | `0x00515400` record (continuação `0x1d458`) | +0x5c=`0x435f3333` (≈223.2f), +0x60=`0x432ccccd` (≈172.8f); vetores +0x18/1c/20 = 3.32 / 3.37 / 3.0 | caminho recorrente `0x1d470–0x1d4c4` via FIFO copro aritmético | **measured** |
| `geometry_mode` TGP classe 0x07 | stream FIFO/geo | **ausente** após filtro (resíduo IEEE `±266.0f` em `0x884000`, sem word de modo aceita) | — | **absent** |
| `focus_x/y` TGP classe 0x09 | stream FIFO/geo | **ausente** (v0375 + v0376 reconfirmam 0 remanescentes) | — | **absent** |
| Matriz 3x4 TGP classe 0x0b | stream FIFO/geo | **ausente** (0 remanescentes; snaps sem 12-float identity-like) | — | **absent** |
| Translate TGP classe 0x0c | stream FIFO/geo | **ausente** | — | **absent** |
| Store guest de matriz TGP 12-float em Work-RAM | qualquer objeto estável | **não medido** | — | **absent** |

---

## 3. ROM estático (guest IPs medidos)

`out/maincpu.bin` (2 MiB). Busca por imediatos IEEE/endereços:

### 3.1 `display_transform_defaults` `0x00031004`

```text
0x31004  ld   0x0050084c, r4
0x3100c  lda  0x40c00000, r8          ; 6.0f
0x31014  lda  0x40966666, r9          ; 4.7f
0x3101c  lda  0x41940000, r10         ; 18.5f
0x31024  stt  r8, 0x54(r4)            ; STORE triplo display
0x31028  mov  0, r12
0x31034  stt  r12, 0x60(r4)           ; zera secundário
0x31038  st   r12, 0x70(r4)
0x3103c  ret
```

Chamador direto medido: `0x0002ec1c` (`display_runtime_initialize`).

### 3.2 `display_command_emit` `0x00031040`

```text
0x31040  ld   0x0050084c, r3
0x3104c  bbs  1, *(r3+0x40), 0x310cc  ; alt → display_transform_update
0x31050  lda  0x00800101, r15
0x31058  st   r15, (g11)[g12]         ; PROTOCOLO FIFO — não é classe 0x0b
0x3105c  ldt  0x54(r3), r4            ; lê triplo display
0x31060  ld   0x005001e4, r15         ; aperture cursor
0x31068  st   r4, 0x0090e000(r15)     ; pacote transform → aperture
0x31078  st   r5, 0x0090e000(r15)
0x31088  st   r6, 0x0090e000(r15)
0x31094  stob r15, 0x005001e4         ; atualiza cursor
0x3109c  lda  0x37806f6f, r15
0x310a4  st   r15, (g11)[g12]         ; protocolo
0x310a8  ldos 0x00070cbc, r10
0x310b8  call 0x00007c60              ; submit objeto
0x310c8  ret
```

**Conclusão estática:** o caminho de display serializa o triplo **na aperture
`0x0090e000[cursor]`**, com palavras de protocolo (`0x00800101`, `0x37806f6f`).
**Não** emite `geometry_write_matrix` / classe `0x0b` para portos geo.

Chamador direto medido: `0x0003143c`.

### 3.3 `display_transform_update` `0x000311b8`

Confirma constantes de destino `0x40c00000/0x40966666/0x41940000` via `lda`
em `0x311ec/0x311f4/0x311fc`. Pode copiar display+`0x54` para o objeto
selecionado `+0x18` (`0x31238 stt r8,0x18(r7)`). Ainda é display-state /
objeto fighter — **não** matriz TGP de stream.

### 3.4 Helper de objeto `0x00007c60`

Reconfirmado (v0372/v0373):

```text
protocolo 0x1a003434 → FIFO
tabela 0x020e0004[g0*16]
st  r8, 0x10(g10)    ; geo 0x800010
stq r8, (g10)[g12]   ; FIFO
```

Xrefs `0x7c60`: clusters `0x190f0`, `0x202xx–0x214xx`, `0x2edfc`, `0x305d8–0x308cc`,
`0x61cc8` (notas v0373), entre outros. Nenhum deles é um store de matriz 3x4.

### 3.5 Câmera init `0x0001d320`

```text
0x1d320  lda/st vetores de task (g13+0x18/1c/20)
0x1d344  lda  0x44160000, r15
0x1d34c  st   r15, 0x00501084         ; STORE escala X
0x1d354  lda  0x44160000, r15
0x1d35c  st   r15, 0x00501088         ; STORE escala Y
...
0x1d444  call 0x216b8
0x1d448  call 0x1f148
0x1d44c  lda/st continuação 0x1d458 em g13+0x0c
```

Outros hits de `0x44160000` em maincpu: `0x20ae9` (não instrução alinhada),
`0x6e2b8`/`0x6e2bc` (tabela/dados — não aceitos como store code).

### 3.6 Câmera recorrente — protocolo aritmético copro (NÃO é TGP matrix)

```text
0x1d470  lda  0x0b001616, r15
0x1d478  st   r15, (g11)[g12]         ; tag FIFO
0x1d47c  ld   0x00501084, r15
0x1d484  st   r15, (g11)[g12]         ; escala 600.0f
0x1d488  lda  0x435f3333, r15         ; constante ~223.2f
0x1d490  st   r15, (g11)[g12]
0x1d494  ld   (g11)[g12], r15         ; readback
0x1d498  st   r15, 0x5c(g13)          ; camera task +0x5c

0x1d49c  lda  0x0b001616, r15
0x1d4a4  st   r15, (g11)[g12]
0x1d4a8  ld   0x00501088, r15
0x1d4b0  st   r15, (g11)[g12]
0x1d4b4  lda  0x432ccccd, r15         ; ~172.8f
0x1d4bc  st   r15, (g11)[g12]
0x1d4c0  ld   (g11)[g12], r15
0x1d4c4  st   r15, 0x60(g13)

0x1d4e4  lda  0x12002424, r15         ; outra tag copro
...
```

**Bits de classe TGP** de `0x0b001616`:
`(0x0b001616 >> 23) & 0x1f = 0x16` — **não** é `0x0b`.
Padrão `0x??00YYZZ` alinhado às tags de protocolo FIFO medidas
(`0x1a003434`, `0x00800101`, `0x37806f6f`, `0x03000606`).

Interpretação fail-closed: **protocolo aritmético do copro via FIFO**
(operandos + readback para a task de câmera), não stream de geometria TGP.

---

## 4. Dinâmica (vf2probe) — BEFORE/AFTER

Cautela AGENTS/v0375: `--max-steps 0` **não** congela. Aqui usou-se
`--set-ip` no início da rotina + `--until` no `ret`/fronteira + `--max-steps`
pequeno + `--memory-trace`.

### 4.1 `display_transform_defaults` @ `0x31004`

Comando:

```text
build/Debug/vf2probe.exe --rom-dir roms/vf2 \
  --snapshot out/attr-long/long-29.vf2snap \
  --set-ip 0x31004 --until 0x3103c --max-steps 64 \
  --memory-trace --trace \
  --read-u32 0x50084c --read-u32 0x515d54 --read-u32 0x515d60
```

| Campo | BEFORE (snap) | AFTER (probe final / memory-trace) |
| --- | --- | --- |
| `0x50084c` | `0x00515d00` | `0x00515d00` |
| `0x515d54` | `0x40c00000` (6.0f) | rewrite `0x40c00000` @ ip=`0x31024` |
| `0x515d58` | `0x40966666` (4.7f) | rewrite `0x40966666` |
| `0x515d5c` | `0x41940000` (18.5f) | rewrite `0x41940000` |
| `0x515d60` | `0` | rewrite `0` @ ip=`0x31034` |
| halt | park ip `0x22498` | stop address `0x3103c`, `run_instructions=10` |

Trace: `out/attr-transform/probe_disp_defaults_31004b.jsonl`.

### 4.2 Escala de câmera @ `0x1d320`

```text
build/Debug/vf2probe.exe --rom-dir roms/vf2 \
  --snapshot out/attr-long/long-29.vf2snap \
  --set-ip 0x1d320 --until 0x1d364 --max-steps 400 \
  --memory-trace --read-u32 0x501084 --read-u32 0x501088
```

| Campo | BEFORE | AFTER |
| --- | --- | --- |
| `0x501084` | `0x44160000` (600.0f) | rewrite `0x44160000` @ ip=`0x1d34c` |
| `0x501088` | `0x44160000` (600.0f) | rewrite `0x44160000` @ ip=`0x1d35c` |
| task+0x18/1c/20 | (valores de park) | `0xbe570a3d` / `0x3f47ae14` / `0xc0d3d70a` |

Trace: `out/attr-transform/probe_cam_init_1d320.jsonl`.

### 4.3 Câmera recorrente @ `0x1d458` (FIFO copro arith)

```text
build/Debug/vf2probe.exe --rom-dir roms/vf2 \
  --snapshot out/attr-long/long-29.vf2snap \
  --set-ip 0x1d458 --until 0x1d4c8 --max-steps 300 \
  --memory-trace --trace
```

Writes medidos (`step` correlacionado a `ip_before`):

| step | ip_before | addr | value |
| ---: | ---: | --- | --- |
| …666 | `0x1d478` | `0x00884000` | `0x0b001616` |
| …668 | `0x1d484` | `0x00884000` | `0x44160000` (600.0f) |
| …670 | `0x1d490` | `0x00884000` | `0x435f3333` |
| …672 | `0x1d498` | task+0x5c | `0x435f3333` (readback = última constante) |
| …674 | `0x1d4a4` | `0x00884000` | `0x0b001616` |
| …676 | `0x1d4b0` | `0x00884000` | `0x44160000` |
| …678 | `0x1d4bc` | `0x00884000` | `0x432ccccd` |
| …680 | `0x1d4c4` | task+0x60 | `0x432ccccd` |

Neste probe a `g13` efetiva apontou para `0x00514980` (registro vazio no snap);
o registro **vivo** com continuação `0x1d458` e +0x5c/+0x60 preenchidos é
`0x00515400` no park long-29. Não se promove semântica de “foco” para
223.2/172.8 — são valores medidos em task de câmera via FIFO copro.

Trace: `out/attr-transform/probe_cam_recur_1d458.jsonl`.

### 4.4 `display_command_emit` @ `0x31040` (park emit31040)

Park `out/attr-logo/emit31040.vf2snap` já está em `ip=0x310c8` (ret do emit).
Probe com `--set-ip 0x31040 --until 0x310c8 --max-steps 4000 --set-u32 0x500064=5`
executou o helper `0x7c60` e parou no `ret`. Reads finais:
`0x50084c=0x515d00`, `0x515d54=6.0f`, `0x5001e4=0xec`.
Pacotes FIFO do helper dependem de `g10/g11/g12` do park; não se trata de
matriz TGP. Evidência v0373 (objeto `0xee1` da tabela `0x70cbc`) permanece.

---

## 5. Snaps — regiões

Parks lidos **sem executar** (`dump_attract_state` / `measure_tgp_state.py`):

| Park | ip | sel | phase3 | `0x5001e4` | `0x50084c` | triplo +0x54 | `0x501084/88` |
| --- | --- | ---: | ---: | --- | --- | --- | --- |
| long-29 | `0x22498` | 3 | 10 | `0x74` | `0x515d00` | 6.0/4.7/18.5 | 600/600 |
| emit31040 | `0x310c8` | 3 | 14 | `0xec` | `0x515d00` | 6.0/4.7/18.5 | 600/600 |
| boot-sel03-fifo | `0x2d38` | … | … | `0x30` | `0x515d00` | 6.0/4.7/18.5 | **0/0** |
| boot-sel09-fifo | … | … | … | `0x98` | `0x515d00` | 6.0/4.7/18.5 | (ver `region_*.json`) |
| all0-ready1 | … | … | … | `0xe0` | `0x515d00` | 6.0/4.7/18.5 | … |
| sixth-fresh | … | … | … | `0x30` | `0x515d00` | 6.0/4.7/18.5 | … |

Região `geometry` do snap (~32 KiB): ~40 words nz, **controle/ponteiro-like**
(ex.: long-29 head `0x80000000`, `0x0008942e`, `0x00000004`, `0x00008808`).
Scan `looks_like_matrix` (12 floats, banda IEEE plausível, score de identidade):
**0 candidatos identity-like** em geometry/coprocessor de todos os parks
medidos.

Objeto display `0x515d00` (long-29): +0x0c=`0x0002eaa0` (ponteiro runtime),
+0x40 flags=0, +0x54.. = triplo, +0x80=`0xa8000000`. **Não** contém 12 floats
de matriz 3x4.

Objeto câmera `0x515400` (long-29, registro de task):

```text
+0x0c continuação = 0x0001d458
+0x18/1c/20      = 3.32 / 3.37 / 3.0
+0x40            = 0x0e
+0x44..0x58      = 0.75 / 0.95 / 1.0 / 0.4 / -0.8 / 1.35
+0x5c/+0x60      = 223.2 / 172.8
+0x64            = 1.0
```

Campos medidos; **sem** renomear para focus/matrix sem evidência adicional.

---

## 6. Classificação de TODAS as escritas geo-port (v0376)

Fontes: `fifo-phase5`, `fifo-attract`, `boot-sel03/09`, `emit31040`,
`long29-obj` (traces já existentes; não git).

Métrica agregada:

| Métrica | Valor |
| --- | ---: |
| Escritas geo-port `0x800010`/`0x804000` | **928** |
| Classe 0x09 bruto / protocolo / restante | 412 / 412 / **0** |
| Classe 0x0b bruto / protocolo / restante | 38 / 38 / **0** |
| Classe 0x0c bruto / protocolo / restante | 46 / 46 / **0** |
| Classe 0x07 bruto / protocolo / restante | 94 / 72 / **22** (IEEE `±266.0f` em FIFO) |

Valores geo-port mais frequentes: inteiros de `w0` de tabela de objetos
(`0x00ac1502`, `0x00f8013f`, `0x00040401`, …) — helper `0x7c60`.
Também aparecem IEEE pequenos (`0x3d2b367a≈0.042`, `0xbc11d14e≈-0.009`, …)
compatíveis com pacotes mesh/prim do dump geo de v0375 §3.2.
**Nenhum** é pacote classe-09/0b de câmera.

Tags copro aritméticas vistas **no FIFO**, não como opcode de matriz:

| Word | Onde | Classe TGP | Aceito como matrix/focus? |
| --- | --- | ---: | --- |
| `0x0b001616` | FIFO `0x884000` | 0x16 | **não** |
| `0x12002424` | FIFO `0x884000` | 0x04 | **não** |
| `0x00800101` | FIFO (emit/display) | 0x00 | protocolo |
| `0x1a003434` | FIFO (helper 0x7c60) | 0x1a | protocolo |
| `0x37806f6f` | FIFO (emit) | 0x1b | protocolo |

Em phase5/attract: `0x0b001616`×2, `0x12002424`×2, `600.0f`×9 no FIFO,
`6.0f` escrito em `0x515d54` (attract) e `0x90e0e0` (aperture/emit).

---

## 7. O que o re-render pode usar HOJE (apenas medido)

`out/attr-transform/measured_view.json` permanece:

```json
{
  "matrix": null,
  "focus_x": null,
  "focus_y": null,
  "geometry_mode": null,
  "confidence": "absent"
}
```

Inputs medidos **permitidos** (não são matriz TGP):

1. IDs de objeto + offsets polygon-ROM (v0372/v0373).
2. Triplo display `(6.0, 4.7, 18.5)` em `0x515d54/58/5c` — display-state.
3. Escala câmera `0x501084/0x501088=600.0f` onde nonzero.
4. Cursor aperture `0x5001e4` e janela `0x0090e000[]`.
5. Hex de pacotes FIFO/geo (protocolo + prim mesh).
6. `apply_tgp_transform.py` **deve** continuar identity passthrough
   (`confidence=absent`). Não inventar câmera a partir de hybrid C.

---

## 8. Fail-closed

- **Não** promover `(6.0, 4.7, 18.5)` a matriz de view TGP.
- **Não** promover `0x0b001616` a opcode de matriz só porque o byte alto é `0x0b`.
- **Não** promover `223.2 / 172.8` a `focus_x/y` sem um pacote classe-0x09 medido.
- **Não** nomear objetos attract (`0x88/0x14x`) como logo/title (v0373).
- **Não** copiar pseudocode/MAME para `src/recovered/`.
- **Não** commitar snaps/traces/ROMs.

---

## 9. Artefatos (não git)

```text
tools/python/measure_tgp_state.py
out/attr-transform/summary.json
out/attr-transform/static_store_ips.json
out/attr-transform/absence_table.json
out/attr-transform/geo_port_classes.json
out/attr-transform/measured_view.json          # confidence=absent
out/attr-transform/region_*.json
out/attr-transform/probe_*.jsonl
out/attr-transform/probe_evidence.json
```

---

## 10. Próximos alavancas (medidos, não implementados aqui)

1. Correlacionar aperture `0x0090e000[cursor]` com pacotes que o TGP realmente
   consome — o triplo de display pode ser **entrada de um estágio host/coprocessador**
   ainda não classificado como stream de geometria.
2. Decodificar o protocolo copro `0x0b001616` / `0x12002424` como aritmética
   (operandos/readback) e provar com estados sintéticos; **não** tratar como
   matriz TGP.
3. Procurar stores de 12 floats em outros corredores (fa_player / game_disp)
   com `--memory-trace` em parks que ainda não estão em attract phase5.
4. Taint/edges nas escritas `0x515d54` e `0x501084` para ver se algum bit
   alimenta um submit geo subsequente de forma não-protocolar.
5. Manter `extract_fifo_transforms.py` / `apply_tgp_transform.py` em
   `confidence=absent` até existir witness de matriz/focus.

---

## 11. Validação observada

- `py_compile` de `tools/python/measure_tgp_state.py`: OK.
- `measure_tgp_state.py` executado: 6 parks parseados, 928 geo-port writes,
  `measured_view.confidence=absent`.
- Probes `0x31004`, `0x1d320`, `0x1d458`: `status=ok`, halt nos `--until`
  esperados, escritas listadas acima correlacionadas por `ip_before`.
- Nenhuma mudança em `src/recovered`, executor, oracle ou `CHANGELOG`.
- Gate ROM-backed de recovery: **não aplicável** (sem semântica nova de jogo).
- **Não commit.**
