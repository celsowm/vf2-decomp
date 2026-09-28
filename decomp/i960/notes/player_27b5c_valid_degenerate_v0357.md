# v0357 — `0x270d4`/`0x27b5c` com ponteiros válidos vs degenerados

## Forma degenerada (fronteira v0352/v0356)

Parks sixth-fresh / punch / player-14288-punch: `F0+0x1a0=0`, `+0xbd8=0`.
Drive `0x4505` a partir daí chega a `0x270d4` com record/scratch nulos; o
ROM lê selector de `*(0)` e o `cvtri` em `0x27cc8` falha
(`unsupported instruction`) — **esperado**.

C agora **fail-closed**: `hybrid_execute_player_1428c` recusa
`record_pointer==0 || scratch_base==0`; `hybrid_execute_player_27b5c`
recusa `selector > 0x1fff`. Unit `test_player_27b5c_zero_record_fail_closed`
não admite 9726/9745 nessa forma.

## Forma válida (medida)

Parks `player-1428c-*` / `player-14288-natres|rt`:
`F0+0x1a0=0x0201c2fc`, `F0+0xbd8=0x00520000`.

```text
vf2probe --snapshot out/player-1428c-f0-s6.vf2snap \
  --set-ip 0x000270d4 --set-reg g7=0x00510980 \
  --until 0x0002712c
```

| Campo | Antes | Depois |
| --- | --- | --- |
| run_instructions | — | **9378** |
| IP final | `0x1428c` | **`0x2712c`** (ret) |
| g2 / g3 / g5 / g6 | — | `0xaf` / **`0x520630`** / `0x50ea98` / `0x50e2d0` |
| slot `+0x1e0..+0x5a0` em `0x520000` | zeros | floats/pacotes expandidos (ex. `0x44b605b0`, `0x4660382d`, …) |
| cvtri/stis tail | — | 36× `ld/cvtri/stis/cmpdeco` executados no oracle |

O bloco `0x1428c` C existente contabiliza **9726/9745** (inclui mais que o
wrapper `0x270d4`). O pin diferencial dos **cinco slots** com este record
ainda exige confrontar C vs oracle byte a byte no park `player-1428c-*` —
**não** inventado aqui; as contagens/cursors acima são witness medido.

## Parks locais

`park-270d4-valid.vf2snap`, `park-player-270d4*.vf2snap`,
`player-1428c-f0-s6.vf2snap`.
