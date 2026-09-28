# `fa_player` `0x19ef8` selector `0x284` sibling — v0560

The parked `out/pre14288.vf2snap` was restored, `g0` was set to `0x284`, and
the entry `fighter+0x1a4` word was set to zero before running the reference
executor to `0x1428c`. The oracle measured 1804 instructions and 4 calls / 4
returns. The native corridor now matches the complete live state.

Measured selector-specific evidence:

- entry is accepted only for the depth-zero `player=0x00510980` park;
- selector setup executes opcode `1` and then terminator `0`, ending at
  `data_pointer+0x0b`; the final stored cursor words are the same measured
  `data_pointer+0x0b` value;
- the persistent scratch pointer is zero on entry and the transient expansion
  base is `0x00520000`;
- the 60-byte expansion census is `33×06`, `15×04`, `9×03`, `3×01`;
- the census long arm consumes this exact measured byte stream:
  `05 04 04 07 05 07 07 08 06 05 04 04 07 07 07 06 08 08 09 05 07 06 08
   06 07 08 07 05 06 06 05 07 09`;
- the three float inputs are read from `table_pointer+0xbcc` and are
  `0x00000000`, `0x00000000`, `0x46800013`; the converted stores are the
  measured `0, 0, 0x4000` window at `player+0x16e`;
- the selector-specific tail leaves `player+0x170 = 0x40000000` and
  `player+0xbdc = 0x20`, with `g2 = table_pointer+0x37`.

The implementation gates the measured selector, setup shape, histogram,
long-arm stream and float window independently. Unknown selectors and
unmeasured neighboring streams remain `VF2_ERROR_UNSUPPORTED`.
