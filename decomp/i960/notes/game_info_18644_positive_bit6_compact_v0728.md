# Positive state-8 bit-6: full symmetric matrix + compact rule (v0728)

## Sweep (8 shards, fresh boundary)

Regenerated the fighter-metadata scenario from the live `0x164ac`
boundary (`out/state8-posbit6-v0727.json` +
`.boundary.vf2snap`: state 8, bits 1,2,4,6,8,21,26,29,30,31,
threshold 0) and swept the complete positive-bit-6 symmetric space:
all 512 compositions with bit 6 present, each through the 12-case
matrix (distributions `(flags,0)`/`(0,flags)`/`(flags,flags)` x
countdown `{0,1}` x mode-bit-6 `{0,1}`) = **6144 native/reference
pairs, 6144/6144 MATCH, zero DIFFs** on current master.

Clustering (by committed `measured_positive_state8_bit6_mask`):

- admitted + exact: 54 masks = 16 no-high (bit6 +/- lows +/- bit8),
  5 singles + 10 pairs + 10 triples + 5 quads (bit6+bit8, no lows),
  8 all-five-high (bit6+bit8, any lows);
- excluded + exact: 458 masks (highs without bit 8, highs mixed with
  low bits, foreign-free) match through sibling gates;
- diff: none.

## Compact rule (no behavior change)

The committed 15-entry `high_3_4` table is exactly all C(5,3)+C(5,4)
triples/quads with bit6+bit8 and no lows (verified constant-by-constant
in `prove_bit6_compact.py`). It folded into the structured predicate as
counted `three_high_bits`/`four_high_bits` arms (clear-one-bit-at-a-time,
same style as the existing one/two-bit terms); the table is deleted.
`prove_bit6_compact.py` (now committed under `decomp/i960/tools/`)
ports the old and new predicates and proves equivalence over the full
2048-case input domain (all 2^10 measured-bit combos x foreign-bit
presence): 2048/2048 identical, 54 admitted.

## Differential re-proof (new binary)

- all 54 admitted masks x full 12-case matrices: 54/54 exact
  (648 pairs exercise the counted rule and the keyed corrections);
- random 64-mask excluded boundary sample: 64/64 exact.

The v0689 residual is answered: no simpler rule is inferable from
zero-diff data, and none is needed — the jagged admitted/excluded
boundary IS the counted rule, proven equivalent and re-proven
differentially. Unilateral mixed compositions outside the symmetric
sweep stay fail-closed per the sibling-gate evidence.
