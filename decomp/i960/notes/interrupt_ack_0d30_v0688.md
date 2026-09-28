# Interrupt acknowledge handler `0x00000d30`

The external `model2recomp` static analysis identified `0x00000d30` as a
separate four-instruction entry. The measured words are:

```text
0x00000d30  lda 0x00e80000,r4
0x00000d38  subo 5,0,r5
0x00000d3c  st   r5,(r4)
0x00000d40  ret
```

The clean-room recovery in `vf2_recovered_interrupt_ack_dispatch` requires an
entered i960 procedure frame at the entry, writes `0xfffffffb` to the measured
acknowledge port, performs the architectural procedure return, and accounts
for four instructions. `tests/i960/test_interrupts.c` executes the same words
through the reference executor and compares the complete live CPU and modeled
Model 2A state.

This note records a low-level unit-differential slice only. It does not promote
the nearby `0x00000e10` or `0x00000e30` entries, whose dispatch conditions and
side effects still require measurement.
