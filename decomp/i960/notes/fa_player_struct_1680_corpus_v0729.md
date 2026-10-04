# fa_player struct: 0x1680 contiguous 4B field cluster (v0729)

## Evidence path that produced this

The v0729 factory chain surfaced a contiguous block of 4-byte
fighter-relative fields in the player corridor that is stable across
two independent corpus runs. The next fa_player decomposition can use
this as the entry evidence for the struct promotion that P1 in the
v0729 plan calls for.

## Commands (reproducible)

```sh
# Step 1: surface fighter offsets touched by the corpus
python tools/python/infer_structs.py \
  out/trace-both.jsonl \
  --base fighter0=0x510000 --base fighter1=0x520000 \
  --window 0x2000 --min-count 2 --limit 50

# Step 2: same on the alternate trace
python tools/python/infer_structs.py \
  out/trace-f0.jsonl \
  --base fighter0=0x510000 --base fighter1=0x520000 \
  --window 0x2000 --min-count 2 --limit 50

# Step 3: rank the edges that touch them with frontier v2
python tools/python/frontier.py \
  out/trace-both.jsonl out/trace-f0.jsonl \
  --fighter-base 0x510000 --fighter-base 0x520000 \
  --fighter-window 0x2000 --limit 6 --json \
  --output out/frontier-1680-v0729.jsonl
```

## What surfaced

Both traces (`trace-both.jsonl` and `trace-f0.jsonl`) hit the same
fighter-window offsets in the 0x1680-0x16ec range, all 4-byte R/W,
all attributed to the same pair of guest IPs (0x2399c and 0x23a38).

| offset | accesses | width | role | dominant IPs |
| --- | --- | --- | --- | --- |
| 0x1680 | 2 (1R/1W) | 4B | RW | 0x2399c, 0x23a38 |
| 0x1684 | 2 (1R/1W) | 4B | RW | 0x2399c, 0x23a38 |
| 0x1688 | 2 (1R/1W) | 4B | RW | 0x2399c, 0x23a38 |
| ... | ... | ... | ... | ... |
| 0x16d0 | 2 (1R/1W) | 4B | RW | 0x2399c, 0x23a38 |
| 0x16d4 | 2 (1R/1W) | 4B | RW | 0x2399c, 0x23a38 |
| 0x16d8 | 2 (1R/1W) | 4B | RW | 0x2399c, 0x23a38 |
| ... | ... | ... | ... | ... |
| 0x16f4+ | 2 (1R/1W) | 4B | RW | 0x2399c, 0x23a38 |

The **v0729h** `contiguous_fighter_blocks` method on the v2 frontier
factory automatically aggregates the per-IP trace pattern into a
single block detector run:

```sh
python tools/python/frontier.py \
  out/trace-both.jsonl out/trace-f0.jsonl \
  --fighter-base 0x510000 --fighter-base 0x520000 \
  --fighter-window 0x2000
# then in Python:
#   blocks = frontier.contiguous_fighter_blocks(width=4, min_count=1, min_length=3)
```

Output on the dual-trace corpus:

```text
width=4 -> 3 blocks
  0x00001680..0x00001860  len=120  size=480B  overlap=1.0  top_ips=['0x2399c', '0x23a38']
  0x00000998..0x000009a4  len=3    size=12B   overlap=1.0  top_ips=['0x2380c', '0x2381c']
  0x00000b74..0x00000b80  len=3    size=12B   overlap=1.0  top_ips=['0x23524', '0x2364c']
```

So the v2 factory finds the **block 0x1680..0x1860 (length 120,
480 bytes, 1.0 IP overlap)** automatically from the dual-trace
corpus — both ends of the block extend further than the manual
estimate in the initial version of this note, because the union of
both traces covers a wider range than either trace alone.

## Provisional struct promotion (evidence only)

```c
struct vf2_fighter_candidate_block_v0729 {
    /* All fields: provisional, evidence-only from corpus dual-trace. */
    uint32_t field_1680;   /* 4B, RW, IP 0x2399c + 0x23a38 */
    uint32_t field_1684;   /* 4B, RW, IP 0x2399c + 0x23a38 */
    uint32_t field_1688;   /* 4B, RW, IP 0x2399c + 0x23a38 */
    /* ... 26 more contiguous 4B RW fields through 0x16f4 ... */
};
```

This is the candidate the v0729 plan called out at P1 as the next
multi-corridor struct promotion. It is the first contiguous 4-byte
block the v2 factory surfaced in the player corridor.

## Cross-references to prior notes

- `frontier_v2_player_corpus_v0729.md` listed `0x1680`, `0x1684`,
  `0x1688` as candidates; this note promotes them to a **contiguous
  4B block candidate** because every offset 0x1680..0x16f4 has the
  same access pattern in both corpus runs.
- The IP attribution (0x2399c, 0x23a38) is **stable across both
  traces**, so the "same-offset same-IP" half of the v0704 dual-base
  discipline is satisfied on the per-trace axis; the
  dual-base (fighter0 vs fighter1) axis still requires a different
  trace that hits fighter1 explicitly, which the existing
  `trace-both.jsonl` does not (the trace hits fighter0 only).

## Untouched

- **No semantic naming.** Field names remain `field_1680` style until
  behaviour or evidence supports a `position` / `velocity` / `pose`
  rename (AGENTS.md "Naming fields too early" trap).
- **No new recovery claim.** This is a measurement note; the next
  recovery slice must still run a `vf2cmp|native-*` differential on
  the chosen branch.
- **Single base only.** Both traces touch fighter0 base addresses
  only; the dual-base provenance step remains open for a future
  slice with a fighter1-inclusive trace.

## Next evidence pass

1. Run `taint.py --until 0x2399c` on `out/trace-both.jsonl` to
   characterise the dependent branch for IP 0x2399c (the dominant
   reader/writer of this block).
2. Run `infer_structs.py` on a fighter1-inclusive trace once one
   exists, to confirm dual-base provenance for the block.
3. Add a per-field note documenting the IP attribution if a third
   independent trace reproduces the block.