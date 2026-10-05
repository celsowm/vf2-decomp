# P2 step 3: the 0x23984 producer loop is fully verified — 30/30, padding word is zero (v0732p)

> ## RETRACTED IN PART at v0732q — the "padding word is zero" conclusion is WRONG
>
> **Wrong claims, precisely:**
>
> - `w3 = 0` / "the padding word is zero". It is **not** a constant. `stq r4`
>   stores the quadword `r4` = `{r4, r5, r6, r7}`, and `r7` was **loaded one
>   instruction earlier** from `0x0000000c(r3)[r11*16]`. Measured 30/30 with 30
>   distinct values: `w3 == u32(r3 + 0x0c + slot*16)`.
> - The mechanism sentence "the `stq` stores a 64-bit quadword register whose
>   high word is zero". There is no zero high word; the high word is `r7`, a
>   load.
> - The framing of `r3` as "pinned only to keep the read in range" whose
>   "poststate contribution is unmeasured". `r3` is the **base of the 4th
>   word's source**; it is central to the rule, not incidental.
>
> **Why it measured zero:** the fixture pinned `r3 = 0x0100a000` but never
> wrote anything at that address, so the whole auxiliary window read as zero.
> The pre-clear proved the 4th word is *written*; it could not prove *what* is
> written, and "zero" was promoted from "what this fixture contained" to "what
> the code stores". This is the v0732k episode again in a new place: a fixture
> that failed to vary the variable under test.
>
> **Still correct and retained:** the 30-iteration count, the `0x2394c`
> permutation, source stride 12 from `r8`, destination stride 16 from
> `g7 + 0x0d00`, words 0/4/8 of each slot, the 211-instruction / 0-call count,
> and the pre-clear discipline itself — which is what made the error
> *detectable*.
>
> See `p2_producer_contract_v0732q.md`.

**The block's producer is a completely determined, measured rule.** This is the
block the whole `fa_player` corridor has been treating as an opaque 480-byte
array. It is 30 iterations of a permuted 3→4-word expansion, and every clause
of that sentence is now measured rather than inferred.

**No C recovery is committed yet.** What follows is a proven rule plus the
fixture that proves it, which is the precondition for writing the C. The
recovery is the next step, not this one.

## The verified rule

```text
for r9 in 0..29:
    r11    = u8(0x2394c + r9)                 # ROM-resident permutation table
    w0,w1,w2 = u32(source + r9*12 + 0)        # the 12-byte packed triple
                      + 4)
                      + 8)
    w3     = 0                                # <-- the padding word
    dest   = g7 + 0x0d00 + r11*16
```

`source` is `r8`; `g7` is the fighter base. 30 iterations, **211 instructions,
0 procedure calls** for the whole block.

## The fixture

`vf2probe` accepts at most `VF2_PROBE_MAX_MUTATIONS` = **128** mutations per
invocation, and `--set-reg` draws on the same budget as the memory mutations.
The fixture therefore runs in three snapshot passes:

| pass | mutations | what it does |
|---|---|---|
| 1 | 93 | seed the 360-byte source at `0x1008000` with `0x100 + i` per word, pin `g7`/`r8`/`r3` |
| 2 | 120 | pre-clear the 120 destination words to `0xDEADBEEF` |
| 3 | 3 | run `0x23980 -> 0x239ac` with `--memory-trace` |

```sh
build/Debug/vf2probe.exe --rom-dir roms/vf2 \
  --snapshot out/sixth-fresh.vf2snap \
  --set-ip 0x23980 --set-reg g7=0x00510980 \
  --set-reg r8=0x01008000 --set-reg r3=0x0100a000 \
  --until 0x239ac --max-steps 4000 --memory-trace --trace
# status ok, halt_reason "stop address", ip 0x239ac, 211 instructions
```

Reproduced by `out/producer.py` (gitignored; rebuilds the three snapshots from
`out/sixth-fresh.vf2snap`).

## The result

```text
destination words written : 120
source words read         : 90            (= 30 x 3, the whole source)
slots whose 3 words == the source triple : 30 / 30
distinct w3 values across slots          : {'00000000': 30}
```

A sample of the image, `perm` = the table value, `r9` = the loop index:

