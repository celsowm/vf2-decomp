# 0x11704 recovered in C (v0753) — video_table_expand_128

## Summary

`0x11704` is recovered in C and unit-tested. The function is a
nested-loop byte-to-word expander that copies **128 source bytes
to 128 destination words (with 3-byte gaps)**, repeated `g2` times.

This is a stand-alone execute hook (not yet wired into the
dispatcher chain). It is the 5th recovered sub-callee of `0x1fcc0`
(display_profile_apply); the only remaining sub-callee is
`0x2c38` (color_table_rebuild, 432 B, 11 blocks, complex —
deferred).

## Function shape

| metric | value | source |
|---|---|---|
| extent | `0x11704..0x11744` (64 B) | `vf2i960 function` |
| blocks | 5 | `vf2i960 function` |
| sub-calls | 0 | `vf2i960 function` |
| indirect branches | 0 | `vf2i960 function` |

## Disassembly

```
0x11704  lda      0x12800000, g0     # dst = LUMA_RAM_BASE
0x1170c  lda      0x00078d10, g1     # src
0x11714  ld       0x00078d0c, g2     # outer count (u32)
0x1171c  shlo     7, 1, g3            # g3 = 128
0x11720  ldob     (g1), r3            # byte
0x11724  st       r3, (g0)            # word (4-byte stride)
0x11728  addo     1, g1, g1            # src++
0x1172c  addo     4, g0, g0            # dst += 4
0x11730  cmpdeco  1, g3, g3            # g3--; if g3==0 fall
0x11734  bl       0x00011720         # else inner-loop
0x11738  cmpdeco  1, g2, g2            # g2--; if g2==0 fall
0x1173c  bl       0x0001171c         # else restart inner
0x11740  ret
```

## Recovered semantics

```c
for (i_outer = 0; i_outer < g2; ++i_outer) {
    for (i_inner = 0; i_inner < 128; ++i_inner) {
        uint8_t b = read_byte(src);
        write_word(dst, (uint32_t)b);  // zero-extend
        src += 1;
        dst += 4;
    }
}
```

The 4-byte destination stride means each byte lands in the low
8 bits of a 32-bit word; the high 24 bits of each word become
zero (matching the `ldob` + `st` register shape, since `r3` is
a 32-bit register with the byte zero-extended).

## Caller / dispatch

This function is called from the original i960 program as a
sub-routine of `0x1fcc0` (display_profile_apply). The recovered
function takes a single return point (`0x11740`).

## Test

`tests/recovered/test_player_11704_native.c` exercises two
scenarios:

1. `outer=1` — 128 bytes copied to luma RAM.
2. `outer=3` — 384 bytes copied to luma RAM, advancing the
   source pointer between outer iterations.

Each scenario verifies:
- All 128*N destination words at 0x12800000..(0x12800000+128*N*4)
  contain the expected byte (zero-extended to u32).
- The 4-byte gaps between writes remain at the sentinel.
- The word just past the range is untouched.
- Return status is `VF2_OK` and `cpu->ip == 0x11740`.

The fake `main_rom` is required because `0x78d0c` and `0x78d10`
lie in the i960 program ROM window; the test writes the source
bytes and the outer count directly into the fake_rom backing
buffer.

```
$ ctest -R vf2_player_11704
1/1 Test #46: vf2_player_11704 ........ Passed  0.01 sec
```

## Status

- All 4 paths through the function are recovered (no paths REFUSED).
- No sub-calls means no refused sub-callees.
- Function code: `src/recovered/hybrid.c:33650-33718`.
- Header declaration: `include/vf2/hybrid/player.h:130-138`.
- ctest entry: #46 (vf2_player_11704).
- `src/` byte-different from `cd27cce2` (the v0753 work is
  appended to `hybrid.c` and `player.h`).

## Wiring boundary (unchanged from v0744)

The dispatcher chain in `hybrid_execute_player_post_29414` and
similar would call this via `hybrid_execute_interpreted_until`,
which uses `vf2_i960_run` with only `stop_address` / `max_steps`
— not a per-step callback. Wiring is a separate slice that needs
the per-step hook table (v0744 candidate A).
