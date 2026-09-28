# v0363 — Referência MAME MB86233 / Model 2 (debug TGP)

## O que foi copiado

Em `third_party/` (**não** faz parte do recovery):

- `mame-mb86233/` — core + disassembler Fujitsu MB86233/86234
  (MAME `mame0289`, **BSD-3-Clause**);
- `mame-model2-ref/` — `model2.cpp`/`model2.h` (mapa copro/geo/TGP);
- `tools/python/tgp_disasm_mame.py` — listing de análise, não oráculo;
- `third_party/README.md` — política clean-room e mapa de uso.

## Fronteira clean-room

```text
MAME sugere → executor/oráculo VF2 mede → differential CTest prova
```

Não copiar handlers MAME para `src/recovered/`. Não linkar `third_party`
no runtime. Logo 3D / attract de jogo continua medido pelo **nosso**
oracle (v0362: gate `0xa748`, fronteira `teste` @ `0x19024`).

## Mapa Model 2 TGP (do driver MAME — hipótese de debug)

| Endereço i960 | Papel MAME | Nosso modelo |
| --- | --- | --- |
| `0x00980000` | copro ctl (bit31 = upload program) | observer/model2a |
| `0x00980008` | geo ctl | geometry FIFO control |
| `0x00880000` | function port (func em bits 23–28) | `vf2_tgp_write_function_port` |
| `0x00884000` | copro FIFO in/out | `vf2_tgp_read/write_*fifo*` |
| `0x00800000` | geo RAM / stream | buffer/geometry regions |
| copro bank `0x800000` | `copro_data` ROM | `vf2_tgp` banked read |
| copro bank `0x400000` | buffer RAM mask `0x7fff` | polygon/buffer RAM |
| IO sincos/atan/inv/isqrt | tabelas `copro_tgp_tables` | `vf2_tgp_read_*` em `tgp.c` |

Fórmulas sincos/atan/inv/isqrt em `src/hardware/tgp.c` **já espelham**
a família medida/MAME — útil para conferir tabelas, não para inventar
packets de attract.

## Programa TGP no ROM

`copro_data` extraído (`out/copro_data.bin`, 8 MiB) **não** é o
microcódigo de programa no offset 0 (início amostrado zeros); o programa
TGP do VF2 costuma ser **carregado em runtime** pelo i960 para a program
RAM (upload `0x980000`). Debug do logo 3D deve:

1. capturar o upload no oracle/probe (`--memory-trace` em `0x980000` /
   `0x880000` / `0x884000`) durante frames attract com nav limpo (v0362);
2. extrair as words do programa TGP;
3. disassemblar com `tgp_disasm_mame.py` (referência encoding);
4. correlacionar com geometry FIFO / texture-ram medidos no snapshot;
5. só então propor recovery C do que o **oráculo** provar.

## Próximo passo opcional (não executado nesta fatia)

- Instrumentar parks `attr-nav/navclear-*` para dump do buffer de
  programa TGP e FIFO `0x884000`;
- Continuar o attract i960 (fronteira `teste`) em paralelo — são
  fronteiras distintas (i960 attract vs TGP microcode).
