# Targeted taint: state-8 `fa_game_info` entry branch dependencies (v0696)

First measured use of `tools/python/taint.py` (AGENTS.md next work #3)
beyond its self-test. Taint is sideband only and never influences the
oracle.

## Reproduction

```sh
python tools/python/trace_case.py out/state8-positive.json \
  --output /tmp/taint_trace.jsonl
python tools/python/taint.py --rom-dir roms/vf2 \
  --scenario out/state8-positive.json \
  --trace /tmp/taint_trace.jsonl
```

Scenario: `out/state8-positive.json` defaults (both selectors 8, both
flags 0, countdown 0, mode 0, threshold 0).

## Result (26 branches)

```text
branch 0x000164cc depends on:
  fighter0 + 0x0844 bit 5
  fighter1 + 0x0000 bit 5
  ; bbs 5, r15, 0x00016500
branch 0x000164f0 depends on:
  fighter0 + 0x0844
  fighter1 + 0x0000
  ; cmpobe 0, r3, 0x00016500
branch 0x000186bc depends on:
  fighter0 + 0x0000
  fighter1 + 0x0844
  ; cmpibg r9, r3, 0x00018728
branch 0x000186c0 depends on:
  fighter0 + 0x0000 bit 4
  fighter1 + 0x01a4 bit 4
  fighter1 + 0x0844 bit 4
  ; bbc 4, r7, 0x000186d4
branch 0x000186d4 depends on:
  fighter0 + 0x0000 bit 4
  fighter0 + 0x01a4 bit 4
  fighter1 + 0x0844 bit 4
  ; bbc 4, r8, 0x00018728
branch 0x00018730 depends on:
  fighter0 + 0x0000
  fighter1 + 0x0844
  ; cmpible r9, r3, 0x00018738
branch 0x00018748 depends on:
  fighter0 + 0x0000 bit 15
  fighter1 + 0x05b4 bit 15
  fighter1 + 0x0844 bit 15
  ; bbc 15, r4, 0x00018750
branch 0x00018760 depends on:
  fighter0 + 0x0000 bit 15
  fighter0 + 0x05b4 bit 15
  fighter1 + 0x0844 bit 15
  ; bbs 15, r4, 0x00018768
branch 0x00018770 depends on:
  fighter0 + 0x0000
  fighter1 + 0x0844
  ; cmpobne 0, r14, 0x00018890
branch 0x00018774 depends on:
  fighter0 + 0x0000 bit 4
  fighter1 + 0x01a4 bit 4
  fighter1 + 0x0844 bit 4
  ; bbs 4, r7, 0x00018890
branch 0x00018784 depends on:
  fighter0 + 0x0000
  fighter1 + 0x0844
  ; cmpobne 0, r3, 0x00018890
branch 0x000187ac depends on:
  fighter1 + 0x05b4
  ; bne 0x00018890
branch 0x000188a0 depends on:
  fighter1 + 0x05b4 bit 6
  ; bbc 6, r15, 0x000188ac
branch 0x000188ac depends on:
  fighter0 + 0x01a4 bit 15
  fighter1 + 0x05b4 bit 15
  ; bbs 15, r8, 0x000188cc
branch 0x000188b0 depends on:
  fighter0 + 0x01a4 bit 8
  fighter1 + 0x05b4 bit 8
  ; bbs 8, r8, 0x000188cc
branch 0x000188b4 depends on:
  fighter0 + 0x01a4 bit 16
  fighter1 + 0x05b4 bit 16
  ; bbs 16, r8, 0x000188c8
branch 0x000188b8 depends on:
  fighter0 + 0x01a4 bit 14
  fighter1 + 0x05b4 bit 14
  ; bbc 14, r8, 0x000188cc
branch 0x000188cc depends on:
  fighter0 + 0x01a4 bit 8
  fighter1 + 0x05b4 bit 8
  ; bbc 8, r8, 0x00018978
branch 0x00018984 depends on:
  fighter1 + 0x05b4
  ; cmpobe 0, r3, 0x00018998
branch 0x000189c0 depends on:
  fighter0 + 0x01a4 bit 14
  fighter0 + 0x0844 bit 14
  fighter1 + 0x0000 bit 14
  ; bbs 14, r8, 0x000189d4
branch 0x000189d0 depends on:
  fighter0 + 0x05b8 bit 1
  fighter0 + 0x0844 bit 1
  fighter1 + 0x0000 bit 1
  fighter1 + 0x05b8 bit 1
  ; bbs 1, r3, 0x00018a00
branch 0x000189ec depends on:
  fighter0 + 0x0000 bit 6
  fighter1 + 0x0844 bit 6
  ; bbc 6, r15, 0x000189f8
branch 0x000189fc depends on:
  fighter0 + 0x0000
  fighter1 + 0x0844
  ; cmpibl r13, r3, 0x00018a04
branch 0x00018a1c depends on:
  fighter0 + 0x0000 bit 21
  fighter0 + 0x01a4 bit 21
  fighter0 + 0x0844 bit 21
  fighter1 + 0x0000 bit 21
  fighter1 + 0x01a4 bit 21
  ; bbc 21, r12, 0x00018a24
branch 0x00018a2c depends on:
  fighter0 + 0x0844
  fighter1 + 0x0000
  ; cmpobne 22, r14, 0x00018a50
```

## Reads

- The `0x188b0`/`0x188cc` bit-8 dependencies confirm the mask-family
  branch structure the v0690 enumeration already encodes; no rule
  change follows.
- The `0x18a1c` bit-21 fan-in (five sources) versus the single-bit
  `0x188a0` bit-6 gate shows where taint discriminates compound from
  simple branch conditions for future positive-mask work.
- `+0x05b4`/`+0x05b8` appear as first-class branch sources
  alongside `+0x01a4`, supporting their candidate-field status.
- One zero-flag case only: dependencies may narrow under nonzero
  masks; repeat per mask family before using for gate design.
