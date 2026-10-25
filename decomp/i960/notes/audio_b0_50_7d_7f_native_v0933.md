# v0933 — native C end-sequence commands B0/50/7D and 7F

Date: 2026-10-25. Direct to `master`.

## Evidence and scope

The user's original VF2.1 sound ROM was examined **locally**
in CPU-visible Motorola 68000 word-swapped order. The jump
table selector 0x50 points to 0x1C0A; D0=0x7D branches to
0x1CCC, D0=0x7F to 0x1C24. No original ROM, opcode dump,
sample data or copyrighted audio is present here.

A restricted instruction-by-instruction Python interpreter was
extended only for the two actual branches and compared against
an independently implemented C-equivalent reference algorithm
on 4 synthetic states per branch. All **8/8** scenarios
matched complete 512KiB RAM, 4KiB SCSP register snapshots
and SCSP write address/value ordering. This is isolated
branch evidence, not full game audio parity.

## Original branches

**D0=7D:** iterate 32 voice records from RAM 0x2800
(stride 16), clear only voices with flag bit 6 and channel
**9..14**. Per voice key-off: write SCSP[slot+0x0B]=0x1F,
SCSP[slot]=0x10, RAM[0x2000+slot]=0, voice[2]=0,
voice[4]=0. After all voices, clear 6 bytes starting at
0x4229 and at 0x4249, and **six big-endian words**
0x4212..0x421D; zero counter 0x252E. All channels
0..8 and 15 remain unchanged in those tables.

**D0=7F:** iterate all 32 flagged voices without channel
restriction; same per-voice key-off. For each stopped
voice, if original channel==15 OR original flag bit 4
is clear, decrement shared 0x251E and sweep 32 records:
for every voice[9] !=FF and voice[9] > selected
voice[9], decrement that byte. The sweep uses updated
values from earlier voice shutdowns. Finally clear 16
bytes of each table at 0x4220 and 0x4240 and **16
big-endian words** (32 bytes) from 0x4200.
Counter 0x252E remains unchanged.

Both C paths preflight **all** affected voice slot
addresses before mutation, so an invalid slot refuses
without partial key-off. The live dispatch uses these
native translations only when the original 470-byte
B0/50 routine fingerprint matches the attached ROM;
unknown variants and arguments remain fail-closed.

## Verification

Public ROM-free tests assert selection, age/count
behavior, table coverage, per-voice SCSP stores and
atomic invalid-address refusal. The owner-ROM differential
in `tests/m68k/test_cdispatch.c` compares complete
sound RAM and SCSP model state against original M68K
execution on eight scenarios. That owner test still
needs to be run using the user's original files.

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON \
  -DVF2_ROM_DIR=/path/to/private/vf2
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The 7E branch was already recovered in v0932, so all
three important B0/50 end-control subcommands now have
independent native C implementations. This does not
resolve the Akira A0 music stream start, hardware-timed
scheduler, SCSP DSP behavior or PCM parity.
