# `fa_player` `0x1453c` state-28/state-16 join (v0439)

The `0x14528` dispatch reaches the `0x14564..0x1456c` register swap when
`r7 != 16` and `r8 == 16` after the state-27 checks. The native recovery had
measured joins for `r7 == 0`, `24` and `25`, but rejected `r7 == 28`.

With the valid type-5 index `0x73` at the selected fighter's `+0x194`, a
controlled ROM run from `out/park-1442c.vf2snap` measures the state-28 sibling
as:

- `55` instructions to `0x1463c`, with `+1` call and `+1` return through
  `0x1ab34`;
- `129` instructions when board bit 9 is clear and the `0x7fc0` text tail is
  taken, with `+2` calls and `+2` returns; and
- exact CPU, condition-state, frame, and Work-RAM equality against native C.

The recovery admits only this measured state-28/state-16 shape. Other
unmeasured state/scaling combinations and type-5 walker misses remain
fail-closed.
