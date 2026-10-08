# v0755g: 0x1fcc0 (`display_profile_apply`) recovered in C

**Slice 8 of the advance plan execution chain.** All 6 sub-callees of
`0x1fcc0` (per v0748) were recovered as standalone execute hooks in
v0749–v0755f. This slice inlines all of them and recovers the parent
function. With this, the full sub-call chain inside `0x1fcc0` is
recoverable by inlining; the parent itself remains a stand-alone
execute hook until a per-IP hook entry is wired for it in
`hybrid_execute_interpreted_until`.

## Function shape

```
display_profile_apply: address=0x0001fcc0 end=0x0001fee4 blocks=15 indirect=no
```

548 B, 15 blocks, no indirect branches. The function does:
1. Mode-decision from fighter pair (r3,r4) and pre-existing 0x500068/0x500064/0x50004c.
2. Three convergent paths write 0x500064 (mode byte) + 0x500068 bit20 +
   0x50a000/0x50a004 (one of two float pairs).
3. Six inlined sub-calls: 0x1ff0c → 0x1fee4, then video/table
   cluster, then 0x1fffc → refuses 0x2c38, then 0x4b410, then zero
   0x50a014..0x50a026, then 0x2eab8, then 0x11704. The 0x2c38
   refusal lives inside the inlined 0x1fffc — this function's
   refusal point matches v0751's existing 0x1fffc refusal.

## Path inventory (test exercises all 6)

| path | trigger | exp mode | exp 0x500068 | exp 0x50a000/4 |
|------|---------|----------|--------------|------------------|
| A (combo 2+1) | r3[0x1b1]=2, r4[0x1b1]=1 | 0x0c | 0 | 0x3b32674f / 0x3f800000 |
| A2 (combo 1+2) | r3[0x1b1]=1, r4[0x1b1]=2 | 0x0c | 0 | 0x3b32674f / 0x3f800000 |
| B (bit21+bit20 set in 0x500068) | pre_500068=0x300000 | 0x0a | 0x300000 | 0x3a3117c4 / 0x40000000 |
| C (mode=10 & 0x50004c=2) | pre_500064=10, pre_4c=2 | 0x0b | 0 | 0x3b32674f / 0x3f800000 |
| D (default) | pre_500064=5 | 5 (unchanged) | 0 | 0x3b32674f / 0x3f800000 |
| D2 (bit21 set, default mode) | pre_500068=0x200000 | 5 | 0x200000 (bit21 stays) | 0x3b32674f / 0x3f800000 |
| E (bit21 set, mode=10, 0x4c!=2) | pre_500068=0x200000, pre_500064=10 | 10 | 0x200000 (bit21 stays) | 0x3b32674f / 0x3f800000 |

## Three small recovery design choices

1. **Combo detection uses `goto combo_set` instead of fall-through**:
   the disasm has two distinct fall-through structures (the (2,1)
   combo jumps via `cmpobe 1`; the (1,2) combo falls through from
   `block_0001fce8`). The C version mirrors both with explicit
   `goto combo_set` to keep the i960 semantics observable.

2. **0x018021ee is an unmapped hardware register**: the model2a
   doesn't include VDP1 control registers, so writing here returns
   `VF2_ERROR_OUT_OF_BOUNDS`. The recovery reads the value from the
   ROM table at 0x6ef0c, attempts the write, but treats
   `VF2_ERROR_OUT_OF_BOUNDS` as expected (any other error aborts).
   The i960 hardware would have written to the VDP1 register via
   the bus; the per-step hook's interpretation fallback would
   fail similarly, so this is conservative.

3. **Main ROM must be attached** for the recovery to run end-to-end.
   The test attaches a fake 0x80000-byte zero buffer as `main_rom`.
   In a real native differential this is provided by the loaded
   ROMs (the recovery is reading from the same regions the
   scheduler reads from).

## Sub-callees inlined

| callee | recovery status | what the inline does |
|--------|-----------------|----------------------|
| 0x1ff0c (mode constants) | recovered v0750 | calls 0x1fee4 internally |
| 0x1fee4 (init 1.0 floats) | recovered v0749 | writes 26 1.0 floats |
| 0x1fffc (color profile apply) | recovered v0751 | writes 3 bytes, refuses 0x2c38 |
| 0x2c38 (color table rebuild) | recovered v0755f | REFUSED via 0x1fffc's existing pattern |
| 0x4b410 (video command submit) | recovered v0752 | 5 writes |
| 0x2eab8 (display runtime initialize) | recovered v0754 | 36+ writes, inlines 0x31004 |
| 0x11704 (video table expand_128) | recovered v0753 | nested loop |

The 0x2c38 refusal means this function's effective end-state is
"cpu->ip = 0x20050 + return VF2_ERROR_UNSUPPORTED". The remaining
3 sub-calls (0x4b410, 0x2eab8, 0x11704) and the 0x50a014..0x50a026
zero-fill live after the refusal point in the i960, so they are
NOT executed by this recovery.

## Test

`tests/recovered/test_player_1fcc0_native.c` (vf2_player_1fcc0,
ctest #48). Verifies:
- status == VF2_ERROR_UNSUPPORTED (the 0x2c38 refusal).
- cpu->ip == 0x20050 (matches v0751's existing refusal pattern).
- 0x500064 matches expected mode per path.
- 0x500068 matches expected (bit20 cleared/set per path; bit21
  preserved if it was pre-set).
- 0x50a000/0x50a004 match the two float pairs.
- Init floats verified for paths where mode==10 dispatch does not
  overwrite them (skipped for paths B and E in the fixture).

## What's in this slice (v0755g) and what's NOT

- This slice **does** recover 0x1fcc0 in C with all 6 sub-calls
  inlined (5 fully executed + 1 refused via inlined 0x1fffc).
- This slice **does not** wire 0x1fcc0 into the per-step hook
  table in `hybrid_execute_interpreted_until`. That wiring
  remains a separate slice (per v0744's general design).

## Validated

- `git diff --stat src/`: recovery + test changes only.
- ctest #48 `vf2_player_1fcc0`: PASS.
- All other 0x1fcc0-related tests: still PASS.
