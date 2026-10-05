# P2 step 4: the 0x23984 producer's full contract — and v0732p's `w3 = 0` is WRONG (v0732q)

**Two results, one of them a retraction.**

1. `vf2probe --dump-regs` closes the tool gap that made the v0732p contract
   incomplete: the register poststate at a boundary is now machine-readable.
2. With that poststate readable, the loop's real instruction sequence falls out
   of the disassembly, and it **contradicts v0732p**. The 4th word of every
   slot is a **memory read**, not a constant zero. v0732p's central conclusion
   was an artifact of its own fixture.

---

## 1. RETRACTION: v0732p's "the padding word is zero"

v0732p (`p2_producer_verified_v0732p.md`) states:

> **Not** the next overlapping source word … The pre-clear value `0xDEADBEEF`
> does not appear either, so **every** slot word really is written, and the 4th
> is written as zero.

and gives the mechanism as:

> the `stq` stores a 64-bit quadword register whose high word is zero, so the 3
> loaded words plus a zero land in the 16-byte slot.

**Both are wrong.** The disassembly is unambiguous:

```text
00023980  mov      0, r9
00023984  ldob     0x0002394c[r9], r11            <- loop head
0002398c  mulo     12, r9, r10
00023990  ldt      (r8)[r10], r4                  <- 12 bytes into r4, r5, r6
00023994  ld       0x0000000c(r3)[r11*16], r7     <- a FOURTH load, into r7
0002399c  stq      r4, 0x00000d00(g7)[r11*16]     <- stores the quadword r4
000239a4  cmpinco  29, r9, r9
000239a8  bne      0x00023984
000239ac  lda      0x3d4ccccd, r15                <- next block
```

`stq r4` stores the **quadword `r4`**, which on the i960 is the register *pair*
`r4, r5, r6, r7`. So the 4th word of the slot is `r7` — and `r7` was just
loaded from `r3 + 0x0c + r11*16` one instruction earlier. It is not a zero; it
is whatever that memory holds.

### Why v0732p measured zero

v0732p pinned `r3 = 0x0100a000` to "keep the read in range", seeded the source
at `0x01008000` and pre-cleared the destination at `g7 + 0x0d00` — but it never
wrote anything at `0x0100a000`. The whole auxiliary window read as zero, so
every `r7` was zero, so every 4th word was zero. The pre-clear proved the 4th
word is *written*; it could not prove *what* is written. Reading "zero" as
"the constant zero" was the mistake.

This is the same class of error as the v0732k wrong-fighter-bases episode: a
fixture that did not vary the variable under test, so a value observed in the
fixture got promoted to a property of the code.

### The measurement that settles it

Seed `r3 + 0x0c + slot*16` for all 30 slots with **distinct** values
(`0xA0000000 + slot*0x101`), pre-clear the destination to `0xDEADBEEF`, run
`0x23980 -> 0x239ac`:

```text
run: status=ok halt=stop address ins=211 calls=10288 compare=equal
exit registers: r4=343 r5=344 r6=345 r7=0xa0001c1c r9=30 r10=348 r11=28

slots whose w3 equals u32(r3 + 0x0c + slot*16) : 30 / 30
distinct w3 values across slots               : 30
```

30 of 30, 30 distinct values. `r7` on exit is `0xA0000000 + 28*0x101` — the
value for slot 28, which is `perm[29]`, the **last** iteration. So `r7` is a
genuine data-dependent load.

Reproduce with `out/w3test.py` (gitignored).

---

## 2. The producer's complete, corrected rule

```text
for r9 = 0 .. 29:
    r11    = u8(0x2394c + r9)                  # 30-byte permutation, in ROM
    r10    = 12 * r9
    r4     = u32(r8 + r10 + 0)                 # ldt: 12-byte packed triple
    r5     = u32(r8 + r10 + 4)                 #   into r4, r5, r6
    r6     = u32(r8 + r10 + 8)
    r7     = u32(r3 + 0x0c + r11*16)           # <- the word v0732p missed
    u32(g7 + 0x0d00 + r11*16 +  0) = r4
    u32(g7 + 0x0d00 + r11*16 +  4) = r5
    u32(g7 + 0x0d00 + r11*16 +  8) = r6
    u32(g7 + 0x0d00 + r11*16 + 12) = r7         # stq r4
```

