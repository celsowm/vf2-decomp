# v0351: Combate Vivo — Fases A–E

## Verdict

| Item | Status |
| --- | --- |
| Arming coli via PUNCH | **Fechado**: `native-sixth-dispatch` → `out/sixth-fresh.vf2snap`; `vf2cycles --input 16` 30+300 ciclos MATCH; park `out/coli-arm-fresh-c330.vf2snap` com slot10 `flags=0x80000000`, `entry=0x000221e8`, countdown `0x00500024=0`. |
| Midbody live `g0=1→0x225cc` | **Medido**: 380 passos, 12 call-instr / 10 rets, `0x225cc`×1 longo (249 até `0x22294`). |
| C `0x22298` live sibling | **Nativo**: bit8 set + bit1 clear + gates medidos → body **13**, `stos 0` em `g7+0x6dc`. Unit positivo + 3 negativos fail-closed. |
| Pin whole-task prefix+380 | **Não pinado** (falta drive C que reproduza prefixo+corpo com `g0=1` após arming; probe não reentra o corpo no mesmo parque). |
| frontier / layout | **Estendido**: `--fighter-base` em `frontier.py` + `tools/python/fighter_offsets.py`; `+0x0026` bilateral em `fighter_candidate.h`. |
| Player shapes irmãos | **Fail-closed C** (boot 1743 / natres 1659 seguem fora); medidas suplementares de probe não são pin controlado. |
| Endurance | **MATCH observado até dispatch 9626+** (timeout 600s ainda MATCH). |
| Gates | CTest Debug **57/57 PASS**; unit runtime **PASS**; `vf2cycles --input 16 --cycles 8` **8/8 MATCH**; `test_frontier.py` **PASS**. |

## 1. Arming PUNCH do coli

```text
build/Debug/vf2i960.exe native-sixth-dispatch roms/vf2 out/sixth-fresh.vf2snap   # MATCH
build/Debug/vf2cycles.exe --rom-dir roms/vf2 --snapshot out/sixth-fresh.vf2snap \
  --cycles 30 --input 16 ... --output-snapshot out/coli-arm-fresh-c30.vf2snap   # MATCH, countdown 292
build/Debug/vf2cycles.exe --snapshot out/coli-arm-fresh-c30.vf2snap \
  --cycles 300 --input 16 ... --output-snapshot out/coli-arm-fresh-c330.vf2snap # MATCH
```

Park c330 medido:

| Campo | Valor |
| --- | --- |
| `0x514980` flags | `0x80000000` |
| `0x51498c` entry | `0x000221e8` |
| `0x500024` countdown | `0` |
| `0x508000` | `0x00008a00` |
| fighters | `0x510980` / `0x512980`, F0=`0x04000000` |

`vf2probe --input 16` a partir do sixth **não** arma (countdown congelado); o arming exige o loop de frames do `vf2cycles`. Após arming, o probe não reentra `0x221e8` em 5000 passos (permanece em frame-wait `0x10fa8`); o corpo live continua sendo o park midbody.

## 2. Live midbody `g0=1` (evidência)

Receita: `g7=0x510980`, `g8=0x512980`, `g13=0x514940`, `f0+0x1a4=0x100`, `f0+0x820=1`, `0x5149cc=0xffff`.

| Alvo | Resultado |
| --- | --- |
| `--until 0x22290` | 129 passos (call `0x225cc`) |
| `--until 0x10dcc` | **380** passos, final ok |
| call-instr | **12** (inclui `bal` `0x225bc`/`0x223bc`) |
| rets | **10** |

Call graph: `0x22298`×2, `0x22404`×2, `0x225bc`×2, `0x223bc`×1, `0x225cc`×1, `0x230d4`×1, `0x23238`×2, `0x1ab34`×1.

Spans: `0x22404#1` **78** (hit+helpers), `#2` **14** (warm), `0x225cc` **249** até `0x22294` (alinha com corpo longo v0314).

Segundo `0x22298` (medido no trace):

