# v0350: fecho dos abertos restantes — live coli, parks player, endurance

## Verdict

| Aberto | Status |
| --- | --- |
| Drive live coli `g0=1→0x225cc` | **Fechado no midbody live**: park `coli-midbody-22210` + receita (`g13=0x514940`, `+0x1a4=0x100`, `+0x820=1`, `0x5149cc=0xffff`) alcança `call 0x22290` (129 passos) e completa até `0x10dcc` em **380/9/10** com **`0x225cc`×1**. |
| Parks player fora do punch10 | **Fechado**: `sixth-regen` e **`player-14288-fifth-rt`** completam `0x4505` em **1745/4/4** com o mesmo estado final do punch10. `boot` mede **1743**; `player-14288-natres` mede **1659** (shapes irmãs, fail-closed no C). |
| Endurance >5000 | **Fechado**: `native-nth-dispatch` **MATCH até 8000** neste build MSVC. |

## 1. Coli live `g0=1 → 0x225cc`

### Midbody park (live shape)

```text
vf2probe --snapshot out/coli-midbody-22210.vf2snap \
  --set-ip 0x00022210 \
  --set-reg g7=0x00510980 --set-reg g8=0x00512980 \
  --set-reg g13=0x00514940 \
  --set-u32 0x00510b24=0x100 \
  --set-u8  0x005111a0=0x01 \
  --set-u32 0x005149cc=0xffff \
  --until 0x00010dcc --max-steps 5000 --trace
```

| Alvo | Resultado |
| --- | --- |
| `--until 0x22290` | **129** passos, `ip=0x22290` (call `0x225cc`) |
| `--until 0x225cc` | **130** passos (entra no corpo do resolver) |
| `--until 0x10dcc` | **380** passos, **9** calls / **10** rets |

Call graph da forma live: `0x22298`×2, `0x22404`×2, **`0x225cc`×1**, `0x230d4`×1, `0x23238`×2, `0x1ab34`×1.

`g13=0x514940` é o valor que materializa `0x5149cc` como slot/exclude do contact sob o park midbody (o `0x514b80` das notas v0282 não reproduz o g0=1 a partir deste park).

### Task inteira a partir de `0x221e8`

- Mutações estáticas **sem** g13 live: continuam **9393/17/18**, sem `0x225cc` (v0349).
- `vf2probe --input 16` a partir de `sixth-regen` com as mesmas mutações: após 8M passos o countdown `0x500024` ainda é **639** e o slot coli `0x514980` está inativo — o arming PUNCH do coli não completa nesse modo de probe (diferente do loop de frames do `vf2cycles`).
- Park pós-call live: `out/coli-225cc-entry.vf2snap` (já com `+0x1a4=0x100`, `+0x820=1`, `0x5149cc=0xffff`).

### Pinos C

Inalterados nesta fatia (fail-closed fora deles):

- warm `9214/18/19`
- sibling `9528/18/19`
- sibling `9393/17/18`

A forma live midbody **380/9/10** é evidência de tail com `0x225cc`; um pin whole-task dela exigiria C produzir prefix+380 com o mesmo estado — não pinado sem essa prova.

## 2. Parks player `0x4505`

| Snapshot | F0 inicial | Resultado |
| --- | --- | --- |
| `punch10` / `-t6` / `-pf5` / `-type6` / `-t8` / `-t10` / `punch-b5test` | `0x04000000` | **1745/4/4** |
| **`sixth-regen`** | `0x04000000` | **1745/4/4** (fora do nome punch10) |
| **`player-14288-fifth-rt`** | `0x04000000` | **1745/4/4** (família `player-14288-*`) |
| `boot` | `0` | **1743/4/4** (shape irmão; final F0=`0x800`) |
| `player-14288-natres` | `0x84000002` | **1659/4/4** (bit 31 set; C ainda fail-closed nesse F0) |
| `player-14288-rt` / `-punch` / `-sixth` / `-fifth` / `fifth-b31*` | variados | fault `0x2705c` |

Estado final medido (shape 1745): `F0=0x04000400`, `+0x1a4=0x200`, `+0xbe4=0`.

**Conclusão B2:** a prova positiva ROM-backed **não** depende do arquivo `punch10.vf2snap`. `sixth-regen` e `player-14288-fifth-rt` reproduzem exatamente 1745/4/4. C mantém o pin 1745 e fail-closed em F0 bits 31/1/5/6/23/21 para `0x4505` (alinhado aos parks que ainda faultam).

Unit `test_player_19ef8_selector_4505`: controles negativos (F0 `0x80000002`, bit 5, selector `0x1234`) nunca produzem o corredor medido 1745/`0x1428c`.

## 3. Endurance

MSVC Debug, `vf2i960 native-nth-dispatch roms/vf2`:

| Alvo | Resultado |
| --- | --- |
| 5000 | MATCH (v0349) |
| **7500** | **MATCH** |
| **8000** | **MATCH** |

Sem fronteira dura até 8000 neste toolchain.

## Validação desta fatia

Observado:

- `vf2_native_runtime` unit: **PASS**
- CTest focado (runtime, dispatch 3/6/11/12): **6/6 PASS**
- CTest não-dispatch: **50/50 PASS**
- `vf2cycles --input 16 --cycles 8`: **8/8 MATCH**
- Endurance 7500 e 8000 MATCH observados

## Abertos restantes (estreitos)

- Arming PUNCH do coli via probe (ou `vf2cycles` + park) para pinar whole-task **prefix+380** com `g0=1`.
- Admitir shapes player irmãs medidas (`1743` boot, `1659` natres) com gates C correspondentes.
- Endurance >8000 neste toolchain.