Every clause is now measured. The block is **211 instructions, 0 procedure
calls**, and it writes **all four** words of all 30 slots.

### So what is the "3 -> 4 expansion"?

Still correct, and now better explained. The source contributes **3** words per
iteration (`ldt`, 12 bytes, stride 12). The destination slot is **4** words
(16 bytes). The 4th word is not a padding constant and not a copy of a source
word — it is an **independent load** from a second structure indexed by the
*destination* slot rather than the source index:

```text
source index   r9 = 0..29   (loop order)
destination    r11 = perm[r9]   (permuted)
w0..w2 come from the SOURCE, indexed by r9
w3    comes from the AUX window, indexed by r11
```

The two are indexed differently on purpose, which is why the stride mismatch
(12 in, 16 out) is not a gap at all.

---

## 3. The recovery for this loop ALREADY EXISTS — `coli_2396c_body`

This is the largest result of the slice, and it invalidates the premise of the
last three notes. `0x23980` is not a new frontier. It is the loop **inside** an
already-recovered entry:

```text
0002396c  lda   0x020078a8, r3         <- r3 = 0x020078a8
00023974  ldob  0x00000004(g7), r15    <- slot = g7[4]
00023978  ld    0x00023944[r15*4], r8  <- r8 = src_table[slot]
00023980  mov   0, r9                 <- the loop starts here
...
000239ac  lda   0x3d4ccccd, r15       <- loop ends
```

`src/recovered/hybrid.c` already contains `coli_2396c_body` with these exact
constants:

| existing constant | value | this slice's independent measurement |
|---|---|---|
| `VF2_COLI_POLYCLUSTER_ENTRY` | `0x0002396c` | same |
| `VF2_COLI_POLYCLUSTER_INDEX_TABLE` | `0x0002394c` | the permutation — same |
| `VF2_COLI_POLYCLUSTER_CLUSTER_BASE` | `0x00000d00` | the block base — same |
| `VF2_COLI_POLYCLUSTER_SRC_TABLE` | `0x00023944` | the `r8` table — same |
| `VF2_COLI_POLYCLUSTER_SLOT_OFFSET` | `0x00000004` | `g7[4]` — same |
| `VF2_COLI_POLYCLUSTER_ROM_BASE` | `0x020078a8` | **`r3` on exit** — same |
| `const_a` | `0x3d4ccccd /* +0.05f */` | `r14`/`r15` — same, comment already correct |
| `const_b` | `0xbdcccccd /* -0.1f */` | `r13` — same, comment already correct |
| `+0x104 = scale_b + 0.05` | float add | **verified to the bit** — same |
| `+0x108 = (g7+0x1c) + 0.05` | float add | `r11` — same |
| `extra` read for slot word 3 | `ROM_BASE + 0xc + dest*16` | **exactly what I measured** — same |

