# third_party — referências externas (não é recovery)

Esta árvore contém **código de terceiros** usado só como **referência de
hardware / debug**. Não faz parte da recuperação clean-room do jogo e
**não deve ser linkado** em `src/recovered/`, no executor i960, nem no
portável final C17.

## Princípio (AGENTS.md)

```text
MAME sugere → nosso executor/oráculo mede → testes diferencial provam
```

Igual ao fluxo Ghidra → medida → C. Semântica de jogo continua vindo
**só** de evidência medida na ROM original.

## `mame-mb86233/`

| Arquivo | Origem MAME mame0289 |
| --- | --- |
| `mb86233.cpp` / `mb86233.h` | Fujitsu MB86233/86234 (TGP) core |
| `mb86233d.cpp` / `mb86233d.h` | Disassembler + encoding notes |

- URL: `https://github.com/mamedev/mame/tree/mame0289/src/devices/cpu/mb86233`
- Licença: **BSD-3-Clause** (cabeçalhos nos arquivos)
- Autores MAME: Olivier Galibert, et al.

### Por que ajuda o vf2-decomp

VF2 Model 2A usa **TGP MB86234** como coprocessador de geometria.
Nosso `src/hardware/tgp.c` + `include/vf2/tgp.h` modelam FIFO, bank,
tabelas sincos/atan/inv/isqrt e um executor de stream de geometria
**limitado** — não um MB86233 completo.

Referências úteis deste dump:

1. **Encoding** das instruções TGP (`mb86233d.cpp` comentários + `disassemble`).
2. **Bancos de RAM** internos (0x000–0x0ff / 0x200–0x3ff) e mapping Model 1/2.
3. **IO / RF**: fifo in `0x1`, fifo out `0x2`, bank reg `0x3`, gpio0 (atan compare).
4. **Espaço externo** no Model 2 (ver `mame-model2-ref/model2.cpp`):
   - program RAM compartilhada (upload via `0x00980000` bit31);
   - data `0x0000–0x00ff` / `0x0200–0x03ff`;
   - IO `sincos/atan/inv/isqrt` (mesma família de fórmulas já em `tgp.c`);
   - bank: `0x800000` → `copro_data` ROM, `0x400000` → buffer RAM `0x7fff`.
5. **Function port** `0x00880000`: empacota função em bits 23–28 + payload.
6. **FIFO** `0x00884000` (i960 ↔ TGP) — correlato do nosso observer/copro port.

### O que **não** fazer

- Não copiar handlers MAME para `src/recovered/` e chamar de recovered.
- Não tornar MAME dependência de runtime/CI.
- Não comitar ROMs, `.vf2snap` ou traces aqui.
- Não “admitir” caminho de attract/3D só porque o MAME desenha algo.

## `mame-model2-ref/`

Trechos do driver Model 2 (`model2.cpp` / `model2.h`, **BSD-3-Clause**)
para mapa de memória copro/geo/texture e boot do TGP. Driver MAME por
R. Belmont, Olivier Galibert, ElSemi, Angelo Salese, Matthew Daniels.

## Tools de análise (repo)

- `tools/python/tgp_disasm_mame.py` — disassembler TGP **análise-only**
  inspirado no encoding do `mb86233d.cpp`, sobre `copro_data`/program
  extraído. Não substitui o oráculo.

## Build

`CMakeLists.txt` **não** inclui `third_party/`. Se um dia um tool de
debug precisar compilar algo daqui, faça-o fora do alvo de recovery e
mantenha fail-closed o caminho de jogo.
