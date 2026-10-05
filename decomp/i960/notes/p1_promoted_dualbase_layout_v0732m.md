# P1: promoted dual-base provisional fighter layout (v0732m)

**Evidence only. No C struct is added yet, and no field is given a semantic
name.** This records the offsets that now satisfy the v0704/v0706 dual-base
discipline, with the **corrected** fighter base from
`p1_fighter_bases_retraction_v0732k.md`:

```text
fighter0 = 0x00510980      fighter1 = 0x00512980      window = 0x2000
```

All offsets below are expressed from that base, so absolute address is
`base + offset`. `0x510000`-relative readings (the v0729/v0730 notes) are
this table minus `0x980`.

## The whole visible struct is dual-base

`out/trace-both.jsonl` has **144 distinct offsets, 341 memory events, in each
fighter window, and all 144 are shared.** Every one of the top 40 offsets by
access count is `base_count == 2`. This is not a block that happens to be
shared — the struct is symmetric by construction, which is exactly what
"same layout for both fighters" should look like.

| offset | R | W | width | guest IPs | note |
|---|---|---|---|---|---|
| `0x0000` | 60 | 0 | 4 B | `0x23a94` | the dominant reader; 60 reads per base |
| `0x0004` | 4 | 0 | 1 B | `0x23974`, `0x22404` | byte width, unlike its neighbours |
| `0x0018` | 2 | 2 | 4 B | `0x2380c`, `0x2381c`, `0x23824`, `0x23834` | read+write, balanced |
| `0x0020` | 2 | 2 | 4 B | `0x23810`, `0x23820`, `0x23828`, `0x23838` | read+write, balanced |
| `0x01a4` | 70 | 0 | 4 B | `0x23a9c`×60, `0x238ac`, `0x2229c`, `0x222a0`, `0x2241c` | **most-accessed field; the scenario's `fighter0_flags` / `fighter1_flags`** |
| `0x01a8` | 4 | 0 | 2 B | `0x22410`, `0x23398`, `0x2339c` | |
| `0x01aa` | 4 | 0 | 2 B | `0x238b4`, `0x2243c` | |
| `0x01f4` | 4 | 0 | 4 B | `0x23524`, `0x23528`, `0x2364c` | start of a 3-tuple |
| `0x01f8` | 4 | 0 | 4 B | `0x23524`, `0x23528`, `0x2364c` | |
| `0x01fc` | 4 | 0 | 4 B | `0x23524`, `0x23528`, `0x2364c` | end of the 3-tuple |
| `0x0644` | 2 | 2 | 4 B | `0x23bac`, `0x237c8`, `0x237d8` | read+write |
| `0x064c` | 2 | 2 | 4 B | `0x23bb0`, `0x237d0`, `0x237e0` | read+write |
| `0x0650` | 2 | 2 | 4 B | `0x2383c`, `0x23854`, `0x23858` | read+write; taint reported `+ 0x0650 bit 31` |
| `0x06dc` | 2 | 2 | 2 B | `0x223b4`, `0x224b4` | 2-byte twin of the `0x0d00` family |
| `0x0808` | 4 | 0 | 2 B | `0x238b8`, `0x22440` | |
| `0x0820` | 4 | 0 | 1 B | `0x238c0`, `0x22450` | byte width |
| `0x0d00..0x0edc` | 2 | 2 each | 4 B | `0x2399c`, `0x23a38` | **the 120-offset block** |

## The `0x0d00` block, promoted

```text
offset        0x00000d00 .. 0x00000ee0
length        120 offsets
byte_size     480
width         4 B, all offsets
reads/writes  240 each PER BASE  (480 each in the merged roll-up)
ip_overlap    1.0
guest IPs     0x0002399c, 0x00023a38  (both write, both read, every offset)
base_count    2   <-- PROMOTED
```

