# v0368 — Pin oracle do final-status `0x4bf90` / clear `0x4bfc4`

## Receita (probe, park phase14 `s04-frame`)

```text
vf2probe --set-ip 0x0004bf90 --until 0x0004bfdc --max-steps 200 \
  --set-u32 counters/ready ...
```

| Condição inicial | passos até `0x4bfdc` | `0x550000` final |
| --- | ---: | --- |
| ctr0=ctr1=ctr2=**0**, ready=1 | **12** | **0** (clear) |
| ctr0=0, ctr1=0, ctr2=**1**, ready=1 | **10** | **1** (mantém) |
| ctr0=**1**, ready=1 | **6** | **1** (mantém) |

Board medido `0x00508000=0x00008a00` → **bit 9 set** → ROM `bbs 9`
pula `call 0x4d25c` e cai no `ret` em `0x4bfdc`.

## ROM `0x4bf90` (medido)

```text
mov 0, r3
stos r3, 0x0055c2f0          ; status u16 = 0
ld 0x5502c0, r14 / cmpobne 0 → 0x4bfcc
ld 0x5502d0, r14 / cmpobne 0 → 0x4bfcc
ld 0x5502e0, r14 / cmpobne 0 → 0x4bfcc
mov 0, r3
st r3, 0x00550000            ; clear ready
ld 0x508000, r15
bbs 9, r15, 0x4bfdc
call 0x4d25c
ret
```

Entrada medida no corredor texture: status scan `0x4bd24` → todos os
records inativos → `b 0x4bf90` (note `texture_record_scan_v0024` /
`texture_orchestrator_v0023.csv`).

## C `execute_texture_final_status_call`

Já pinado em `tests/analysis/test_orchestrator_bridge.c`:

- all-zero + bit9: `recovered_instruction_count == 13`,
  `procedure_returns == 1`, ready **0**, status u16 **0**;
- contagem **13** = 12 passos do oracle até `ret` **+** execução do `ret`.
- ctr0≠0 + !bit9: caminho `call 0x4d25c` (unit `test_final_status_first_counter_call`).

`VF2_TEXTURE_RUNTIME_FLAGS == 0x00508000` (mesmo endereço do ROM).

## Depois do clear medido no attract

Park `all0-ready1` (ready=0 pinado via `0x4bf90`) → frames com
`0x500704=0`: **permanece sel=3 / phase=0x0e**; spin de objeto
`0x4c7xx`; nz_tex ~10154; tiles vazios. **Phase15+ não avança** —
worker phase14 não grava `phase+1` (v0366/v0367).

`model2a`/`tgp` continuam **sem** auto-clear de `0x550000` na
conclusão de vídeo; o clear é só via i960 `0x4bf90`/`0x4bfc4` (ou C
bridge equivalente) com contadores 0.

## Fail-closed logo 3D

Mesmo com latch medido/clearável, **não** há malha/logo 3D nomeada
(tiles vazios; FIFO sem packet→objeto). Não se recupera C de logo.

## Validação

- Unit final-status existentes permanecem o pin C (13 insns com ret).
- Focused CTest native/attract: **7/7** observado em v0365–v0367.
- CTest orchestrator/texture: ver saída no commit desta fatia.
- Sem mudança de semântica recovery/executor em v0368 (tools+notes+docs)
  **salvo** se a unit orchestrator reprovar — aí corrigir C para o pin
  medido, não o contrário.
