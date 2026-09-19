# v0360 — Tela SEGA (imagem) alcançada: selector-0 warning

## Definição de witness

A “imagem SEGA” medida neste corredor **não** é tile ASCII `0x80xx` e **não**
é malha poligonal única na FIFO de geometria. É a **tela legal/assinatura do
Model 2** desenhada pelo worker do **frame selector 0**, cujas strings ROM
incluem:

```text
W A R N I N G
THIS GAME IS TO BE USED ONLY IN JAPAN.
...
SEGA ENTERPRISES,LTD.          @ maincpu 0x0000aaad
```

Blits via helper `0x00008918` para destinos tile-plane
`0x01000332, 0x01000618, …, 0x01001650` com **glyphs estilizados** (tabela
`0x02a69f02`, halfwords `glyph+0x8000` — na prática `0x89xx`), por isso
decoders `0x80xx` nunca viam “SEGA”.

## Mecanismo (ROM, selector 0 @ `0x0000a804`)

```text
r3 = *(u8*)(*(u32*)0x50016c + 0x3350)   ; COUNTRY assignment
if country != 0: force selector=2; ret
call 0x61198                            ; signature check 0x59cfe0
if g0 != 0: force selector=2; ret       ; g0=-1 se assinatura presente
; g0 == 0: caminho de DESENHO
  clear/rect 0x01004000 (0xc007c007)
  rect helper 0x8ef0
  9x text helper 0x8918 (strings ROM)
  countdown 0x500024 = 5<<7 = 640
  selector += 1                         ; 0 -> 1
ret
```

Helper `0x61198`: compara `0x59cfe0` com
`{0x52455320,0x4e4c2053,0x4e204544,0x20514555}`; se igual `g0=-1`; senão
`g0=0`; **sempre zera** as 4 palavras.

Selector **1** (`0xa974`) só decrementa o countdown e incrementa o selector
quando expira → caminho natural termina em selector 2 (e depois 3 → `0x11`
TEST MENU neste estado de config / warm com assinatura).

## Witness medido (oracle)

| Park / drive | sel antes | Assinatura | Resultado |
| --- | --- | --- | --- |
| `park-warm-attract` | 0 | **presente** | fast path **34** insns → sel **2** (pula SEGA) |
| `sixth-fresh` + force sel0 | 17→0 | zeros | draw path, sel **1**, glyphs nos destinos |
| **`park-after-irq` natural** | 0 | `0xa5a5a5a5` (não casa) | `resume-trace`→`0xa6c0`, 1 frame **15853** insns, sel **1**, countdown **640**, mesmos destinos glyph |

Contagem **15853** = `VF2_SELECTOR0_INSTRUCTIONS` já recuperada em
`native_runtime_condition_impl_base.c` (`execute_selector0_body`).

COUNTRY medido **0 (JAPAN)** em sixth/warm/after-irq — o pulo observado no
warm **não** é country; é a **assinatura já gravada** no warm-boot/test.

## Por que campanhas v0353–v0356 falharam

1. sixth / EXIT / warm com assinatura → selector 0 **pula** o desenho.
2. Tile decoders só `0x80xx`; glyphs SEGA são `0x89xx`.
3. Texture-ram / geometry FIFO **idênticos** TEST vs warm (hash iguais) —
   a logo legal **não** é textura nova nem malha FIFO única neste frame.
4. C já tinha o corpo do selector-0 draw; faltava unit pin do ramo sem
   assinatura + documentação do witness natural.

## C nativo

- `execute_selector0_body`: fast path 34 / draw path **15853**, 9 strings
  (inclui `0xaaad` SEGA), countdown 640, selector 0→1.
- Unit `test_frame_dispatch_selector0_sega_warning_draw` (ROM-independent):
  assinatura zero + COUNTRY 0 → 15853, sel=1, countdown=640.
- Unit existente `test_frame_dispatch_selector0_signature_fast_path`: 34/2.

Fail-closed: COUNTRY ≠ 0 ou assinatura presente → apenas o caminho medido
(34 → sel2) ou `UNSUPPORTED` se `g0` inesperado.

## Aberto (após a tela SEGA legal)

- Atrair de **jogo** / logo 3D poligonal: o corredor natural ainda vai
  sel 2 → 3 → **0x11 TEST MENU** neste estado (warm com assinatura).
  Logo de jogo continua fronteira (outro estado de máquina / demo).
- Pin diferencial byte-exact C vs oracle nos glyphs `0x89xx` (tabela ROM
  `0x02a69f02`) — opcional; witness natural já medido.

## Parks / tools (não git)

`park-after-irq`, `sega-natural-afterirq`, `sega-natural-frame`,
`sega-sig0-from-sixth`, `sega-warning-full`.
Tools: `tools/python/dump_sel0_strings.py`, `decode_glyph_tiles.py`,
`dump_display_witness.py`, `dump_deep_witness.py`.
