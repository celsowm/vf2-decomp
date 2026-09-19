# v0364 — Executor COBR `teste` + attract pós-`0x19024` + tráfego TGP

## Executor (semântica arquitetural i960)

COBR `op 0x20..0x27` (`testno/testg/teste/testge/testl/testne/testle/testo`)
tem **um operando registrador** (`decoder.c`: `operand_count=1`, não é branch).

Implementação em `src/i960/executor.c`:

```text
dest = condition_matches(b*) ? 0xffffffff : 0
```

mapeando `teste→be`, `testg→bg`, `testne→bne`, etc.

Unit: `tests/i960/test_executor.c` (códigos ROM `0x22780000` = `teste r15`).
`vf2_tests`: **All vf2-decomp tests passed.**
Focused CTest após rebuild: **7/7** (`native_runtime*`, `player_270d4`,
`first_dispatch*`, `native_sixth_dispatch`) — **sem regressão de MATCH**.

## Attract com nav limpo (continuação v0362)

`resume-trace` com `0x500704=0` a cada frame a partir de
`attr-nav/navclear-f01` (sel=3, phase=0):

| Frame | sel | phase3 | cd | tex0 nz64k | TEST? |
| --- | --- | --- | --- | --- | --- |
| f00 | 03 | 03 | 256 | 0 | não |
| f01–f02 | 03 | 03 | 256 | 0 | não |
| long (8M insns) | 03 | 03 | **253** | **4360** | não |

- Oráculo **não** para mais em `teste @ 0x19024` (8M steps sem halt
  unsupported até budget).
- Phase3 permanece (countdown em `0x500024` decrementa lentamente).
- Tiles 0x80xx/0x89xx continuam vazios neste trecho (sem texto TEST).
- **Witness parcial:** texture-ram first-64k **nz=4360** no park long
  vs **0** em `sixth-fresh` (TEST) e nos frames curtos de attract.
  Hash `tex0` muda entre frames (`898e474d` → `4a7b44b1` → `8a9f4972`).
- Geometria: ~40–41 words nz, hash muda; **não** é ainda witness de
  malha SEGA única (sem decodificação de mesh).

## Tráfego TGP/geo medido (memory-trace)

Park `navclear-f01` → `--until 0x19024` com nav=0:

- **1157 writes** em `0x00884000` (copro FIFO);
- function codes `(val>>23)&0x3f` mais frequentes:
  `0, 41, 57, 60, 63, 19, 6, 61, 1, 15, …`;
- words com cara de float IEEE e pacotes estilo `0xNNNN80xx`;
- writes também em geo `0x00800000` (contadores).

Isto **prova** que o attract de fase 3 alimenta o TGP/FIFO — não é o
corredor parado de TEST. Ainda **não** prova malha 3D SEGA (logo):
falta correlação packet→objeto medida e/ou frame visual único.

Tools: `tools/python/trace_tgp_traffic.py`,
`analyze_attract_witness.py`.

## Fail-closed

- Não se nomeia campo/objeto de logo sem evidência.
- Não se copia MAME para `src/recovered/`.
- `teste` é semântica **ISA**; qualquer recovery de jogo continua
  oracle-first.

## Validação observada

- `build/Debug/vf2_tests.exe` — all passed (inclui unit `teste`).
- `ctest -C Debug -R "native_runtime|native_sixth|player_270d4|first_dispatch"` — **7/7**.
- Parks locais: `out/attr-nav/postteste-*`, `tgp-f01.jsonl` (não git).
