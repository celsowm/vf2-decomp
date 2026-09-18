# v0348: itens 1–4 — player 0x4505, site-B reachability, coli pin, endurance

## Verdict

| Item | Result |
| --- | --- |
| 1. Tail player live-valid | **`0x4505` nativo admitido** (medido punch10, 1745/4/4). Warm `0x505`/`0x284` ainda falham. |
| 2. Site-B-only whole-task | **Inalcançável sem mutar board** entre cascade e `0x22dd4` (ROM não escreve `0x508000` no braço). Gate fica no span medido. |
| 3. Pin coli não-warm | Forma de referência **9528/18/19** pinada junto de 9214/18/19. Sem `0x225cc` nesse drive. |
| 4. Endurance >40 | **MATCH até dispatch 1000** neste build. |

## 1. Player `0x19ef8` selector `0x4505`

### Medição (punch10)

```text
vf2probe --snapshot out/punch10.vf2snap \
  --set-ip 0x00014288 --set-reg g0=0x4505 --set-reg g7=0x00510980 \
  --set-u32 0x00510b24=0x20 --until 0x0001428c --max-steps 4000
```

- **OK**, **1745** passos, `0x14288 → 0x19ef8 → … → 0x1428c`.
- Mesmo caminho com `+0x1a4 = 0` (também 1745).
- `0x505` e `0x284` no mesmo park **falham** em `0x287A0` (1781/1889 passos).

### Grafo de chamadas medido (0x4505)

```text
0x14288 call 0x19ef8
  prologue bbc/clrbit×3/mov/stib  (v0342, bit-14)
  six-bbc 0x19f98…0x1a018 not-taken
  call 0x1a1e4 (setup) → bx/ret
  call 0x26ef0 (scratch) → ret 0x270d0
  call 0x27130 (clear/return) → ret
  bbs/bbc → 0x1a134 ret → 0x1428c
```

Calls/rets: **4 / 4**. Não visita `0x287a0` nem `0x2705c`.

### C nativo (esta fatia)

`hybrid_execute_player_19ef8`:

- admite selector `0x4505` além de `0x505`/`0x284`;
- tabelas e `+0x1a8` usam `selector & 0x1fff` (ROM mascara em `0x1a034`);
- `+0x1a4` para `0x4505`: `0` ou `bit 5` (`0x20`) — demais bits fail-closed;
- bit 14 do selector não é mais rejeitado quando `== 0x4505`;
- contagem final `0x4505`: **1745** (referência punch10); `0x505` mantém 1652; `0x284` mantém 1805.

Ainda **não** há prova ROM-backed whole-task PUNCH para `0x4505` (punch10 é park de referência via probe, não o corredor 320/320). A admissão é fail-closed fora da forma medida.

## 2. Site-B-only whole-task

Cascade `0x2292c`: board bit 9 **clear** → site A (continuação já inclui site B).
Board bit 9 **set** → `0x2298c` (sem site A).

Disasm `0x2298c→0x22b18`: **nenhum store em `0x508000`**. Portanto o braço bit-14 em `0x22dd4` re-lê o mesmo board: set → skip site B; clear já teria tomado site A.

Conclusão medida: composição site-B-only (board clear em `0x22dd4` sem site A) **não é atingível** num drive ROM sem mutação de board entre as duas leituras. O gate v0347 permanece com base no span probe (237 passos) + helpers provados.

Drive tentado (coli-fail, bit8 + scan=1 + board `0x8800`): completa a task coli, mas **não** executa `0x225cc` (contatos ainda 0).

## 3. Pin hybrid coli não-warm

### Forma medida (referência)

```text
vf2probe --snapshot out/coli-fail.vf2snap \
  --set-ip 0x000221e8 \
  --set-u32 0x00510b24=0x100 --set-u32 0x00512b24=0x100 \
  --set-u8  0x005111a2=1 --set-u32 0x00508000=0x8800 \
  --set-u8  0x005111a0=1 --set-u32 0x005149cc=0xffff \
  --until 0x00010dcc --max-steps 25000 --trace
```

| Shape | steps | calls | rets | 0x225cc |
| --- | ---: | ---: | ---: | --- |
| warm (sem mutação) | 9214 | 18 | 19 | no |
| sibling bit8+scan1+board-clear | **9528** | **18** | **19** | no |

Mesmo grafo de chamadas que o warm (`0x23524`, `0x22298`×2, `0x22404`×2, folhas `0x23878`×6 etc.); +314 instruções vêm de ramos internos mais longos, não de `0x225cc`.

### Pin C

`hybrid` coli task dispatcher:

- pin quente inalterado: **9214 / 18 / 19**;
- pin adicional medido: **9528 / 18 / 19** (só essa forma);
- qualquer outra contagem → `VF2_ERROR_UNSUPPORTED`.

O pin extra só aceita se o C nativo **produzir** exatamente 9528/18/19; caso contrário continua fail-closed (comportamento desejado).

Contact `g0=1` (v0303, body 72) + long-body `0x225cc` continuam abertos como drive próprio.

## 4. Endurance dispatch >40

MSVC Debug, `roms/vf2`, `vf2i960 native-nth-dispatch`:

| Alvo | Resultado |
| --- | --- |
| 40 | MATCH |
| 100 | MATCH |
| 200 | MATCH |
| 500 | MATCH |
| **1000** | **MATCH** |

Sem fronteira dura até 1000 neste build. Endurance histórico já chegou a 3749 (GCC, nota anterior).

## Validação desta fatia

Observado após build MSVC Debug (`VF2_ROM_DIR=roms/vf2`):

- CTest não-dispatch: **50/50 PASS**
- CTest focado (runtime, player, dispatch 1/2/3/6/11/12): **12/12 PASS**
- `vf2cycles --rom-dir roms/vf2 --snapshot scratch-sixth.vf2snap
  --cycles 8 --input 16`: **8/8 MATCH** (296 blocos / 17.340 insns)
- Endurance `native-nth-dispatch` 40/100/200/500/**1000** MATCH

## Abertos

- Prova ROM-backed whole-task / unit C para `0x4505` fora do park punch10.
- Drive contact `g0=1` → `0x225cc` em task coli inteira + pin dessa forma.
- Site-B-only só se surgir evidência de escrita em `0x508000` no braço.
- Endurance >1000 neste toolchain.