```text
bbc 8 not-taken → bbs 1 not-taken → bbc 14 g7+0x1a4 taken → 0x22320
ldos g7+0x61c==0; bbs 14 not-taken; r6=g8+0x821 ∉ {6,5,2}
cmpibne 2 taken → stos r11(=0), g7+0x6dc; ret
body 13 antes do ret
```

## 3. Recovery C

`coli_22298_body` (hybrid.c) admite o sibling live:

- flags `g8+0x1a4` bit8 set, bit1 clear;
- `g7+0x1a4` bit14 **clear**;
- `g7+0x61c == 0`;
- `g8+0x821 ∉ {2,5,6}`;
- efeito: `stos 0` em `g7+0x6dc`, body **13**.

Irmãos fail-closed: bit14 set (loop float `0x222b4`), `+0x61c!=0`, scan em `{2,5,6}` (`0x22338`/`0x2233c`).

Unit `test_coli_bitmask_22298_early_path`:

- warm body 6+ret=7 (inalterado);
- live sibling **14**/1 ret, store 0;
- três negativos com poison `0xbeef` preservado.

Pinos coli whole-task C **inalterados**: warm `9214/18/19`, `9528/18/19`, `9393/17/18`. Forma live midbody **não** vira pin whole-task nesta fatia.

## 4. Tooling / layout

- `frontier.py`: `--fighter-base` (repetível) + `--fighter-window`; correlaciona `--memory-trace` a offsets fighter-relativos; relatório `fighter-relative offsets`.
- `tools/python/fighter_offsets.py`: ranking standalone (base_count, R/W, larguras).
- Live coli bilateral (bases `0x510980`/`0x512980`): `+0x01a4`, `+0x0821`, `+0x0026`, `+0x06dc`, `+0x0000`, `+0x0004`, `+0x01a8`. Unilaterais: `+0x0828`, `+0x1234`, `+0x06d4`, `+0x06d8`, `+0x0700`, `+0x082a`, `+0x019f`, `+0x01aa`, …
- `fighter_candidate.h`: adicionado `VF2_FIGHTER_OFF_0026` / largura 2 (evidência bilateral live). Nomes continuam neutros.

## 5. Player (Fase C) — fail-closed

| Park | Probe set-ip `0x14288` |
| --- | --- |
| punch10 | F0=`0x04000000` no park; drive curto não reproduz 1745 sem o shape completo |
| boot | fault memória (~242 passos) neste drive |
| natres | completa até `0x1428c` em **4398** passos (não é o pin medido 1659); F0 final `0x84000002` |

C mantém pin **1745** punch10/sixth-regen/fifth-rt e fail-closed em F0 bits 31/1/5/6/23/21. Shapes 1743/1659 **não** admitidos sem witness controlado.

## 6. Validação observada

- `vf2_native_runtime_tests`: **passed**
- `ctest --test-dir build -C Debug`: **57/57 passed**
- `vf2cycles --input 16 --cycles 8` (sixth-fresh): **8/8 MATCH** (296 blocos / 46,755 insns)
- `native-nth-dispatch 10000`: MATCH contínuo observado pelo menos até **dispatch 9626** (comando expirou em 600s ainda em MATCH; sem fronteira dura até onde foi executado)
- `tools/python/test_frontier.py`: **all passed** (duckdb/parquet skip por dep ausente)

## 7. Abertos restantes

1. Drive C/ROM do corpo coli **após** arming (scheduler → `0x221e8` com receita live) para pinar prefix+380 se o híbrido reproduzir.
2. Recuperar spans `0x22404` hit live (78) e conferir se `coli_22404_body` v0303 cobre a forma com `bal 0x225bc`/`0x223bc` (hoje pinos unitários são shapes compactos).
3. Admitir player boot **1743** / natres **1659** só com witness completo.
4. Endurance além de ~9626 neste toolchain (rodar com timeout maior).
5. Frontiers de geometria player pós-`0x28780` com traces punch10/sixth (Fase D incompleta — ranking live coli está dentro de ranges recovered).
