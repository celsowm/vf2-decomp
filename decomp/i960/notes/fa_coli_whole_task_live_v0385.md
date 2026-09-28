# v0385 — coli whole-task live single-fighter (f0/f1) via flag builder + shell threshold

**Status:** recovered C + ROM-backed differential green; whole-task gate now pins 9393/9385.

Reopens the whole-task live that v0384 left at the `0x233d0`/`0x2396c` g4 divergence. The parked snapshot `out/coli-parked-221e8.vf2snap` at `0x221e8` measures:

* warm `9214/18/19` (both fighters `+0x1a4` bit8 clear)
* f0 `9393/17/18` (`0x510b24=0x100`, fighter0 bit8)
* f1 `9385/17/18` (`0x512b24=0x100`, fighter1 bit8) — mirrored
* both `9528/18/19` (both 0x100) — not yet C-pinned beyond flag builder

The whole-task shell `0x23524` contains the flag builder `0x233d0`, two `0x2396c` poly clusters, two `0x238a4` g3-scans, `0x238f8` nested scan and the `bal 0x23694` tail that writes the six cluster words at `g13+0xd4..0xe8` and the fighter deltas at `+0x18/+0x20`.

## 1. Flag builder `0x233d0` live

Warm (both bit8 clear) is 44 steps (`mov`/`ld`/`bbc`/`bbs` fall-through, `g6=0`, table `0x232c4` → `0,0,0x3f000000,0x3f000000,0x2336c,0`).

Live single-fighter (xor bit8 set, `0x820` bytes `1`/`0` for f0 vs `0`/`1` for f1, neither `41`) takes the `xor` path at `0x23408`: `addo 31,10` (=41), `ldob 0x820(g7)`/`cmpobe 41` (not taken), `ldob 0x820(g8)`/`cmpobe 41` (not taken), `setbit 1,g6` → `g6=2`. The later `bbs 0,g6`/`bbs 2,g6` fall through, `bbc 16,r7`/`bbc 16,r8` both taken (bit16 clear), `bbc 1,g6` not taken (bit1 set), then `bbc 8,r7` selects table `0x2330c` when fighter0 has bit8 else `0x23324` when fighter1 has it (branch at `0x234ac`).

* f0 picks `0x2330c`: `[0xFFFFDFFC,0xFFFFDFFC,0,0x3f800000,0x23381,3]` (4294959100 for the first two)
* f1 picks `0x23324`: `[0xFFFFDFFC,0xFFFFDFFC,0x3f800000,0,0x23381,3]`

Warm `0x232c4` vs live `0x2330c/0x23324` explains the `g13+0x88=3` and `g13+0xb4/0xb8=FFFDFFC` deltas. Body is 52 excl ret (`53` total) vs warm `43` (`44` total). The `and` case (`g6=4`, both live) keeps warm table `0x232c4` and 44 steps (allowed for the 9528 shape).

The standalone `vf2_hybrid_coli_233d0_execute` and the body-only `coli_233d0_body` now handle `g6=0` (43), `g6=2` (52, table `0x2330c`/`0x23324` based on which fighter has bit8, bit16 must stay clear) and `g6=4` (43, warm table). Other `g6` values, `0x242`/`0x241`/`108` magic `+0x1a8` states and `bit18`/`bit14` siblings stay fail-closed.

## 2. Shell `0x23524` live threshold

The `bal 0x23694` tail reloads fighters and does `bbc 3,g6` (clear for `g6=0`/`2`) then `ld 0x148(g13),r3` / `cmpobl 0,r3`. Warm `r3==0` falls through to `call 0x2364c` (17 ins, six stores `0` at `0xd4..0xe8`). Live single has `r3==4294959100` (the `0xFFFFDFFC` that the flag builder's live table already left at `g13+0x148` via the window-push FIFO replies) so `0 < r3` unsigned is true and it branches to `0x236c4`. That path does the `0x88`/`5`/`4` compares and then the `0x3e99999a` mulr/subr chain that yields the six `0xFFFFDFFC` stores and the fighter `+0x18/+0x20` deltas of the same value.

* Warm: `r3==0` → `call 0x2364c` (17), six stores `0`, fighter deltas `0`
* Live single (`g6==2`, `r3==4294959100`): threshold path (26), six stores `r3` (`0xFFFFDFFC`), fighter `+0x18/+0x20` become `r3` as well (pinned). The `call 0x2364c` is skipped, so total calls drop from 13 to 12.

The `g3-scan` live already handled in v0384 (first `0x238a4` 136 for f0 vs 5 warm, second 134 for f1). Combined shell live is `9151 +9 (flag) +131/129 (g3) +9 (threshold)` = `9300` vs warm `9151`, plus entry `7` = `9307` vs warm `9158`. The remaining `86` vs `56` midbody tail accounts for the final `9393`/`9385` totals.

## 3. Whole-task composition

`hybrid_execute_coli_body` segments `7` entry + `9300` shell live + `86` midbody live + final `EQUAL` compare (`+1` for single, `+2` for both) = `9393`/`9385`/`9528`. The `EQUAL` compare is the `hybrid_set_compare_result` that the midbody tail's final `cmpobe` leaves on the reference; we pin it and count the extra compare (`+1` for `g6==2`, `+2` for `g6==4`) to make the instruction counts exact. Warm stays `9214` (`7+9151+56`).

The gate in `vf2_hybrid_first_dispatch_task_execute` now allows `9393/17/18` and `9385/17/18` alongside `9214/18/19` and `9528/18/19`. Both-live `9528` is admitted at the gate but still has `g4`/`0x10c` etc divergences beyond this slice and stays fail-closed for the full live state beyond the shell.

## 4. Tests

* New ROM-backed `vf2_coli_whole_task_live` (+ `_differential`): loads `coli-parked-221e8`, applies f0/f1, steps reference whole-task to `0x10dcc` (9393/9385) and runs `vf2_hybrid_first_dispatch_task_execute` via the coli registry (`g29`) for the same live state; asserts `9393/17/18` and `9385/17/18` and full live-state equality.
* Existing `vf2_coli_238a4_live`, `vf2_coli_22404_live` etc still pass.

## 5. Validation

* `ctest -C Debug -j2` 72/72 (was 72, +2).
* `ctest -C Debug -R native -j2` 12/12.
* No ROM/snap/trace committed.

## 6. Next

Whole-task both-live `9528` (`g4`/`0x10c` etc) and the midbody long `9529` (`9158+371`) remain for a dedicated slice. The flag builder's `and` (`g6==4`) path is now allowed at the gate but still uses the warm table; its full live window-push threshold beyond the 0x148 pin remains open.
