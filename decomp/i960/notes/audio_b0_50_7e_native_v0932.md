# v0932 — B0/50 7E end-of-sequence shutdown translated to native C

Date: 2026-10-25. Integrated in `master`.

## Verified using the owner's original ROM locally

The user-owned original VF2.1 sound ROM was inspected locally,
in CPU-visible 68000 byte order. No ROM files, samples, WAVs,
or verbatim ROM instruction streams were committed.

The B0 selector `0x50` dispatches to `0x1C0A`; the
argument `0x7E` enters its channel shutdown path at
`0x1D5E`. The version fingerprint of the routine's
complete 470-byte area is compared before live admission.

Native helper: `vf2_sound_board_apply_b0_end_7e`.

For each of 32 voice records, a flagged (bit 6) voice is
selected only when its channel is 0..8 or 15. The sound
driver writes `SCSP[slot+0x0B]=0x1F`, then
`SCSP[slot]=0x10`, clears `RAM[0x2000+slot]`, and zeros
voice flag and channel bytes; other bytes remain intact.
`slot` is a **big-endian byte offset**, not a slot index.

After all voices, the driver clears shared `0x251E`,
the first 9 bytes at `0x4220` and `0x4240`, and the
first **9 words (18 bytes)** at `0x4200`. The prior
unpublished attempt incorrectly used `0x252E` and only
9 bytes at `0x4200`; neither mistake is in the
committed implementation.

## Validation

The separate private, constrained 68000 branch interpreter
was run against the original user-owned ROM with three
synthetic input states: idle, all armed, and mixed. Exact
full-RAM, SCSP register-image and ordered SCSP writes
matched the separately written reference in 3/3 cases.
This validates the isolated path, not full arcade sound.

Public ROM-free tests cover the native helper, 32 voices,
channel and flag predicates, preservation of unrelated bytes,
zeroing of exactly 9 words, shared counter selection and
atomic refusal for an invalid slot.

The owner-ROM differential
`test_b0_50_7e_original` also runs original program opcodes
through the repository's full 68000 stepper and checks
the native helper's complete RAM and SCSP model state.
It runs only with privately supplied original ROM:

```sh
cmake -S . -B build -DVF2_BUILD_TESTS=ON -DVF2_ROM_DIR=/private/vf2
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`BE 50 7D`, `BE 50 7F`, original stream scheduling and PCM
render parity remain open; no new WAV is claimed here.
