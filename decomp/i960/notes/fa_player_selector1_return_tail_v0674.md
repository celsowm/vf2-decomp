# v0674 — selector-1 return tail at `0x144b8`

The measured selector-1 continuation has a bounded tail after the
`0x26ef0`/`0x27130` calls return. Starting at `0x144b8`, the neutral
state-exchange arm reaches `0x1463c` after 35 instructions, with no
additional procedure call or return.

The recovered tail models the measured `0x144b8..0x1450c` exchange stores:
fighter-0 `+0x1aa`, `+0x61e`, `+0x626`, fighter-1 `+0x822`, `+0x654` and
`+0x62a`, followed by the common exit clearing both `+0x198` fields. It also
preserves the measured local registers, comparison result and complete
counter state. State-16/state-27 successor arms remain unsupported.

The ROM-backed fixture resumes `out/selector1-before-144b8.vf2snap` and
compares complete CPU and mutable Model 2A state through `0x1463c`. The
checkpoint and traces remain ignored derived evidence; no proprietary data is
committed.
