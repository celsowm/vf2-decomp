# v0352: fecho dos abertos Combate Vivo

## Verdict

| Aberto | Status |
| --- | --- |
| Coli pós-arming → corpo | **Medido**: `native-resume` de `coli-arm-fresh-c330` alcança `0x000221e8` (40 blocos / 2151 insns). Task inteira `0x221e8→0x10dcc` com receita live: **9398** passos / **17** calls / **18** rets, **sem** `0x225cc`. Pin C whole-task **não** expandido (9398 fora de 9214/9528/9393 → fail-closed). |
| Span `0x22404` live hit | **Medido**: **78** passos até `ret 0x225b0` (body 77 + ret). Gates: `f0+0x1a4=0x00010000` no momento do query (bit16), `+0x820=1`, `+0x5b8=0`, `+0x1aa/+0x808=0`, slot `0`, exclude `0xffff`, ROM mask `0x2007ace`, FIFO `0x884000`, stores `f1+0x6d4/+0x65c..+0x664`. Diferente do shape unit v0303 (body 72) — **não** pinado em C nesta fatia. |
| Player boot **1743** / natres **1659** | **Reproduzidos** com drive padrão `g0=0x4505`, `g7=0x510980`. C agora seleciona contagem por F0 bit26: set → **1745**, clear → **1743**. natres F0 bit31 permanece fail-closed. |
| Endurance | **MATCH observado até dispatch 10675** (arquivo `out/endurance-11000.txt` truncado em MATCH; sem fronteira dura). |
| Frontier geometria player | **Fronteira medida**: após corredor `0x4505`, unsupported final em **`0x00027cc8`**; call `0x270e8→0x27b5c`; hot mem `0x26f24/0x26fb8/0x27b8c/0x27c10`. |

## 1. Drive player (padrão v0348)

```text
vf2probe --snapshot <park> --set-ip 0x00014288 \
  --set-reg g0=0x4505 --set-reg g7=0x00510980 \
  --set-u32 0x00510b24=<0x20|0x0> --set-u32 0x00500804=0x00510980 \
  --until 0x0001428c --max-steps 5000
```

| Park | F0 in / +0x1a4 | run | finals (LE) |
| --- | --- | --- | --- |
| punch10 / sixth-regen / fifth-rt | `0x04000000` / `0x20` | **1745**/4/4 | F0=`0x04000800`, `+0x1a4=0x200`, `+0x1a8=0x10505`, `+0xbe4=0` |
| boot | `0` / `0` | **1743**/4/4 | F0=`0x800`, `+0x1a4=0x200`, `+0x1a8=0x10505`, `+0xbe4=0` |
| natres | `0x84000002` / `0x20` | **1659**/4/4 | F0=`0x84000882` (bit31 retido), `+0x1a4=0x200`, `+0xbe4` float ≠ 0 |
| punch10 F0 forçado `0` | `0` / `0x20` | **1743** | (regra bit26) |
| punch10 F0 forçado `0x200` | bit9 / `0x20` | **1743** | (bit26 clear) |
| punch10 F0 `0x04000000` | bit26 / `0x20` | **1745** | |

Regra C (`hybrid.c` selector `0x4505`):

- gates existentes (F0 forbid bits 31/1/5/6/23/21, `+0x1a4∈{0,0x20}`, tabelas, …) **mantidos**;
- `player_flags & (1<<26)` → `executed += 1745`;
- senão → `executed += 1743`;
- natres (bit31) continua **UNSUPPORTED**.

Unit `test_player_19ef8_selector_4505`: negativos ainda nunca produzem **1745** para F0 proibido.

## 2. Coli task inteira com receita

`native-resume roms/vf2 out/coli-arm-fresh-c330.vf2snap 64 0 0x000221e8 out/coli-parked-221e8.vf2snap`
→ `blocks=40 instructions=2151 entry=0x1645c exit=0x221e8`.

Do park `0x221e8` + receita (`+0x1a4=0x100`, `+0x820=1`, `0x5149cc=0xffff`, `g13=0x514940`):

- **9398** passos até `0x10dcc`;
- call graph: shell `0x23524` + filhos + `0x22298`×2 + `0x22404`×2 + `bal 0x225bc`×2 + `0x223bc`×1;
- **`reached_225cc = false`**.

Conclusão: a receita live de midbody **não** sobrevive ao shell `0x23524` quando a task é executada a partir da entrada. O caminho longo `0x225cc` segue sendo shape de park midbody (`0x22210`), medido em 380 passos (v0351).

Pinos C coli whole-task inalterados: `9214/18/19`, `9528/18/19`, `9393/17/18`. Forma **9398/17/18** fica documentada como sibling fail-closed.

## 3. Frontier player pós-`0x4505`

Trace `out/player-14288-14310.jsonl` (punch10, drive padrão, até `0x14310`):

- **3458** passos, **709** memory records;
- final: `unsupported operation` em **`0x00027cc8`**;
- calls: `0x14288→0x19ef8`, `0x1a1e4`, `0x26ef0`, `0x27130`, `0x14298→0x270d4`, `0x270e8→0x27b5c`;
- hot mem IPs: `0x26f24` R, `0x26fb8` R, `0x27b8c` R, `0x27c10` R, `0x26f48` W, `0x27bb0` W.

Próximo alvo de recovery nativo: helper/corpo em torno de **`0x27b5c` → `0x27cc8`** (medido, não inventado).

## 4. Validação observada

- `vf2_native_runtime_tests`: **passed**
- `ctest --test-dir build -C Debug`: **57/57 passed**
- `vf2cycles --input 16 --cycles 8` (sixth-fresh): **8/8 MATCH**
- `native-nth-dispatch`: MATCH contínuo até **dispatch 10675** (timeout com arquivo ainda em MATCH)
- Drive player: 1745×3 parks, 1743 boot, 1659 natres — todos com `run_instructions` observados

## 5. O que permanece aberto (próxima alavanca)

1. Recuperar `0x27b5c`/`0x27cc8` player (fronteira medida do corredor `0x4505`).
2. Pin C da forma coli **9398** apenas se o híbrido reproduzir exatamente esse sibling.
3. Estender `coli_22404_body` para o hit live body **77** (gates da §2/span) e só então tentar pin midbody live 380.
4. natres **1659**: admitir em C só com gates bit31 + contagem 1659 + estado final medido se o corpo C for comprovado igual.
5. Endurance além de ~10675 neste toolchain.
