# v0358 — Pin `0x270d4` five slots: medição e bloqueio fail-closed

## O que foi medido

Park `out/player-1428c-f0-s6.vf2snap` (record `F0+0x1a0=0x0201c2fc`,
scratch `F0+0xbd8=0x00520000`), drive oracle:

```text
vf2probe --snapshot out/player-1428c-f0-s6.vf2snap \
  --set-ip 0x000270d4 --set-reg g7=0x00510980 \
  --until 0x0002712c
```

Selectors u16 no record (main_data `0x0201c2fc`):

| offset | selector |
| --- | --- |
| +0x00 | `0x0505` |
| +0x02 | `0x0039` |
| +0x10 | `0x00f1` |
| +0x14 | `0x00e7` |
| +0x3e | `0x00af` |

Wrappers ROM: cinco `call 0x27b5c` com `g3=r8+{0x1e0,0x2d0,0x3c0,0x4b0,0x5a0}`.

## Executor `cmpobl` e o artefato 9378

O segundo laço de `0x27b5c` despacha status com:

```text
ldob (g5), r5
cmpobl 5, r5, 0x27c60
be   0x27c88
cmpobl 3, r5, 0x27c4c
be   0x27c40
cmpobl 1, r5, 0x27c38
```

`vf2_i960_run` **não** atualizava `compare_result` em COBR `cmpobl`/`cmpobe`
(o `be` seguinte lia flags stale). Com esse executor:

- span medido **9378** insns;
- payload nos slots começava “cedo” (ex. slot0 word0 = `0x44b605b0`).

Com CC arquitetural em COBR (fix experimental, depois **revertido**):

- span medido **9235** insns;
- slots esparsos (zeros + payload em offsets fixos);
- C `hybrid_execute_player_27b5c` **igualava** o oracle byte a byte nos
  cinco slots e cursors `g3=0x520630 g5=0x50ea98 g6=0x50e2d0`.

## Por que o fix não entrou em master

Atualizar `compare_result` em COBR quebra o MATCH do corredor aceito
(`fa_kill_osage` / `native-first-dispatch` / `phase17_zero` / bridges):
as recoveries C calibraram `hybrid_set_compare_result` no comportamento
stale do executor. Diferencial passa a falhar em `cpu-state` (IP igual,
`compare_result`/campos vizinhos divergem).

Política AGENTS: menor mudança comprovada; não enfraquecer validação;
fail-closed em fronteira desconhecida. Portanto:

- executor COBR **sem** escrita de CC (comportamento master atual);
- `0x270d4` em `vf2_hybrid_first_dispatch_task_execute` retorna
  **`VF2_ERROR_UNSUPPORTED`**;
- pin byte-exact dos cinco slots **não** é admitido;
- unit v0357 de record/scratch zero permanece.

## Reabertura (próxima alavanca)

1. Campanha dedicada: COBR `cmpo*` atualiza `compare_result` + revalidar
   pins de `compare_result` no corredor (kill_osage, phase17, game_info).
2. Depois, remontar o pin ROM-backed C vs oracle em `0x270d4→0x2712c`
   com as constantes **9235** / slots esparsos (não as 9378 stale).
3. Só então admitir o wrapper nativo.

## Parks / tools locais (não git)

`player-1428c-f0-s6.vf2snap`, `park-270d4-oracle-after*.vf2snap`,
`out/dump_oracle_270d4.py`, `out/dump_record_selectors.py`,
`out/emit_270d4_expected.py`.
