# v0346: coli exit landing — live call return and double-pop

## Verdict

The live `fa_coli` call to `0x225cc` is `call 0x225cc` at `0x00022290`.
The call return address is the following `ret` at `0x00022294`, which
pops to the parent return (scheduler `0x00010dcc`). Standalone units
that enter `0x225cc` with a stand-in return (`0x22240`) keep a single
procedure pop. Units that enter with the live return `0x22294` **and**
leave a parent frame on the stack now double-pop through that `ret` to
`0x10dcc`.

## Disassembly evidence (real ROM)

```text
00022284  cmpobe  0, r6, 0x00022294
00022288  mov     r7, g7
0002228c  mov     r8, g8
00022290  call    0x000225cc
00022294  ret
```

Both mid-body exits that reach `0x225cc` land on `0x22290`; the
instruction after the call is always the `ret` at `0x22294`.

## Wrapper change

`vf2_hybrid_coli_225cc_execute` now:

1. runs `coli_225cc_body` as before;
2. `hybrid_complete_procedure` for the `0x230b8` ret (lands on the
   entered return);
3. if `ip == 0x22294` and `local_frame_depth > 0`, completes a second
   procedure return (the live `0x22294` ret).

The mechanism is shared by every coli shape. Procedure-only units that
still enter with `0x22240` are unchanged.

## Site-A live-landing unit

Parent frame: `enter(0x22210, 0x10dcc)`.
Child frame: plant `g7`/`g8`, then `enter(0x225cc, 0x22294)`.

Expected (site-A long leg):

| Field | Value |
|---|---|
| final `ip` | `0x10dcc` |
| instruction delta | **884** (882 + `0x230b8` ret + `0x22294` ret) |
| calls delta | **7** |
| returns delta | **9** (7 nested + child ret + parent ret) |
| `local_frame_depth` | 0 |

## Validation observed

- `vf2_native_runtime` unit suite: pass (includes new live-landing).
- `vf2_native_runtime_state`, `vf2_endurance_regressions`,
  `vf2_native_differential`, `vf2_native_scheduler`: pass.
- ROM-backed `native-third/fourth/fifth/sixth/eleventh-dispatch`: pass.
- `vf2cycles --input 16` from `scratch-sixth.vf2snap`: **8/8 MATCH**
  (296 blocks / 17,340 instructions each side, both `0x1645c`).

Warm PUNCH never reaches `0x225cc` (both contact results zero), so the
whole-task pin `9214/18/19` is untouched.

## Remaining coli gaps

- Unmeasured flag/scan/board combos still fail closed by design.
- Site-B-only entry still fail-closes (prefix proven only on the
  site-A leg).
- Hybrid coli task dispatcher still pins the warm whole-task counts;
  non-warm live admission needs its own measured shape pins.
- `hybrid_execute_coli_body` composition of mid-body that *does* call
  `coli_225cc_body` already lands at `0x10dcc` via mid-body's own
  complete; this slice proves the standalone-wrapper landing.
