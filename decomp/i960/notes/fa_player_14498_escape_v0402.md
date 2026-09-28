# v0402: recover fa_rob 0x14498 escape (other +0x19f values)

## Verdict

`hybrid_execute_player_1442c` now recovers the `0x14498` escape for
every `+0x19f` value other than 25/22 on all three `0x14474` entries.
When `cmpobe 25, r6` and `cmpobne 22, r6` both miss, the body restores
`g7/g8` at `0x14498`/`0x1449c` and continues `0x144a0 -> 0x14528 ->
0x14548 -> 0x14560` (neutral: the other fighter's `+0x197` not
16/25/27) to the `0x14628` common exit clearing both `+0x198`. No
`+0x194`/`+0x1a4` stores run; `r6` is the measured `19f` byte and `r15`
keeps the helper poststate.

Spans from `0x1442c` to `0x1463c`:

- direct (f0 `+0x197 == 24`): **57** steps / +2 / +2 (middle 18 + exit
  5; 59 when f1 == 24 via the both-sibling prefix 36);
- swapped (f1 `+0x197 == 24`): **60** steps / +2 / +2 (middle 21 +
  exit 5).

Last compare is `0x14560 cmpobne 16, r8` (taken): it leaves GREATER
when `r8 < 16` (direct escape with neutral f1, e.g. 0) and LESS when
`r8 > 16` (swapped/both escapes with `r8 == 24`). The direct escape
sets the postcondition from `compare(16, r8)`; the swapped escape is
pinned LESS. Measured `19f` values 0/1/30 all share the same counts;
`19f == 0` is the committed fixture representative.

## Measured paths

```sh
build/Debug/vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-u8 0x510B17=24 --set-u8 0x512B1F=0 --until 0x1463c
# -> 57 steps (direct escape; 19f=1/30 identical)
build/Debug/vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-u8 0x510B17=0 --set-u8 0x512B17=24 --set-u8 0x510B1F=0 \
  --until 0x1463c
# -> 60 steps (swapped escape; 19f=30 identical)
build/Debug/vf2probe --rom-dir roms/vf2 --snapshot out/park-1442c.vf2snap \
  --set-u8 0x510B17=24 --set-u8 0x512B17=24 --set-u8 0x512B1F=0 \
  --until 0x1463c
# -> 59 steps (both-24 escape)
```

## Unrecovered / out of scope

- Escape shapes whose downstream fighter state is not neutral (f1 or
  f0 `+0x197` in {16,25,27}, either `+0x19b == 16`) diverge to the
  state-25 arm, the `0x1453c`/`0x14570` fighter-state arms or the
  short `19b == 16` exit and stay fail-closed.
- The `0x1453c`/`0x14570` fighter-state arms (need the `0x1ab34` tail)
  and the `0x14640` state-27/28/bit-4 gates stay fail-closed.

## Pins

- `vf2_player_1442c_live_differential` now runs fourteen ROM-backed
  cases including the three escapes (57/60/59, full live-state equal
  with the measured GREATER/LESS/LESS postconditions).
- ctest Debug 78/78 with no corridor regressions.
