# v0934 — Native A0 low-header music init at original 0x1E98

Date: 2026-10-25. Scope: sound CPU descriptor initialization only.

## Original sound ROM evidence

Local audit of the user's `epr-17574.30` (CPU-visible,
word-swapped) confirmed that family index `0x10` selects
ROM row `0x608F24` (max 0x001F) with **32 indexed records**, all with a first byte
`0x10` (bit 7 clear). However, six indices
`0x19..0x1E` have a **0xFFFFFFFF sentinel** rather
than a valid stream root. The remaining 26 indices
resolve to a mapped stream root, with index `0x1F`
aliasing the first record. Never claim 32 distinct
audible songs from this indexed row.
Akira's **AD 10 04** selects record `0x609C36` and stream
root `0x615773`; the next pointer stored into sound RAM
is **`0x609C3E`** (address of the second root word).
No original ROM/music sample bytes are included in this commit.

The real ROM path at 0x1E98 reads that record and uses
the low-bit7 arm (0x1EAE..0x1ED2). The high-bit7 arm for
AD23/AD24 (0x1ED4..0x1F02) remains unchanged.

Native helper `vf2_sound_board_apply_a0_low_music_record`:
- reject unreadable, non-0x10 records and invalid first
  stream pointers *before mutating state*;
- clear the primary 32-byte sound descriptor at RAM 0x3000;
- store status 0x88, 16-bit timer 1, big-endian first
  stream pointer at +4, big-endian record+8 at +8;
- preserve every unrelated descriptor and SCSP state.

Normal sound `dispatch_next` admits **valid** AD 10
music records if the original A0 0x1E98..0x1ED3 and
0x1F04..0x1F13 bodies match their private-derived
FNV-1a 64-bit fingerprints. Original 0xFFFFFFFF
root sentinels, invalid pointers and other ROM revisions
remain fail-closed; the original table max-gate
is still respected.

## Verification and limits

The public ROM-free C test uses a synthetic low-header
record to verify descriptor zeroing, status/timer/pointers,
zero changes on bad header, out-of-ROM root, explicit
original FF sentinel, and truncated record. The owner-ROM C-vs-68000 test runs the original
body until 0x1ED2 for music selectors 00/04/1F and
compares the entire descriptor, SCSP and unrelated
RAM, excluding 68000 JSR stack scratch.

The existing original-ROM Akira first-command test is
updated from the former "C refuses at 1E98" contract.
It now also compares native first descriptor and
original CPU descriptor at drain exit 0x265C.
The rest of that test still uses original 68000 to
sequence the music; native C is NOT claimed to render
the song and no original WAV is committed.

**Next unresolved feature:** the native L2 stream worker
for low-header music, starting with descriptor 0's
0x615773 stream command. Authentic PC/cycle/IRQ cadence,
SCSP DSP and PCM playback parity remain independent
open tasks.

### v0934b private census check

A new owner-ROM-only census requires 26 resolvable
row selectors and six FF sentinel entries; every
sentinel is tested against the native helper for an
atomic unsupported return with untouched 32-byte
descriptor. The owner-ROM header/pointer data remains
private. This corrects the initial 32/32 assumption
without broadening the actual C dispatch.
