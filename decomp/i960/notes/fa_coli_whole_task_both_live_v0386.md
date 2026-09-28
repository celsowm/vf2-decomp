# fa_coli whole-task both-live 9528 (v0386)

Parked snapshot `out/coli-parked-221e8.vf2snap` at `0x221e8` (coli task entry).

## Live shapes (measured via vf2probe)

- warm `9214/18/19` (7 entry + 9151 shell + 56 midbody, `EQUAL`)
- f0 single `9393/17/18` (0x510b24=0x100, 0x5111a0=1, 0x5149cc=0xffff)
- f1 single `9385/17/18` (0x512b24=0x100, same)
- both `9528/18/19` (both fighter flags 0x100, same aux)

Entry to 0x22210 (shell) vs 0x10dcc (whole):
- 0x221e8->0x22210: warm 9158, f0 9307, both 9418
- 0x22210->0x10dcc: warm 56, f0 86, both 110

## Flag builder 0x233d0

- warm `g6=0` table 0x232c4 43+1=44
- single `g6=2` table 0x2330c/0x23324 52+1=53 (which fighter has bit8, bit16 clear, 0x820 !=41)
- both `g6=4` (and bit8) table 0x232c4 43+1=44 (warm table, not FFFFDFFC) — gated on half 0/0 and byte 1/0 as on parked.

## Shell 0x23524

- 0x238a4 pair: warm 5+5=10, f0 136+5=141 (fighter0 flagged 136/0x18, fighter1 not flagged 5/0), both 136+134=270 (fighter0 136/0x18 byte1, fighter1 134/0 byte0 with half 0/0). So shell both 9411 body vs single 9300 (+111) and warm 9151 (+260).
- threshold at 0x236b0: warm 0 -> call 0x2364c 17, single live g6=2 r3=0xFFFFDFFC -> 26 (12 calls), both g6=4 r3=0 -> warm 17 (13 calls). Fighter +0x18/+0x20 stay 0 for both (warm), FFFFDFFC for single.
- cluster 0xd4..0xe8: warm 0, single FFFFDFFC, both 0.

## Midbody tail 0x22210->0x10dcc

- warm 56/4/5
- single 86/5/6? (first contact hit, second warm, 0x225cc compact)
- both 110/4/4 body 107 + EQUAL +2 =110 via 0x222ac long loop (both bit8, both half 0/0). Six halfword stores: fighter0+0x6dc, fighter1+0x6dc, g13+0xc, g13+0xe, fighter0+0x8d4, fighter1+0x8d4 (all 0) and 0x0051498c = e8 21 02 as measured. g7/g8 swapped, g14=0x2244c, g0=0.

Whole-task 9528 = 7+9411+110 +2 (g6==4) with EQUAL.

## Native changes

- src/recovered/hybrid.c: vf2_hybrid_coli_midbody_tail_execute early both-live 107/4/4 for the measured parked both shape; hybrid_execute_coli_body already counted +2 for g6==4.
- tests/recovered/test_coli_whole_task_live.c extended to 3 modes (f0,f1,both) 9528/18/19.

## Validation

- ctest -C Debug 72/72
- vf2_coli_whole_task_live differential: f0 9393/17/18, f1 9385/17/18, both 9528/18/19 all live-state equal.
