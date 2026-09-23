# v0404: recover the fa_rob state-27 arm 0x1453c (type-5 walk)

## Verdict

`hybrid_execute_player_1453c` now recovers the fa_rob state-27
fighter-exchange arm reached at `0x14528` when the state-27 fighter's
`+0x197 == 27` (r7 == 27).  The arm:

1. `0x1453c mov 16,r15 ; 0x14540 stib r15,+0x197(g7)` — sets the
   state-27 fighter's `+0x197` to 16;
2. `0x14570 ldos +0x194(g7), g0 ; 0x14574 mov 5,g1 ; 0x14578 call
   0x1ab34` — walks a **type-5** record chain via `+0x194(g7)`;
3. `0x1457c..0x14588` — computes `+0x194(g8) = 0x11000000 + s16(rec+1)`
   and stores it to the other fighter;
4. `0x1458c..0x145dc` — stores `u8(rec+3)` to `+0x822(g8)` (short path:
   both scaling branches taken, text skipped);
5. `0x145f8..0x14620` — clears bit 21 of `+0x1a4(g8)`, computes
   `(g8) = (g8) with bit 6 set to bit 6 of (r3)` via `chkbit`/`alterbit`;
6. rejoins the `0x14628` common exit clearing both fighters' `+0x198`.

Measured short path spans **52** steps from `0x14528` to `0x1463c` with
**+1 call / +1 return** (the 0x1ab34 walker).  The final condition state
comes from `0x14618 chkbit 6, r3`: EQUAL when bit 6 of r3 is set, else
NONE.

## The v0403 blocker is resolved

v0403 scoped this arm but could not complete the `0x14570` tail because
the live park's `+0x194` low half is 0, which indexes `table[0] == 0` and
faults in the walker at `0x1ab4c`.  This slice mines the `0x0200d34c`
type-5 record chains and finds a `+0x194` whose low 13 bits walk to a
type-5 record, then re-probes the tail from a park carrying that `+0x194`
— exactly the "Next" the v0403 note proposed.

Measured index `0x73` (and `0x6f`, plus 110 others) walk the type-5 chain
in 2 iterations to a record rooted in main_data.  From the `0x14528`
state-27 entry the arm completes:

```sh
build/Debug/vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-ip 0x14528 --set-reg g7=0x510980 --set-reg g8=0x512980 \
  --set-reg r7=27 --set-reg r8=0 --set-reg r10=0x510980 \
  --set-reg r11=0x512980 --set-u16 0x510B14=0x73 --until 0x1463c
# -> 52 steps, +1 call / +1 return, ip == 0x1463c
```

The walker is modelled through the existing `vf2_hybrid_coli_1ab34_execute`
procedure (call + ret), so the frame linkage and call/return counters match
the reference exactly.  A walker miss (record == 0) returns
`VF2_ERROR_UNSUPPORTED`, preserving the fail-closed boundary.

## Fail-closed

The measured short path requires, on the live park:
- bit 0 of `+0x1a4(g8)` clear (`0x1459c bbc` taken) — first scaling skip;
- bit 6 of `*(*(u32*)0x50016c + 0x3351)` clear (`0x145c0 bbc` taken) —
  second scaling skip;
- bit 9 of `0x508000` set (`0x145e8 bbs` taken) — text call skipped.

Any of these, or a type-5 walk miss, stays `VF2_ERROR_UNSUPPORTED`.  The
scaling branches (`0x145a0`/`0x145cc` r3*5/4) and the `0x7fc0` text call
path are unmeasured and stay fail-closed.

## Reachability note

The `0x1453c` arm is reached at `0x14528` when the state-27 fighter still
has `+0x197 == 27`.  In the natural `0x1442c` flow the state-27 fighter
first runs the `0x14640` state-27 path (a separate, still-unrecovered
type-15 walk) which clears `+0x197` via the 4-byte `+0x194` store, so the
natural continuation takes the neutral path.  This arm is therefore
recovered and validated as a standalone helper (test entry
`vf2_hybrid_player_1453c_execute_for_test`) plus a focused differential
fixture, and is not yet wired into the `0x1442c` body dispatch.  The
`0x14640` state-27 (type-15) path remains an explicit boundary.

## Pins

- `vf2_player_1453c_live_differential` restores `out/park-1442c.vf2snap`,
  forces the state-27 shape at `0x14528` with `+0x194(f0) = 0x73`, and
  proves the native arm byte-exact to the `0x1463c` boundary: 52 steps /
  +1 call / +1 return, full live-state equal (registers/CC/AC/frames/
  Work-RAM).
- ROM-independent unit pins: NULL arguments, wrong ip, r7 != 27, and no
  pushed frame all fail closed.
- ctest Debug 80/80 with no corridor regressions.
