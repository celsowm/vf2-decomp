# Candidate-field dual-base promotion (v0702)

## Promoted

`field_001c` (4B) reaches full dual-base status:

- coli corridor: ip `0x239bc` reads `fighter0 + 0x001c` and
  `fighter1 + 0x001c` symmetrically (1x each, same width, same ip) in
  the bilateral whole-task trace (`out/trace-both.jsonl` ->
  `out/fields-both.json`: 144 candidate fields, bases
  `0x00510980`/`0x00512980`; raw JSONL stays git-ignored).
- player corridor: v0701 RW at `0x1791c`/`0x17ac0`/`0x17ac8`/
  `0x17b44`/`0x16568` (fighter0).

`field_01a8` (2B) dual-base corroborated: coli ips `0x22410`/
`0x23398`/`0x2339c` (both bases, R) alongside v0701 player RW.

`field_01a4` (4B) dual-base in a third window: `coli-midbody-22210`
-> `0x10dcc` (56 steps) reads both fighters at `0x2229c`/`0x222a0`/
`0x2241c`.

The earlier bilateral trace independently confirms dual-base
`+0x0000`/`+0x0004`/`+0x0018`/`+0x0020`/`+0x01a4`/`+0x01a8`/`+0x01aa`/
`+0x001c`, consistent with the header (no type/width changes needed).

## Not promoted (measured mechanism, not neglect)

`field_0804` / `field_0bdc` stay single-corridor (player only):

- The `pre14288` -> `0x10dcc` trajectory issues exactly ONE `0x19ef8`
  call (from `0x14288`); F0 goes deep, F1 stays shallow.
- Setting F1 `+0x1a4` to admitted mask `0x59f` yields a byte-identical
  stream to F1-natural (both 14634 steps, final F1 `+0x1a4` keeps the
  mask while F0 normalizes to `0x200`).
- Continuing past `0x10dcc` idles in a self-branch (no second `19ef8`
  within 100k steps); F1's deep pass needs a frame advance, for which
  no probe workflow is established (`--raise-irq`/`--enter-interrupt`
  vectors unknown). That workflow is the prerequisite, not more
  same-park traces.
