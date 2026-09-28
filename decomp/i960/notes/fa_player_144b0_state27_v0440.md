# `fa_player` direct `0x144b0` state-27 successor (v0440)

The direct state-25 arm at `0x144b0` computes the unsigned `cmpobl` selector
before its state chain. With fighter0 state `+0x197 == 25`, fighter1 state
`+0x197 == 27`, fighter1 `+0x194 == 0x73` (a valid type-5 index), and the
measured `r13 < r3` relation, the ROM takes the `0x1452c` state-27 swap into
the shared `0x14570` body.

From `out/park-1442c-s25.vf2snap`, the controlled reference run measures:

- `100` instructions to `0x1463c`;
- `+2` calls and `+2` returns, including `0x19ef8` and `0x1ab34`; and
- exact CPU, condition-state, frame, and Work-RAM equality against native C.

The native arm now admits only the measured state-25/state-27 successor when
the unsigned relation is the taken (`r13 < r3`) case. Other direct state-25
compositions remain fail-closed.
