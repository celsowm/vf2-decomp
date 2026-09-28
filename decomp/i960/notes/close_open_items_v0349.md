# v0349: fechamento dos abertos — 0x4505, coli g0=1, endurance

## Verdict

| Aberto | Status |
| --- | --- |
| Prova `0x4505` fora de punch10 | **Concluída com limites**: punch10-t6/pf5/type6 reproduzem 1745; parks `player-14288-*` continuam fault `0x2705c`; C fail-closed nesses (gate F0 bits 31/1); unit C pinna 1745/4/4 + negativo. |
| Drive coli `g0=1→0x225cc` na task inteira | **Parcialmente fechado**: leaf `g0=1` e tail live 22404-e1→`0x225cc`→`0x10dcc` (346/6/8) medidos; whole-task estática a partir de `0x221e8` **não** reproduz `g0=1` (pinos 9393/17/18 e 9528/18/19). |
| Endurance >1000 | **MATCH até 5000** neste build MSVC. |

## 1. Player `0x4505`

### Referência ROM (fora do arquivo punch10.vf2snap)

| Snapshot | Resultado |
| --- | --- |
| `punch10.vf2snap` | OK 1745/4/4 |
| `punch10-t6.vf2snap` | OK 1745/4/4 |
| `punch10-pf5.vf2snap` | OK 1745/4/4 |
| `punch10-type6.vf2snap` | OK 1745/4/4 |
| `player-14288-punch/sixth/fifth` | fault `0x2705c` (~985–1024) |

Transplante de `F0=0x04000000` para `player-14288-rt` **não** corrige (ainda fault `0x2705c` em 985 passos). A degeneração não é só flags F0.

### C nativo

- Tabelas/`+0x1a8` com `selector & 0x1fff`.
- `0x4505`: `+0x1a4 ∈ {0, 0x20}`; F0 **não** pode ter bits 31,1,6,5,23,21 (park com `0x80000002` fail-closed, alinhado à fault de referência).
- Contagem **1745**/4/4.

### Unit `test_player_19ef8_selector_4505`

Fail-closed gates (this slice):

- F0 `0x80000002` (bit 31/1 — park que referencia-faulta) **não**
  produz o corredor medido 1745/`0x1428c`.
- F0 bit 5 set (forbidden no shape punch10) idem.
- Selector `0x1234` (não admitido) idem.

A prova positiva ROM-backed fica nos quatro snapshots punch10-family
(1745/4/4). O plant sintético completo de `player_selector` setup/
scratch não foi reconstruído no unit desta fatia.

## 2. Coli contact `g0=1` → `0x225cc`

### Provado

| Drive | Resultado |
| --- | --- |
| `coli-22404-e1` + receita v0282 → `0x225cc` | **96** passos, `ip=0x225cc` |
| mesmo drive → `0x10dcc` | **346** passos, **6** calls / **8** rets, call `0x225cc`×1, `0x230d4`×1, `0x23238`×2, `0x1ab34`×1 |
| unit contact `g0=1` (v0303) | 73/0/1, `g0=1` (já no repo) |

### Whole-task a partir de `0x221e8` (mutações estáticas)

Mutations `+0x1a4=0x100` e `+0x820=1` **sobrevivem** a `0x23524` (medido até `0x22404`: ambos ainda set). Ainda assim o drive de task inteira completa com:

| Shape | steps | calls | rets | `0x225cc` |
| --- | ---: | ---: | ---: | --- |
| warm | 9214 | 18 | 19 | no |
| bit8+scan1+board-clear (v0348) | 9528 | 18 | 19 | no |
| bit8+index1 da task entry (v0349) | **9393** | **17** | **18** | **no** |

O caminho `g0=1→0x225cc` exige o estado live do park `22404-e1` (g13/slots do drive real). Mutação estática na task entry não o reproduz.

Pinos C coli (fail-closed fora destes):

- `9214/18/19` warm
- `9528/18/19` sibling v0348
- `9393/17/18` sibling v0349

## 3. Endurance dispatch

MSVC Debug `native-nth-dispatch roms/vf2`:

| Alvo | Resultado |
| --- | --- |
| 1000 | MATCH |
| 2000 | MATCH |
| 3000 | MATCH |
| 3749 (histórico) | MATCH |
| 4000 | MATCH |
| 4500 | MATCH |
| **5000** | **MATCH** |

Sem fronteira dura até 5000 neste toolchain. Supera o endurance GCC anterior (3749).

## Validação (esta fatia)

Observado após build MSVC Debug:

- `vf2_native_runtime` unit: **PASS** (inclui fail-closed `0x4505`)
- CTest não-dispatch: **50/50 PASS**
- CTest dispatch 3/6/11/12: **4/4 PASS**
- `vf2cycles --input 16 --cycles 8`: **8/8 MATCH**
- Endurance `native-nth-dispatch` 1000–**5000** MATCH

## Abertos restantes (honestos)

- Drive **live** (não estático) de task coli que materialize `g0=1→0x225cc` do entry `0x221e8` — precisa de park com g13/slots corretos durante o midbody.
- Parks `player-14288-*` com floats válidos para corredor `0x505`/`0x4505` independentes do punch10.
- Endurance >5000 neste toolchain.