```text
slot perm  r9  dest        w0        w1        w2        w3
   1   0  0x00511690  00000100 00000101 00000102  00000000
   0   1  0x00511680  00000103 00000104 00000105  00000000
   2   2  0x005116a0  00000106 00000107 00000108  00000000
   5   3  0x005116d0  00000109 0000010a 0000010b  00000000
   4   4  0x005116c0  0000010c 0000010d 0000010e  00000000
  ...
  28  29  0x00511840  00000157 00000158 00000159  00000000
```

Note `slot 1` holding the triple for `r9 = 0`, and `slot 0` holding the triple
for `r9 = 1` — the permutation is doing exactly what the table says, and the
consecutive source words land in the *same* 4-byte run
(`0x100..0x102`, `0x103..0x105`, ...) while the *slot* order is shuffled. That
is the whole point of the table: the destination order differs from the source
order.

## The open question from v0732o, answered

v0732o flagged the 4th word of each slot as unknown - "one source word per
slot is NOT covered by the triple". It is now measured: **all 30 padding words
are `0x00000000`.**

Two alternatives were ruled out by the same measurement:

- **Not** the next overlapping source word (which would have been `0x100 + r9*3
  + 3`, a distinct value per slot). The pre-clear value `0xDEADBEEF` does not
  appear either, so **every** slot word really is written, and the 4th is
  written as zero.
- The `stq` stores a 64-bit quadword register whose high word is zero, so the
  3 loaded words plus a zero land in the 16-byte slot. That is the mechanical
  reason, and it is consistent with `ldt` loading a 12-byte triple into the low
  3 words of the quadword pair `f6`/`r12`.

## Why the pre-clear mattered

Without pass 2, a slot word that was never written would show whatever the
snapshot happened to hold, and a zero would be indistinguishable from
"pre-existing zero". Seeding the destination with `0xDEADBEEF` and observing
`0x00000000` in every 4th word is what proves the zero is *written* rather than
*left over*. This is the same discipline as the v0727 digit-cell work: a value
that must be proven written, not merely observed.

## What is proven and what is not

**Proven, by ROM-backed measurement with a controlled fixture:**

- the loop is 30 iterations over `r9 = 0..29` (`cmpinco 29, r9, r9`);
- the destination index comes from the 30-byte permutation table at `0x2394c`;
- the source stride is 12 bytes from `r8`;
- the destination stride is 16 bytes from `g7 + 0x0d00`;
- each slot receives the 3 source words plus a written zero;
- the block is **211 instructions, 0 procedure calls**.

**Not proven, and required before the block may be admitted as a recovery:**

- the register poststate. The fixture proves the *memory image*, not `g7`, `g9`,
  `r11`, `r4` or the compare state on exit. A recovery has to match those too.
- `r3` is used by `ld 0x0000000c(r3)[r11*16], r7` but its destination
  (`r7`) is never consumed before the loop exits at `0x239ac`, so the fixture
  pins `r3` only to keep the read in range. **Its poststate contribution is
  unmeasured** and the loop's tail may well use it.
- the block does **not** stand alone. `0x239ac` continues into
  `0x239ac..0x239e0` (the float constant setup for the consumer) and then
  `0x23a38`. A recovery boundary at `0x239ac` is only legitimate if the
  surrounding frame is also pinned.

## Next step for P2, in order

1. Extend the fixture to `0x239ac -> 0x23a38` and capture the **register**
   poststate, so the recovery has a full contract rather than a memory image.
2. Write the C recovery for `0x23984..0x239a8`. It must **read** the
   permutation from ROM at `0x2394c` — embedding the 30 bytes as a C array
   would be a generated lookup table, which AGENTS.md rule 7 forbids.
3. Add a committed differential in the shape of
   `tests/recovered/test_player_28918_live.c`: reference vs native on
   registers, memory, instruction count and call count, with the fixture
   reproducible from a committed entry point.
4. Then the consumer at `0x23a38`.

## Tool finding: vf2probe mutation budget

`VF2_PROBE_MAX_MUTATIONS` is 128 and the budget is shared between `--set-reg`
and `--set-u8/16/32`. Crossing it used to make the probe print its whole usage
block with no indication of which argument was rejected, which cost real time
while building this fixture. `tools/vf2probe/main.c` now says what went wrong:

```text
vf2probe: too many mutations - at most 128 of --set-reg / --set-u8 /
--set-u16 / --set-u32 are accepted, and 128 were already given
```

`--read-u32` has the same 128 ceiling (`VF2_PROBE_MAX_READS`) and still fails
silently; that one is left alone here because no current fixture needs more
than 128 reads, and changing two limits in one commit is how a tool change
stops being reviewable.
