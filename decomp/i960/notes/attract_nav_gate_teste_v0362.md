# v0362 — Attract pós-SEGA: gate nav `0xa748` e fase 3 medida

## Definição

Logo 3D de jogo continua **não** witness. Esta fatia mede **por que** o
oracle “sempre” caía em TEST MENU e o que acontece quando o gate de input
é neutralizado.

## Gate medido (ROM + trace oracle)

Função **`0x0000a748`** (`frame_geometry_gate`), chamada pelo scheduler
após o cluster (`0xa014 → 0xa748`):

```text
r8 = *(u32*)0x00500704          ; navigation / edge flags
if (r8 bit 26) or (r8 bit 2):
    if sel != 0x11: sel = 0x10  ; stib 16, 0x50002a
    ret
ret
```

No park `sega-after-cd` o oracle lia `0x500704 = 0x0f000000`
(**bit 26 set**) e gravava **`sel 2 → 0x10` em uma única `stib`**
(trace `out/mtrace-cd.txt`, step 6873215, IP `0x0000a76c`).

Frame selector 16 (`0x10a0c`) e o worker sel17 (`0x10b5c`) então
completam o caminho **TEST MENU** (`a4=0x0b`, tiles `TEST MENU|…`).

Isto **não** é o handoff ROM “phases 12–15 → sel16” do recovery C:
é um **gate de input/scheduler** que força sel=16.

## Receita para o attract de jogo (medida)

Com **`resume-trace` gravando `0x500704=0` no início de cada frame** a
partir de `sega-after-cd` (sel=2 pós-SEGA):

| Frame | sel | phase3 | tiles | TEST? |
| --- | --- | --- | --- | --- |
| f00 | **0x02** | 0 | EXAD | não |
| f01 | **0x03** | 0 | EXAD | não |
| f02 | **0x03** | **3** | plano limpo | não |

- Geometria/buffer **mudam** de hash entre f00/f01/f02 (conteúdo novo).
- `0x500700` permanece `0x0f000000` (idle/IRQ); o gate olha **`0x500704`**.
- Forçar só `0x500700=0x0ff7f700` (idle TEST) **não** evita TEST se o
  bit26 em `0x500704` voltar.
- COUNTRY 0/1/2 com o gate ativo **não** muda o handoff para TEST.
- COUNTRY≠0 em selector-0 **pula** a tela legal SEGA (ROM `0xa804`).

## Fronteira fail-closed: `teste` em `0x00019024`

Com nav limpo e sel=3/phase=3, o oracle avança (~160k insns) e **para** em:

```text
Resume trace stopped: status=unsupported operation
IP=0x00019024  instruction: teste r15
```

`teste` é COBR de condição (`decoder.c` cobr_names[2]); o executor de
referência **não** implementa a semântica de execução. O bloco parece
caminho de objeto/fighter (g7, `+0x1a4`, FIFO `0x884000`) do attract/demo.

Sem `teste` no executor, o attract de jogo **não** atravessa esse corpo.
Logo 3D / malha SEGA **não** foi witness neste estado.

## Fail-closed (regra do projeto)

- Não se inventa mesh 3D.
- Não se “conserta” o recovery de sel16 para parecer attract.
- Extensão futura do executor para `teste`: semântica **arquitetural**
  i960 (condição → 0 / 0xffffffff no operando destino), provada com
  snapshot diferencial no park navclear — **não** semântica de jogo.

## Parks / tools (não git, excepto tools genéricos)

Parks: `out/attr-nav/navclear-f00..f02`, `out/cd-nav0*`, `out/mtrace-cd.txt`.
Tools commitáveis: `tools/python/dump_attract_rom.py`,
`dump_attract_state.py`, `sweep_attract_config.py`,
`attract_multiframe.py`, `attract_navclear.py`.

## Validação desta fatia

- Sem mudança de semântica recovery/executor em master nesta fatia
  (apenas tools + notes + docs).
- Baseline focused CTest deve permanecer verde (ver commit).
- Head anterior: `4346f04` (v0361).
