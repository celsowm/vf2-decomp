# `fa_coli` fighter-1 scan matrix — v0684

The parked `0x221e8` whole-task snapshot was swept with the fighter-1
bit-8 flag, `field_0804` bit 15, `field_0821` in `{0,1,4,5}`, and
`field_0822` in `{0,1,16,256}`. The same 128-case domain was applied to the
reference executor and to the native `fa_coli` dispatcher.

All reference cases reached `0x10dcc`. The observed clusters were:

| flags | result |
| --- | --- |
| neither | `9214/18/19` |
| fighter 0 only | `9385/17/18`, or `9391/17/18` for its measured scan-5 case |
| fighter 1 only | `9385/17/18`, or `9391/17/18` for its measured scan-5 case |
| both | `9520/18/19`, or `9526/18/19` when the measured scan-5 case is active |

The native fixture now restores the same snapshot for every case, applies
both fighter addresses explicitly, and compares CPU registers, condition
state, procedure counters, local state and mutable Model 2A memory. All 128
fighter-1 cases match exactly. The scan-5 fighter-1 witnesses required one
additional measured accounting correction in the single-live shape and one
additional correction in the bilateral shape when fighter 0 remains on
scan-0.

This does not establish the unmeasured simultaneous nonzero scan fields,
other fighter fields, or neighboring flag combinations. Those paths remain
unsupported until separately measured.
