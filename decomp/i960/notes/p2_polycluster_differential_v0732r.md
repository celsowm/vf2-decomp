# P2 step 5: the 0x2396c differential, and two real register defects it found (v0732r)

**The missing differential now exists, and it found two genuine bugs in a
recovery that has been "done" since v0285.** Both were invisible to the
committed unit test, for reasons this note documents.

## The gap that was closed

`docs/UNCOVERED_BRANCHES.md:4955` records `0x2396c` as native since v0285, and
the only committed test was `test_coli_2396c_poly_cluster`
(`tests/recovered/test_native_runtime.c:6078`). That test:

- synthesises an **identity** index table — `rom[0x2394cu + i] = i` — so the
  real 30-byte permutation is never exercised;
- **never runs the reference executor**, asserting the native result against
  hand-written constants, so it is a self-consistency test;
- **zeroes the cluster**, so every slot's 4th word reads as zero — the exact
  condition that produced the v0732p retraction.

A body that was right on the identity case and wrong on the permuted case, or
that hard-coded the 4th word, would have passed.

`tests/recovered/test_coli_2396c_live.c` replaces that gap. It starts from the
parked whole-task state at the measured call site with the real ROM attached, so
the live permutation and the live 4th-word window are both in play, and compares
the reference executor against `vf2_hybrid_coli_2396c_execute` with
`vf2_i960_compare_live_state`.

### Two legs, one variable apart

| leg | `g7` | result |
|---|---|---|
| measured call site | `0x00510980` (fighter0) | FULL MATCH |
| mirrored control | `0x00512980` (fighter1) | FULL MATCH |

Both must match — the mirrored leg is the control on the admitted one, not a
second arbitrary case. `g7` has to be pinned **at the entry**: setting it before
the approach is silently ineffective, because the caller reloads `g7` on the way
in. That cost one debugging round and is worth writing down.

### It also guards its own premises

```text
fixture premise: 21/30 non-identity permutation bytes, 30/30 non-zero 4th words
fighter 0x00510980: FULL MATCH (3023 insn, 3 calls, 4 returns, all 120 cluster words, all 30 4th words)
fighter 0x00512980: FULL MATCH (3023 insn, 3 calls, 4 returns, all 120 cluster words, all 30 4th words)
```

The premise check fails the test if the permutation ever becomes the identity or
the 4th-word window ever reads as all zeros. Without it the test could quietly
stop covering the two things it exists to cover — which is how v0732p went wrong.

## Defect 1: `g3` was dropped

The guest's last remap is a bare `ld`/`call`/`stos`:

```text
00023b5c  ld   0x00000118(g13), g3
00023b60  call 0x00023878
00023b64  stos g3, 0x00000618(g7)
```

and nothing reloads `g3` before the ret, so on a direct `0x2396c` entry it is
observable. `coli_2396c_body` computed it into a local and never published it, so
the native left `g3 = 0` where the reference leaves **`0x0000fffe`**.

`coli_2396c_body` now takes a `g3_out`, and `vf2_hybrid_coli_2396c_execute` writes
it to `g3`. The `0x23524` shell's two call sites pass a throwaway: the shell has
its own proven contract, and whether `0x23524` should publish the `g3` its
children leave behind needs a shell differential to answer. Changing it here
would alter a proven recovery on evidence gathered at a different entry.

## Defect 2: `g4` was not complemented

Measured at the ret: **`0xffffffff`**, native `0`.

`0x23a30 mov 0, g4` clears the accumulator, so it enters the remaps as 0, and
`0x23b48 not g4, g4` complements it before the second remap.

**The complement must be applied to the exit value only.** Applying it inside the
body, between the remaps, inverts remap 2's mask (`~g4` becomes 0 instead of
`~0`) and drops `g7 + 0x614` from `0x000000fe` to `0` — a *worse* result than the
bug being fixed. The masks consume the accumulator as it entered; only the
register left at the ret is complemented. Measured, then measured again the other
way, which is the only reason the distinction is in this note rather than a guess.

`vf2_hybrid_coli_2396c_execute` therefore publishes `~g4`. Again the shell is
untouched.

### Operand order, the thing I got wrong twice

i960 three-operand forms print the **destination last**:

```text
00023a4  cmpinco 29, r9, r9     -> leaves r9 = 30, so r9 is the destination
00023b3c and    g4, g3, g3     -> therefore g3 &= g4, NOT g4 &= g3
```

I first read `and g4, g3, g3` as `g4 &= g3` and "fixed" both remaps accordingly.
The differential caught it immediately: instruction count 2963 against the
reference's 3023, `g4` still 0, and `+0x614` now wrong too. The existing body's
`g3 &= g4` / `g3 &= ~g4` was right all along. `cmpinco 29, r9, r9` is the
cheapest way to settle it — the measured loop leaves `r9 = 30`.

**General rule this earned:** a disassembly reading is a hypothesis. The
differential is what decides it, and a "fix" that makes a previously-matching
count disagree is a fix that is wrong, not a fix that found a new bug.

## What the v0732q retraction is now worth

The 4th-word claim is not just retracted, it is **proven** in both directions:

- the 30 slots' 4th words equal `u32(0x020078b4 + slot*16)` — the ROM-resident
  window the guest loads via `r3 = 0x020078a8` at `0x2396c`;
- all 30 are non-zero in live state, so the v0732p fixture's all-zero result was
  the fixture, not the code;
- and the native body reproduces every one of them.

The `0x0d00` block is now covered by a reference differential end to end, on the
real permutation, with the 4th word sourced from ROM.

## Validated

- 12/12 `coli` tests, including `vf2_coli_2396c_live_differential`.
- `vf2_coli_2396c_live_differential`: both legs FULL MATCH, 3023 insn / 3 calls /
  4 returns / 120 cluster words / 30 fourth words each.
- Full suite: see the commit message for the observed count.

## Still open

- The `0x23524` shell publishes neither `g3` nor the complemented `g4`. Whether
  it should is unmeasured, and needs a shell differential.
- `+0x110/+0x114` setbit sides remain as characterised: this leg is the warm
  one, and no claim is made about the unmodelled sides.
- `0x281b0` / `0x28208` joining at `0x2891c`, the mode-5 loop at `0x28a04`, and
  fighter bit 6 at `0x28af8` are untouched.
- Sanitizer gate still unrun for the v0732b–v0732r series.
