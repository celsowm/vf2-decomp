# v0366 — Mapa de fases do attract sel3 (3–14) e gate `0x550000`

## Resultado medido (continuação v0365)

Com `0x500704=0` (gate nav) e counters/`ready` scoutados com
`resume-trace` write + `vf2probe --set-ip 0xa6c0`:

| phase | Como avançou | Observação |
| --- | --- | --- |
| 3 | countdown `0x500024` 256→0 (~1/frame) | nz_tex 0→4360 |
| 4→5 | scout `0x500024=1` | phase5 cd~2926 natural |
| 5→6→7 | scout cd=1 | — |
| 8→9→10 | scout `0x515b50=1` **e** `0x550000=0` | thunk/objeto; nz_tex→10154 |
| 10→11 | scout `0x500028=0x00030001` (mask=1, sel=3) | u32 em `0x500028` zera sel se descuidado |
| 11→12→13→14 | counters `[0x500834]+0x50` | task ptr medido `0x515b00` |
| **14** | **trava** | `0x550000` volta a **1** |

Em **nenhum** destes estados: tiles de logo SEGA, sel≠3 para TEST
(nav limpo), ou malha 3D nomeada.

## Gate `0x550000` (ready / video)

ROM (workers sel3 phase 8/9/14):

```text
ctr = *(u32*)(*(u32*)0x500834 + 0x50)
if ctr != 0: ctr--; ret
if *(u32*)0x550000 == 1: ret          ; espera
; else balx 0x9444  (inline text thunk)
```

Notes v0026 (`selector3_phase8_measurement`): no estado **natural
pós-phase7**, `0x550000 == 1` — o caminho zero-counter **retorna**
(sem avançar). O ramo not-ready (ctr=0 e ready≠1) mede **26 insns**
até `0x9444`.

Xrefs estáticos ao flag: **`0x4b414`, `0x4b83c`, `0x4ba14`**
(video command submit — note `video_command_submit_executable_v0026`).

No oracle desta campanha, após um frame em phase14 com ready
forçado a 0, o flag **volta a 1** e a phase **permanece 0x0e**.
Coerente com: worker/thunk/video re-armam `0x550000=1` e o
incremento de phase não completa no estado medido (ou depende de
outro handshake TGP/vídeo não modelado como “pronto”).

## Coli spin `0x224xx`

A partir da phase10+ o scheduler entra em corpo **`0x22490–0x224a8`**
(família `fa_coli` / objeto) e **para de visitar** `0xa6c0` sozinho.
`resume-trace` com IRQ de frame genérico **não** desgruda (0
frame-interrupts no spin). Recete útil: `--set-ip 0xa6c0` (probe,
`max-steps=1`) + `nav=0` por frame.

## FIFO / texture (consolidado)

- Attract: FIFO `0x884000` **1157–2016** writes; texture 64k nz
  **0→4360→~10154**.
- TEST (`sixth-fresh`): FIFO **9** writes; texture nz **0**.
- Tiles attract: vazios (sem ASCII SEGA/title).

## Fail-closed logo 3D

Fases **3–14** do attract sel3 foram **alcançadas no oracle** com
gates medidos; **não** apareceu malha/logo 3D único. Fase 14+ depende
do latch de vídeo `0x550000` (estado natural = 1 = espera). Sem
witness visual/packet nomeado, **não** se recupera C de logo.

Fronteiras irmãs (não misturar): `fa_coli` body 77 `0x22404`;
`phase17_zero` CC; microcódigo TGP (upload runtime).

## Tools

- `tools/python/attract_phase_step.py`
- `tools/python/dump_phase8_state.py`
- `tools/python/attract_long_campaign.py` (v0365)

## Validação

- Sem mudança de semântica recovery/executor em v0366 (tools+notes+docs).
- Focused CTest **7/7** observado em `2187955` (v0365); executor
  `teste` intacto desde `b611c82`.
