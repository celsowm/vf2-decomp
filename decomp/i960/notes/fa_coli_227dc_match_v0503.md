# `fa_coli` `0x227dc` type-5 match tail

The measured bit-13/bit-3, scan-1, `g7+0x844` bit-30 shape was rerun with
`g7+0x848 = 1` and main-data table entry `0x02014d6d` changed only at its
record type byte from type 2 to type 5. The type-5 walker returns record
`0x02014d75`; the reference then takes the nine-instruction match tail and
returns to `0x22240` after 61 instructions from `0x225cc`.

Observed final state includes `g0 = 0x02014d75`, `g1 = 5`, equal condition
state, `g7+0x198 = 0x1100000f`, and the record byte at `+3` copied to
`g7+0x822`. A ROM-backed unit now compares captured reference and native
snapshots, including CPU, condition, procedure and writable Model 2A state.

Only this measured type-5 record shape is admitted through the existing
bit-30 gate. `g7+0x848 = 0` and other unmeasured match compositions remain
fail-closed.
