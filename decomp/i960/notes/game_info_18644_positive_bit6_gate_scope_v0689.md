# `fa_game_info` `0x18644` positive bit-6 gate scope (v0689)

## Question

`AGENTS.md` lists "positive bit-6 compositions remain explicitly unsupported"
in `fa_game_info` around `0x18644` as a recovery frontier, and
`game_info_1645c_positive_bit6_fail_closed_v0127.md` states that single/pair
high-bit masks without bit 8, high+low mixes, and wider triples/quads "fail
closed rather than silently borrowing the generic bit-6 corridor".

This note records a measurement that shows the `native_bit6_fighter_path`
predicate is an **accounting selector**, not an admission gate. Compositions it
excludes already match the ROM through the generic bit-6/bit-31 corridor, so
widening the predicate adds no recovered behaviour and the stated frontier is
not real for those families.

## Method

`build/Debug/vf2i960.exe` was built from pristine `master` and driven with the
committed ROM-backed validator over the compositions that v0127 excludes:

```sh
python decomp/i960/tools/validate_game_info_state4.py \
  build/Debug/vf2i960.exe roms/vf2 \
  --state 8 --extra-bit 6 --extra-bit 21 --mask 24 \
  --threshold 0 --base out/state8-positive.boundary.vf2snap
```

`--mask 24` selects `bit6 + bit21` with the `--extra-bit` ordering used by the
validator, i.e. the isolated high-21 mask without bit 8 and without low bits.

## Result

| composition | validator result on pristine `master` |
| --- | --- |
| `bit6 + high21` (no bit8, no low) | `12/12 exact` |
| `bit6 + high21 + bit1` | `12/12 exact` |
| `bit6 + high21 + high26 + high29` (no bit8) | `12/12 exact` |
| `bit6 + high21 + bit26 + high29 + high30 + high31` | `12/12 exact` |

Every excluded family tested matched the ROM across the three physical
distributions, both countdown values and both mode-bit-6 settings, with
snapshot, counter and live-state equality. A temporary probe build that widened
`measured_positive_state8_bit6_mask` to "any bit 6 composition inside the
measured bit set `{1,2,4,6,8,21,26,29,30,31}`" produced identical results, so
the widening is behaviour-neutral for the tested domain.

## Interpretation

- `native_bit6_fighter_path` only enables the mask-specific dispatcher
  accounting corrections (the `high_3_4` list, `0x140/0x150`, `0x142..0x156`
  and the range predicates). Compositions outside those masks need no
  correction, and the generic corridor already reproduces the ROM exactly.
- Consequently the v0127 "fail closed" boundary does not manifest as
  `VF2_ERROR_UNSUPPORTED` for high-bit-without-bit8 or high+low families. The
  genuinely unverified area is narrower than the documentation suggests.
- No code change was committed from this experiment: widening the predicate
  cannot be shown to add recovered behaviour, and a wider predicate would also
  be *less* fail-closed for compositions that were never individually
  measured.

## Follow-up

The useful residual question is whether a compact predicate can replace the
hand-written `high_3_4` mask list **while preserving** the existing
corrections. That requires proving the corrections are keyed exactly on the
measured masks, which the exhaustive-run driver
`decomp/i960/tools/validate_game_info_positive_bit6_sweep.py` can drive:

```sh
python decomp/i960/tools/validate_game_info_positive_bit6_sweep.py \
  build/Debug/vf2i960.exe roms/vf2 out/posbit6.jsonl \
  --base out/state8-positive.boundary.vf2snap --shard 0 --shards 8
```

The driver is reproduced-tested by
`decomp/i960/tools/test_validate_game_info_positive_bit6_sweep.py`.
