# Lift/physics-moment witness (v0704, scouting)

## Park

`out/tmp-lift-27ce0.vf2snap` parks at `0x27ce4`, one instruction into
the `0x27ce0` guard (prior session parked it; verified: 1 step from
`0x10dcc`-until runs). A 3000-step trace+memory window from the park
(2112 accesses) characterizes the physics moment. Raw trace consumed
for analysis only, not committed.

## Measured guard shape (disasm, ROM-backed, no semantics invented)

```asm
0x27ce0  ldos  +0x01aa(g7) -> r15
0x27ce4  cmpobne 1, r15, 0x27d00
0x27ce8  ldos  +0x0c4e(g7) -> r15
0x27cec  cmpobne 1, r15, 0x27d00
0x27cf0  ldos  +0x0c4c(g7) -> r15
0x27cf4  ldos  +0x01a8(g7) -> r14
0x27cf8  cmpobne r14, r15, 0x27d00
0x27cfc  ret
0x27d00  call  0x28184 ...
```

Early return unless `+0x1aa == 1`, `+0x0c4e == 1`,
`+0x1a8 == word(+0x0c4c)`; otherwise `0x28184` chain. New candidate
offsets `+0x0c4c` / `+0x0c4e` (beside the `+0x0c50` float-result
field); not observed at runtime in this window (guard short-circuit
took one path), so note-only, no struct change.

## Lift-window field census (fighter0 unless noted)

- `+0x01a4` dual-base **RW** (7R+1W; new ips `0x29130`/`0x1ac00`/
  `0x1b440`/`0x1b46c`/`0x1b4c8`/`0x17714`): first write sighting
  outside the corridor; third dual-base window.
- `+0x01aa` mixed 1B/2B again (`0x27ce0` guard + `0x28184`/`0x28274`/
  `0x283ec` downstream; plus `0x1ad84`/`0x1b318` as in v0701).
- `+0x0bdc` R (2nd window), `+0x0804` R (2nd window): corroborated,
  still fighter0-only -- struct comments updated, no promotion.
- `+0x0be6` 2B RW (`0x291c0`/`0x29210`/`0x29020`/`0x166e0`): NEW,
  single-window, note-only.
- `+0x186c..0x1880` six 4B RW words (`0x16fbc`/`0x17024`/`0x17040`):
  NEW region beyond current struct size, single-window, note-only.
  A second park must confirm before any struct extension.
- `+0x002c`/`+0x0034`/`+0x0080` 4B RW (`0x179xx`): NEW, note-only.

## Differential contract for future native work

Drive the guard inputs (`--set` bytes at `+0x1aa`, `+0x0c4e`,
`+0x1a8`, `+0x0c4c-word`) from the `0x27ce4` park and compare
reference-vs-native through `ret 0x27cfc` vs `call 0x28184`: 2x2x2
input matrix x branch outcome + downstream steps. The guard is 5
instructions; the contract is bounded and excludes the `0x28184`
chain until the guard itself is native.
