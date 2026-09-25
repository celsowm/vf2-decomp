# v0676 — selector-1 continuation through `0x144b8`

The measured nonzero `fa_rob` selector-1 snapshot enters the continuation at
`0x1a048`, with the live player base `0x00510980`, selector `1`, player
`+0x1a4 = 0x00000163`, data pointer `0x02014d6d`, and selector table
`0x02c00000`. The recovery admits only this measured shape and keeps all
unmeasured selector-1 neighbors fail-closed.

The reference reaches `0x144b8` after 3,932 instructions, with three calls
and four returns. The recovered corridor models the measured selector stream
expansion at `0x26ef0`, the `0x27130` shared call, the nested `0x27250` /
`0x28780` output exchange, the 20 triple copies, the 24-word scalar result
loop, the fighter field stores, and the serialized final TGP command-window
state. The modeled TGP boundary uses the normal Model 2A access API; a
configured callback therefore remains authoritative and unsupported callback
behavior remains fail-closed.

The live differential fixture compares complete CPU state, condition state,
procedure counters, and mutable Model 2A state at `0x144b8`. It then resumes
the existing v0674 return-tail fixture through `0x1463c`. The exact checkpoint
and trace used to derive this note remain ignored local evidence and are not
part of the repository.
