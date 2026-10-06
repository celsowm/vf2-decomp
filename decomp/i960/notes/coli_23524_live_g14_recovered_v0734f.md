# v0734f: g14 is recovered at the `0x23524` return — the `0x23524` shell boundary is now FULL MATCH

**Closes Phase 2.1.** Together with v0734e, both named divergences at the
`0x22210` boundary are gone. The differential is **FULL MATCH** on every
admitted leg, and the refused leg C still refuses and is still load-bearing.

## What was wrong

The shell's `bal 0x23694` at `0x23644` leaves `g14 = 0x23648` on every admitted
path (both warm and live execute it). The native never explicitly wrote `g14`,
so the test recorded a g14 divergence with the reference on both admitted legs
(`native 0x22428`, the entry value, vs `ref 0x23648`). It was the last open
divergence at this boundary.

## The fix

One line, immediately before `hybrid_complete_procedure(machine, cpu, body,
total_calls, total_rets)`:

```c
cpu->registers[VF2_I960_G0_REGISTER + 14u] = UINT32_C(0x00023648);
```

The warm arm is left alone (it already reaches `0x23648`). The refused leg C
never reaches this code — its g6-bit0 check returns `VF2_ERROR_UNSUPPORTED`
at the entry gate (`hybrid.c:30828`) before any of the recovered tail runs —
so publishing unconditionally here matches the reference on the admitted paths
and cannot bring C back to life.

## An easy mistake to avoid, recorded

The first attempt used `cpu->registers[14]`. That is a **local** register, not
the global one — the i960 distinguishes the two and the test compares at the
global index (`VF2_I960_G0_REGISTER + 14u`, defined in the test as
`G14_INDEX`). The test printed `native 0x22428` and the gate would not have
failed if I had not re-read it; both legs said `g14 0x23648 vs 0x22428`.

## Gate integrity

The test's "two documented divergences, and nothing else" branch became the
test's own "FULL MATCH" assertion, with `diff.equal == true` the success case.
Anything less is reported by field, including which specific register(s)
differ and their values.

Negative control on this exact tree — publish `g14 = 0`:

```text
MISMATCH A warm: native state does not FULL MATCH the reference
A warm: 1 mismatch(es)
MISMATCH B live f0: native state does not FULL MATCH the reference
B live f0: 1 mismatch(es)
```

Both legs fire, with one mismatch each (the planted g14). The `register[14]`
detail line is in the output but the `MATCH` line is suppressed under the
`MISMATCH` branch.

A SECOND NEGATIVE CONTROL worth keeping the pattern. v0734e had a planted
regression executed through a scripted whole-file replace that silently hit
**ten** unrelated `hybrid_set_compare_result` call sites; the diff review caught
it. v0734f used the precise `edit` tool with full context on the unique
publish site and the diff was a pure insertion (0 removals), so the same
defect class does not recur here.

## Validated

```text
A warm: MATCH (9151 insn, 13 calls, 14 returns, g3=0x00000000 g4=0x00000000);
        g14 0x00023648 vs 0x00023648, cc 2 vs 2, arithmetic_control 0x3f001002 vs 0x3f001002
B live f0: MATCH (9300 insn, 12 calls, 13 returns, g3=0x00000000 g4=0xffffdffc);
        g14 0x00023648 vs 0x00023648, cc 3 vs 3, arithmetic_control 0x3f001001 vs 0x3f001001
C g6 bit0: correctly REFUSED, and load-bearing - reference exits
        g3=0x0000fffe g4=0xffffffff cc=1 g14=0x00022428 after 6193 insn / 10 calls
```

Both admitted legs FULL MATCH on every published CPU field — registers, ip,
compare_result, arithmetic_control, sat, prcb, local_frame_depth, process,
control, the lot. The refused leg C still refuses and still has the
load-bearing g3/g4 values. `src/` diff is a pure insertion.