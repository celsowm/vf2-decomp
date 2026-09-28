# v0356 — attract/game-assign e fronteira player `0x27cc8`

## Attract / GAME ASSIGNMENT (medido, negativo)

- Backup `0x01d03340..` no sixth já está em defaults factory-like
  (`0x01010102` match-count etc.; notes index4/9).
- `vf2cycles` COIN+START `0x180` **12c MATCH** → permanece TEST MENU.
- PUNCH leva a EXIT TEST MODE e o countdown expira de volta a TEST MENU
  (v0355, 332c MATCH). Não há tile `SEGA`.
- Forçar selector `3` + flags `0x80002000` a partir do sixth **não** produz
  tela de attract em um frame de probe (100k passos, residual de fase).

Conclusão: com este estado de config/backup o oráculo **não** sai do menu
operador para attract/logo. Fronteira Sega permanece fail-closed.

## Player corridor pós-`0x4505` até `0x27cc8` (medido)

Drive padrão a partir de `out/sixth-fresh.vf2snap`:

```text
vf2probe --set-ip 0x00014288 --set-reg g0=0x4505 --set-reg g7=0x00510980 \
  --set-u32 0x00510b24=0x20 --set-u32 0x00500804=0x00510980
```

| Trecho | run_instructions | IP final |
| --- | ---: | --- |
| `0x14288` → `0x270d4` | **1749** | `0x000270d4` |
| park `0x270d4` → `0x27cc8` | **1709** | `0x00027cc8` (`cvtri r13,r13`) |
| soma (trace v0352 `player-14288-14310`) | **3458** | unsupported em `0x27cc8` no run longo |

`0x270d4` é o wrapper de **5 calls** a `0x27b5c`:

| call | `g0` (ldos) | `g3` dest |
| --- | --- | --- |
| `0x270e8` | `*(r9+0)` | `scratch+0x1e0` |
| `0x270f8` | `*(r9+2)` | `+0x2d0` |
| `0x27108` | `*(r9+0x10)` | `+0x3c0` |
| `0x27118` | `*(r9+0x14)` | `+0x4b0` |
| `0x27128` | `*(r9+0x3e)` | `+0x5a0` |

com `r9=*(g7+0x1a0)`, `r8=*(g7+0xbd8)`, ret `0x2712c`.

O corpo `0x27b5c` **já** tem C (`hybrid_execute_player_27b5c`) no corredor
`0x1428c` (status bytes + rewind 240 + 36× `cvtri/stis` via
`hybrid_player_convert_real_to_integer`). O executor de referência também
implementa `cvtri` (`executor.c`); `!isfinite` / overflow → UNSUPPORTED.

O run longo do v0352 falha em `0x27cc8` porque o **caminho de referência**
executa o i960 real de `0x27b5c` com o estado live (não o helper C já pinado
do `0x1428c`). O próximo recovery C é admitir o wrapper **`0x270d4`**
(mesma estrutura de 5 slots) reaproveitando o helper, com pin diferencial no
park `0x270d4` — **não** fechado nesta fatia por falta de witness de estado
final completo dos 5 slots no drive live.

Hot mem v0352 confirmada no trace: `0x26f24` R, `0x26fb8` R, `0x27b8c` R,
`0x27c10` R, `0x26f48` W, `0x27bb0` W.

## Parks locais

`park-player-270d4.vf2snap`, `park-attr-sel3.vf2snap`, traces/jsonl v0352.
