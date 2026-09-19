# v0361 — Continuação attract pós-SEGA: sel2→sel3→TEST MENU (medido)

## Sequência natural medida (oracle)

A partir da tela SEGA (v0360, `park-after-irq` → sel=1, countdown 640):

| Passo | sel | Evidência |
| --- | --- | --- |
| Frame SEGA draw | **0→1** | 15853 insns, glyphs, countdown 640 |
| Countdown em sel=1 | 1 | `resume-trace` mantém sel=1 (cd 640→639) |
| Countdown expire (cd forçado a 1) | **1→2** | `sega-after-cd`, flags `0x80000400`, tiles EXAD |
| Frame completo sel=2 | → **0x11** | copy=`0x10`, a4=`0x0b`, **TEST MENU** |
| Frame sel=3 (phase 0) | → **0x11** | mesmo handoff via 0x10 |

Forçar sel=4..15 no mesmo park (write `0x50002a` no início do `resume-trace`)
também termina em **TEST MENU 0x11** neste estado de máquina.

## Tabelas ROM (maincpu, LOAD32_WORD)

Frame dispatch `0x0000a6f8[selector]`:

| sel | worker |
| --- | --- |
| 0 | `0xa804` (SEGA warning / fast-path) |
| 1 | `0xa974` (countdown) |
| 2 | **`0xab0c`** (clear bits 4/15 flags, zero 0x500070/74, arming…) |
| 3 | **`0xacf8`** (selector-3 *phase* dispatcher) |
| 4–5 | `0xc45c` |
| 16 | `0x10a0c` (clear/redraw; C: `++sel`, a4=0x0b) |
| 17 | `0x10b5c` (TEST MODE workers) |

Selector-3 phases `0xaac4[phase]`: `0xae78,0xafe0,…,0xc414,0xc448` (0..17).
Recovery em `texture_bridge_match.c`: fases **12–15** fazem handoff para
**selector 16** (comentário: `f0f4` → sel 16), que incrementa para **17**
e grava `a4=0x0b` (TEST MENU).

## Por que não há logo 3D de jogo neste corredor

- FIFO de geometria / tags de luz **idênticas** TEST vs attract (v0355/v0360).
- Texture-ram: hash **muda** no frame sel3→TEST (`47f45425`/`59c163e3` vs
  baseline `b237cee7`/`ebaf1506`) — upload novo, mas nz na janela inicial
  ainda 0; **não** é witness de malha SEGA 3D.
- Nenhum tile `SEGA` além da tela legal já pinada.
- Com backup/config medidos (COUNTRY=JAPAN, factory-like, sixth/after-irq),
  o oracle **sempre** reentra TEST MENU após o attract legal.

## Tabelas erradas (correção)

Leituras `--read-u32` em probe após 100k steps **não** são confiáveis para a
jump table (valores contaminados). A tabela correta vem da imagem ROM
(`out/dump_rom_tables.py`) e bate com `VF2_SELECTOR2_BODY_ENTRY=0xab0c` e o
check `target==0xacf8` do C para selector 3.

## Fail-closed

Logo 3D / attract de **jogo** (demo/title poligonal) **não** foi witness.
Fronteira: estado de máquina que **não** tome o handoff sel3→0x10→0x11
(ex.: backup/config de operação normal com créditos, sem caminho de teste).
Não se inventa mesh nem se pinna logo sem esse estado.

## Parks (não git)

`sega-natural-frame`, `sega-after-cd`, `sega-sel3-p0`, `sega-sel3-f1..f3`,
`sega-rt-sel4..15`, `sega-natural-sel2/3`.
Tool: `out/dump_rom_tables.py`, `dump_io_control.py`.

## Validação desta fatia

- `vf2_native_runtime_tests`: **passed** (inclui pin SEGA v0360).
- Executor COBR CC e pin `0x270d4` **inalterados** (v0359).
- Sem mudança de semântica recovery além do que já estava em master; esta
  fatia é medição + documentação da fronteira attract→TEST.
