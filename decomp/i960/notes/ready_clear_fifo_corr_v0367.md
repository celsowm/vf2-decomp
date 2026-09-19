# v0367 — Clear de `0x550000` medido + correlação FIFO/texture + trava phase14

## 1. Quem grava `0x00550000` (ROM)

Varredura da imagem maincpu (stores `st r3, 0x00550000`):

| Site | Valor | Papel |
| --- | --- | --- |
| `0x0004b414` | **1** | video command submit (note v0026) |
| `0x0004b83c` | **1** | submit variante |
| `0x0004ba14` | **1** | submit variante |
| **`0x0004bfc4`** | **0** | **clear** do latch |

Clear em `0x4bfc4` (final-status / texture):

```text
cmpobne 0, r14, skip
ld 0x005502e0, r14      ; texture counter2
cmpobne 0, r14, skip
mov 0, r3
st r3, 0x00550000       ; ready = 0
```

Condicional: **r14==0 e counter2 (`0x5502e0`)==0**.

C recovered `execute_texture_final_status_call`
(`VF2_TEXTURE_FINAL_STATUS_ENTRY = 0x4bf90`) escreve `0` em
`0x550000` quando counters `0x5502c0/d0/e0` são todos 0 — coerente
com o gate ROM.

`src/hardware/model2a.c` / `tgp.c`: **nenhum** auto-clear de
`0x550000` na conclusão de vídeo/TGP. O latch só cai se o **i960**
executar `0x4bfc4` (ou o C final-status) com contadores zerados.

## 2. Estado medido nos parks

| Park | ph | ready | ctr0/2 | nz_tex 64k |
| --- | --- | --- | --- | --- |
| postteste-f02 | 3 | 1 | 1 / 1 | 0 |
| long-29 | 0xa | 0 | 0 / 0 | 10154 |
| s04-frame (phase14) | 0xe | **1** | 0 / 0 | 10154 |
| sixth-fresh TEST | 0 | 0 | 0 / 0 | **0** |

Em phase14 o latch fica **1** com contadores **0** — o clear ROM
`0x4bfc4` **não** é observado no caminho de objeto/spin `0x4c7xx` /
`0x224xx` desta máquina.

## 3. Phase14 não avança (ROM + C)

Worker ROM `0xc0a4` e C `execute_selector3_phase14`:

- decrementam `[0x500834]+0x50`;
- se ctr==0 e `0x550000==1` → **ret**;
- se ctr==0 e ready≠1 → `balx 0x9444` (thunk);
- **não escrevem** `0x500030 = phase+1` neste worker.

Portanto fases **15–17** não são atingidas só drenando phase14.
Incremento medido existe em outros workers (ex. phase3 quando
`terminal_gate != 1`).

## 4. Correlação FIFO TGP ↔ texture (medida)

| Conjunto | FIFO writes `0x884000` | nz_tex64k |
| --- | --- | --- |
| attract phase3/5 traces | **1157 / 2016** | 0 → 4360 → **10154** |
| TEST sixth → a6c0 | **9** | **0** |

Function codes (bits 23–28) ricos no attract (0,1,2,6,15,19,41,45,…)
vs raros no TEST (22,8,6,0,36). **Não** há mapeamento code→mesh
nomeada → **fail-closed** para logo 3D.

Tool: `tools/python/correlate_fifo_texture.py`,
`dump_video_ready.py`.

## 5. Fail-closed / fronteiras

- Logo 3D nomeado: **não** witness (mesmo com fases 3–14 e FIFO vivo).
- Phase15+ : só com outro gate medido (não inventar).
- Não alterar recovery de phase14 para “parecer” avançar sem pin
  C==oracle.
- Hardware não modela conclusão de vídeo que limpe `0x550000` —
  documentado; extensão futura seria observer **passivo** + medida.

## Validação

- Sem mudança de semântica recovery/executor em v0367 (tools+notes+docs).
- Focused CTest permanece o resultado observado em v0365/v0364 (**7/7**).