So the 4th word was never a constant: `coli_2396c_body` has always read it from
memory, and my v0732p fixture simply pinned `r3` to a blank address instead of
letting `0x2396c` load the real one. **The repository was right and the v0732p
note was wrong.** The `+0.10c`/`+0x118` threshold scan and the `cmpr`/`setbit`
consumer that v0732n described as unverified are also already implemented in the
same body (see its header comment: "30-trip threshold scan into
g13+0x10c/+0x118, a 4-trip direction inner loop, three inlined 0x23878 remaps
into +0x624/+0x614/+0x618, and a 4-trip max scan into +0x644/+0x64c").

### Running the real entry confirms it

Pin **only** `g7` and let the guest load its own bases:

```text
status=ok halt=stop address ins=214 calls=10288 rets=10286 compare=equal
r3 (loaded by the guest) = 0x020078a8   expected 0x020078a8  -> MATCH
r8 (loaded by the guest) = 0x0090fa00
r9=30 r10=348 r11=28

destination words written : 120
words read from the REAL r3 window (0x020078a8..) : 30

slots matching u32(0x020078a8 + 0x0c + slot*16) : 30 / 30
slots whose w3 is NON-ZERO                    : 30 / 30
```

214 instructions, not 211 — exactly the three extra setup instructions at
`0x2396c..0x2397c`. Reproduce with `out/real2396c.py` (gitignored).

A sample of the real fourth-word column, all of them float bit patterns:

```text
slot  1 : 0x3dfdf3b6      slot  0 : 0x3dfdf3b6      slot  2 : 0x3e03126f
slot  5 : 0x3d75c28f      slot  4 : 0x3da3d70a      slot 29 : 0x3d8d4fdf
```

30 distinct small positive floats. Note slot 1 and slot 0 share a value here, so
the column is *not* injective in live state. **No semantic name is given to it.**
A window of 30 floats feeding a per-slot threshold scan is a shape, not a
meaning, and naming it would be exactly the v0729/v0730 trap.

### What this does and does not close

- **v0732n, v0732o, v0732p and the `0x0d00` block framing are superseded.** The
  block is not an unidentified structure in the `fa_player` corridor; it is the
  `0x2396c` polycluster cluster, and it already has a recovery.
- **The `0x0d00` block still gets no semantic name.** Naming the *code* is not
  naming the *data*.
- **P2 as "recover the producer" is void** — there was nothing to recover. P2's
  real remaining work is the differential below.

## 4. The real gap: `test_coli_2396c_poly_cluster` is not a differential

`tests/recovered/test_native_runtime.c:6078` is the only committed test for this
block, and it cannot catch the v0732p error because it reproduces the same
fixture mistake:

- it **synthesises an identity index table** — `rom[0x2394cu + index] = index` —
  so the real 30-byte permutation is never exercised;
- it **never runs the reference executor**. It asserts the native result against
  hand-written expectations (2618 + remap bodies, `0x3fffffff` at +0x10c), so
  it is a self-consistency test, not a differential;
- it **zeroes the cluster**, so `r7` and every 4th word are zero — precisely
  the condition that made v0732p conclude `w3 = 0`.

So the block's recovery is **unproven against the oracle** and **unexercised on
the real permutation**. That is the gap worth closing, and it is a real
regression risk: a C body that is right about the identity case and wrong about
the permuted case would pass today.

The differential must, per AGENTS.md's acceptance checklist:

- run the reference from `0x2396c` and the native `vf2_hybrid_coli_2396c_execute`
  from the same state;
- compare **all 120 cluster words**, not a sample;
- compare registers, compare state and local frame depth;
- compare instruction and call counts;
- use at least two fixtures that differ in **both** the source window and the
  ROM-resident extra window, and at least one using the **real** permutation
  read from the real ROM rather than a synthesised one.

---

## 5. The register contract, measured with `--dump-regs`

`vf2probe` gained `--dump-regs`. The trace callback was already handed the
whole `vf2_i960_cpu` and ignored it; `--trace` records only
`ip_before/ip_after/size/mnemonic`, and `compare-snapshots` only reports
*differences*. So the register poststate was genuinely unreadable before.

Seed **one register at a time** with a sentinel and check three things: was it
written, did the sentinel change the path (instruction / call / return counts
and halt reason must match the baseline), and was it a pointer. One-at-a-time
because seeding the whole file at once changes several registers that alias in
the same address computation.

### Exit state at `0x239ac`

| register | value | provenance |
|---|---|---|
| `r9`  | `30`  | `cmpinco 29` leaves the counter past its bound |
| `r11` | `28`  | `perm[29]`, the last permutation byte read |
| `r10` | `348` | `12 * 29`, the last `mulo` |
| `r4`,`r5`,`r6` | `343`,`344`,`345` | the last `ldt` triple (source words 87,88,89) |
| `r7`  | data-dependent | the last auxiliary load, `u32(r3 + 0x0c + 28*16)` |

Everything else — `r0`, `r1`, `r2`, `r8`, `r12`..`r15`, `g0`..`g14`, `fp` —
survives a sentinel unchanged, so the block does not write it. `g7` and `r8`
cannot be seeded (the loop needs their real values) and are the two registers
the recovery must be handed.

Compare state on exit is `equal` and `local_frame_depth` is `2`, both
unchanged by the loop.

### Exit state at `0x239e4` and `0x23a38`

The tail past the loop is **not** just "float constant setup", as v0732p
assumed. Measured:

| boundary | ins | what changed vs `0x239ac` |
|---|---|---|
| `0x239e4` | 222 | `r15` = `r14` = `0x3d4ccccd`, `r13` = `0xbdcccccd`, `r12` = `0xbee66666`, `r11` = `0x3d4ccccd` |
| `0x23a38` | 267 | plus `g4 = 0`, `r3 = 0`, `r13` = `0`, `r15` = `0`, `r14` = `0x0051533c` |

Decoding those bit patterns as IEEE-754 binary32:

```text
0x3d4ccccd = +0.05f     0x3dcccccd = +0.1f
0xbdcccccd = -0.1f      0xbee66666 = -0.45f
0xbf000000 = -0.5f
```

So the tail is **two immediate float constants, `+0.05f` and `-0.1f`** — and
that is the whole of it. `r12` and `r11` are *not* constants:

```text
0x239b4  ld   0x0050a010, r12        # a float read from a fixed ROM-region word
0x239bc  ld   0x0000001c(g7), r11    # a float read from the fighter struct
0x239c0  addr +0.0, r15, r14         # r15 = +0.05f
0x239cc  addr r12, r15, r12          # FLOAT add: r12 = mem + 0.05f
0x239d0  addr r11, r15, r11          # FLOAT add: r11 = mem + 0.05f
0x239d4  st   r14, 0x000000fc(g13)
0x239d8  st   r13, 0x00000100(g13)
0x239dc  st   r12, 0x00000104(g13)
0x239e0  st   r11, 0x00000108(g13)
```

Both `addr`s are **floating-point** adds here, not integer ones — confirmed
exactly rather than assumed:

```text
float(0x0050a010) = float(0xbf000000) = -0.5f
-0.5f + 0.05f = -0.45f
bits(-0.45f)   = 0xbee66666   <-- the observed r12
```

The last three digits match to the bit, so the `addr` is a float add. An
integer add would have produced `0xfc4ccccd`. And `r11` comes out `0.05f`,
which means `float(u32(g7 + 0x1c))` is `0.0f` in this fixture.

> **A decoding note, because I got this wrong twice.** The first draft of this
> note read `0x3d4ccccd` as `+0.1f` and `0xbee66666` as `-0.35f`, and then
> briefly concluded the two runs disagreed on `r12` when in fact `3202770534`
> *is* `0xbee66666` — I had mis-subtracted. Decode bit patterns with a
> `struct` call, not by hand, and re-derive any "the two runs disagree" claim
> from the numbers before believing it.

`0x23a30..0x23a34` clears `g4` and `r3` immediately before the consumer's
`ldq 0x0d00(g7)[r3*16], r4`. That last instruction is why the loop needs
`r3 = 0` — and it is also why the producer's `r3`-based load cannot survive
into the consumer: the consumer restarts the block from index 0. `r14` at
`0x23a38` is `0x0051533c`, the node reached by the two 4-deep linked-list
walks at `0x23a04` and `0x23a20`.
**Still 0 additional procedure calls at all three boundaries**, and
`procedure_calls`/`procedure_returns` are `10288`/`10286` throughout — the same
as the v0732p baseline, so the sentinel runs took the identical path.

### Is a boundary at `0x239ac` legitimate?

**Yes, and now on evidence rather than hope.** The loop is self-contained:
`0x23980` sets `r9 = 0` and `0x239a4..0x239a8` closes it, with no call and no
read of anything the loop did not itself establish. `0x239ac` starts a
different block (`lda 0x3d4ccccd, r15`) that writes to `g13 + 0xfc..0x108`,
touches `0x0050a010` and `g7 + 0x1c`, and walks two 4-deep linked lists. Those
are separate concerns and are not admitted by this slice.

---

## 6. Tool change: `vf2probe --dump-regs`

```text
--dump-regs    include the final register file and compare/local-frame state
```

Adds to the `final` record: `"compare"`, `"local_frame_depth"` and a
`"registers"` object with all 32 entries, **named the way `--set-reg` spells
them** (`r0`..`r15`, `g0`..`g14`, `fp`) so a dumped contract can be replayed as
mutations without a second lookup table.

Purely additive to the output. It does not touch the executor, the Model 2A
model, the observer or the memory path, so the oracle is unchanged and the
existing suites are unaffected.

**One real bug found while building this, and it is worth recording.** The
first version closed the `registers` object but not the outer record, so every
`--dump-regs` line was truncated JSON. It was not obvious: the truncated line
still *looks* like a complete record up to the last `}`, and the existing tests
did not exercise the flag because nothing used it yet. Caught only because the
contract sweep parsed the output with `json.loads` instead of reading it. **A
newly added output mode with no parser asserting on it is a latent gate
failure** — which is the v0732f/v0732g lesson again, in a different place.

---

## 7. What is proven and what is not

**Proven, by ROM-backed measurement on a controlled fixture:**

- the loop is 30 iterations over `r9 = 0..29`;
- the destination slot comes from the 30-byte permutation at `0x2394c`;
- source stride 12 from `r8`, contributing words 0/4/8 into `r4`/`r5`/`r6`;
- **the 4th word is `u32(r3 + 0x0c + slot*16)`, indexed by the destination
  slot, not the source index** — 30/30, 30 distinct values;
- the block is 211 instructions, 0 procedure calls;
- the exit register set is `{r4, r5, r6, r7, r9, r10, r11}` and nothing else;
- the tail's only immediate float constants are `+0.05f` and `-0.1f`; `r12`
  and `r11` are float adds of `+0.05f` onto a memory read (verified to the
  bit); and `0x23a30..0x23a34` clears `g4` and `r3`.

**Also proven, from the real entry rather than a pinned fixture:**

- `0x23980` is the loop inside the already-recovered `0x2396c` body, three
  setup instructions after the entry;
- the guest loads `r3 = 0x020078a8` itself, so the block's 4th-word window is
  **ROM-resident**, not a scratch area;
- the real window holds 30 distinct small positive floats, all 30 non-zero;
- running the real entry is 214 instructions (the 211 plus the three setup
  ones), 0 extra procedure calls.

**Not proven:**

- **the C recovery, which already exists but is unproven against the oracle.**
  `coli_2396c_body` implements all of the above, yet its only committed test
  (`test_coli_2396c_poly_cluster`) never runs the reference, uses an identity
  permutation and a zeroed cluster, and so would pass if the permuted case or
  the 4th-word source were wrong. Section 4 is the work.
- the *provenance* of the `0x020078b4` window: it is measured as ROM-resident
  and 30 floats wide, but nothing here establishes what it means.
- the neighbouring legs v0732n listed (`0x281b0`, `0x28208` joining at
  `0x2891c`, the mode-5 loop at `0x28a04`, fighter bit 6 at `0x28af8`). These
  are untouched by this slice and no claim is made about them.

`r3` and `g13` cannot be seeded — seeding either faults immediately
(`out of bounds`, 4 and 218 instructions respectively), because both are used
as base pointers inside the span. They are reported as *unproven* rather than
folded into the contract. A sentinel that faults tells you the register is
read as a pointer; it does not tell you what the block does to it, so those two
rows carry no exit-state claim. Both are now moot at the producer boundary
anyway: the real entry loads `r3` before the loop and the loop never writes it.

## 8. Next step for P2

P2's original goal — "recover the producer" — is **void**: it is already
recovered as `coli_2396c_body`. The remaining work, in order:

1. **Add the missing differential** (section 4). Reference from `0x2396c` versus
   native, comparing all 120 cluster words, registers, compare state, frame
   depth, instruction count and call count. At least two fixtures differing in
   **both** the source window and the ROM-resident extra window, and at least
   one on the **real** permutation read from the real ROM. A fixture that
   varies only the source would have reproduced the v0732p error.
2. Then the `0x281b0` / `0x28208` table heads joining at `0x2891c`, the mode-5
   loop at `0x28a04`, and fighter bit 6 at `0x28af8`.
3. Then P3/P4. "S1" is still only a decision — the sole documented S1 was
   retired at v0707, and it should be dropped rather than worked.