Every v0704/v0706 axis is satisfied: same offset from both bases, same access
width, repeated access from the same two guest functions, and a consistent
read/write role. The 240/240 balance per base is what separates "both bases
touch it" from "one base touches it twice as often", and it is now pinned in
`p1_real_trace_demo.py` (ctest #117) so a regression fails loudly.

## What the v0730 taint note now means, concretely

With the corrected bases those dependencies are measured, and they line up
with this table:

```text
fighter0 + 0x01a4 bit 8      <-> 0x01a4 is the flags word (70 reads, 5 IPs)
fighter1 + 0x01a4 bit 14
fighter0 + 0x01a4 bit 0      <-> same word, different bit
fighter1 + 0x05b8 bit 0      <-> 0x05b8, below the top-40 cut
fighter0 + 0x0650 bit 31     <-> the 0x0650 read+write quad above
```

So the struct carries a **flags word at `+0x1a4`** that several branches
depend on, plus a `+0x0650` word with a sign-bit dependency. That is the
"fighter-flag branches" the v0730 note said the `0x0d00` block does **not**
feed — and both statements are now simultaneously true and consistent: the
block is not a flag input, but the struct around it certainly has flags.

## Provisional struct, evidence-only

Per AGENTS.md, the layout may be recorded with neutral `field_xxx` names. It
is recorded here rather than in C, because nothing yet consumes it and a C
struct with no consumer is premature:

```c
/* PROVISIONAL. Offsets are dual-base-measured (fighter0 = 0x510980,
 * fighter1 = 0x512980). Names are neutral by policy: no field below has
 * independent evidence for a semantic name. */
struct vf2_fighter_provisional {
    uint32_t field_0000;            /* 60 reads @ 0x23a94 */
    uint8_t  field_0004;            /* 1-byte */
    /* 0x0008..0x0017 unobserved */
    uint32_t field_0018;            /* read+write */
    uint32_t field_0020;            /* read+write */
    /* 0x0024..0x01a3 unobserved */
    uint32_t field_01a4;            /* FLAGS: 70 reads, 5 guest IPs,
                                     * branch input on bits 0/8/14/18/23 */
    uint16_t field_01a8;
    uint16_t field_01aa;
    /* 0x01ac..0x01f3 unobserved */
    uint32_t field_01f4;            /* 3-tuple with 01f8, 01fc */
    uint32_t field_01f8;
    uint32_t field_01fc;
    /* 0x0200..0x0643 unobserved */
    uint32_t field_0644;            /* read+write */
    uint32_t field_064c;            /* read+write */
    uint32_t field_0650;            /* read+write; branch input on bit 31 */
    /* 0x0654..0x06db unobserved */
    uint16_t field_06dc;
    /* 0x06de..0x0807 unobserved */
    uint16_t field_0808;
    /* 0x080a..0x081f unobserved */
    uint8_t  field_0820;            /* 1-byte */
    /* 0x0821..0x0cff unobserved */
    /* The block: 30 padded 4-word slots, not a flat 120-word array.
     *
     * Producer (0x23984..0x239a8): 30 iterations of
     *   r11 = table[0x2394c][r9]          (a permutation of 0..29, in ROM)
     *   load  a 12-byte packed triple from (r8)[r9*12]
     *   store it at g7 + 0x0d00 + r11*16
     * i.e. a 3-word -> 4-word expansion with a permuted slot order. That
     * formula predicts 60 of 60 observed destination offsets.
     *
     * Consumer (0x23a38): reads the same addresses and folds each slot index
     * into a bitmask at (g13)+0x10c via setbit r3, after comparing against
     * two float constants at (g13)+0xfc / (g13)+0x100.
     *
     * Same 480 bytes and same 120 measured 4-byte offsets either way; the
     * [30][4] form is preferred because it matches the iteration count and
     * predicts where the three loaded words land. Adjacent 16-byte slots
     * abut, so a 4-byte-granularity contiguity detector cannot see the slot
     * structure - which is why the block first measured as "120 contiguous".
     *
     * See p2_slot_permutation_v0732o.md. NO differential exists for either
     * loop yet. */
    uint32_t field_0d00[30][4];
};
```

**The `0x0d00` block still has no name.** It is written by two guest
instructions and read back by two others, at the same offsets for both
fighters, with no branch depending on it (per the v0730 taint note, which
this slice re-validates). Structurally it is a scratch/descriptor array. It
is **not** renamed to `pose`, `state` or `animation` — that would be exactly
the "naming fields too early" trap.

## What is not done

- **No C struct is added.** Nothing consumes this layout yet, and a struct
  with no consumer is not a recovery. The C-side follow-up belongs to P2-P4.
- **No field is semantically named.** `0x01a4` is called "flags" only because
  branches test bits in it; that is a *role* observation, not a name for what
  the bits mean.
- The `0x05b8` word the taint note names is below the top-40 cut, so it is
  not in the table above. It is dual-base by the same 144/144 evidence.
- `0x0100..0x0cff` and most of `0x0008..0x0fff` are unobserved in this
  corpus. "Unobserved" means this trace does not touch them, not that they
  do not exist.

## Anti-traps

- Do not re-derive these offsets from `0x510000`. Everything here is
  `0x980` higher than the v0729/v0730 tables.
- Do not rename `field_0d00`. Two instructions write it and two read it, and
  no branch depends on it. That is a scratch/descriptor array until
  something else says otherwise.
- Do not treat `base_count == 2` as sufficient on its own. It says the same
  *offset* was touched from both bases; the width, the guest IPs and the
  read/write balance are what make it a shared layout rather than a
  coincidence.
