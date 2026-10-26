# v0935 — first original Akira L2 B-class packet, an instruction-level native primitive

Date: 2026-10-25. Committed to master. The **whole music L2 tick remains unsupported** in normal dispatch.

## Original ROM basis

The owner-owned original sound 68000 ROM was inspected in CPU-visible (LOAD16_WORD_SWAP)
order. Akira music selector AD 10 04 now installs descriptor 0: status 88, timer 1,
next byte stream 0x615773, second music-record field 0x609C3E.

The first 3 bytes at that stream root select the B-class applier at
original ROM PC 0x1FFC, which first writes command/payload to the four-byte
command-ring entry, then clears **the top bit of the third byte**
at 0x2040 before writing the third-byte payload at 0x211C. If that
bit was set, it **sets bit 0 in descriptor status byte 0** via
the 0x2048 arm. The command's fourth sound-ring byte is untouched.

The original 0x211E..0x2128 path advances the 12-bit four-byte
ring write cursor and pending count. The A3 stream pointer was
postincremented three times, from 0x615773 to 0x615776, but
the worker descriptor pointer +4 is unchanged at stop-PC 0x212C.
The important distinction is that A3 is **temporary register
state**, not a committed stream cursor. Subsequent worker
control-flow may consume many more events in the same L2 tick.

## Native implementation

`vf2_sound_board_probe_akira_l2_first_b` is an explicitly-called,
ROM-bound developer diagnostic. It handles **only** the
0x615773 first music-root command shape (BE 50 FE). It:
- verifies descriptor 0 status 88, timer 1, bounded ROM source,
  four-byte ring alignment and pending FIFO capacity **before mutation**;
- executes the first original decrement and B-class packet emission;
- produces ring `BE 50 7E` with its pre-existing fourth byte intact;
- updates stream descriptor byte0 to 89, byte1 to BE, timer to 0;
- updates ring cursor by +4 mod 0x1000 and count +1;
- reports ephemeral A3=0x615776 without committing it to descriptor +4.

This primitive does **not** run from
`vf2_sound_board_maintain_streams`: allowing only one packet
from a music record and declaring the entire L2 IRQ complete would
change original behavior. Normal full music worker paths
still fail-closed transactionally.

The public ROM-free C tests cover the packet, count,
cursor, descriptor mutation, fourth-byte preservation,
non-reentrancy, invalid cursor/count and source mutation
refusal. No Sega code, music stream bytes or PCM are committed.

An owner-only ROM-backed differential starts the original
68000 at PC 0x1F7C on this same descriptor and stops
**before** PC 0x212C. It compares complete RAM and modeled
SCSP state, A3 register against the native report and
source+3 and the original descriptor. It is not part of
the ordinary public ROM-free CI, and still needs execution
with the user's original ROM set.

## Next exact frontier

Continue the original 68000 at 0x212C after this packet,
trace whether descriptor bit0 selects the next worker instruction,
and reconstruct the **whole bounded music stream** until the
original returns, including multi-packet ring emissions and
pointer state. Only then wire a completed tick into native
`vf2_sound_board_maintain_streams`, followed by the SCSP
and real interrupt scheduling separately. No fresh Akira WAV
or audio-fidelity claim is made here.

## v0936 — continuation through the first three music B packets

At original PC 0x212C, the test previously stopped after
**one** packet, but bit0 of stream descriptor status makes
the 68000 retry at 0x1F96 while preserving A3.

Locally reading the user-owned sound ROM, the follow-up
commands are BE 20 80 and BE 10 72 00. The second emits
BE 20 00, with BCLR bit7 signalling another retry. The
third uses the *special* 0x10 branch: because descriptor
byte +0C is zero, it copies the third input byte 0x72
to descriptor +0E and **reads the next 00 as the actual
command's third output byte**. It emits BE 10 00,
clears the status bit0, and reaches PC 0x212C with
A3=0x61577D. The record +4 pointer is unchanged.

The new explicit native
`vf2_sound_board_probe_akira_l2_next_two_b` requires the
previous BE 50 7E already in the sound ring and validates
the source stream, bit0 status, timer zero, descriptor
+0C, queue count/cursor and capacity **before mutation**.
It appends the two extra ring entries without touching
their fourth bytes, writes +0E=0x72, and returns A3
separately rather than prematurely committing it.

ROM-free C tests cover the exact three packets, source
mutation refusal, malformed state and circular ring
wrap. An owner-only differential invokes the **real
68000 sound ROM** in one-instruction resumable slices,
waiting for the **third** arrival at PC 0x212C
(`sound_ram[command_count] == 3`), then compares the
entire RAM image, full modeled SCSP and A3 to the
independent native two-step helper. The differential
has not been executed in public CI without private ROM.

Crucial limit: after those first three packets the
same L2 worker continues into 9F-class commands; its
full return, pointer-update and real CPU-cycle timing
are still unrecovered. Do not interpret these three
packets as an entire Timer-A IRQ or claim native
continuous music / a new WAV.

## v0937 — first original class-9 music event

The local owner VF2.1 sound-ROM jump table at 0x1FBC
has a class-9 entry at 0x1FE0, a JMP (d16,PC) to
**0x2080**. The first Akira class-9 stream event
is 9F 48 F7 immediately after the three previously
recovered B-class events, at 0x61577D.

The original 68000 0x2080 class-9 routine writes
the opcode and following byte, BCLR #7 on its third
byte, sets status bit0 on a previously set high bit,
and joins the shared 0x211C emitter. At original
stop PC 0x212C the fourth music command is
**9F 48 77**, status becomes 0x89, pending ring
count rises from 3 to 4, cursor from 12 to 16,
and ephemeral A3 points to 0x615780. The music
descriptor stream pointer at +4 remains 0x615773.
The ring's fourth byte stays untouched.

A new standalone `vf2_sound_board_probe_akira_l2_first_9f`
performs only this fourth original packet, with strict
source, descriptor, three-prior-packet, cursor, count
and ROM bounds guards. This is NOT a full L2 tick.
The public synthetic unit covers success, mutated
source refusal, capacity rejection, re-entry rejection
and 4KiB cursor wraparound. The opt-in owner-ROM
differential single-steps the real Motorola 68000
to the *fourth* visit to 0x212C and compares full
RAM, SCSP and temporary A3 with the three C probes.
It is not executed in public ROM-free GitHub Actions.

The next original stream events are more class-9
packets followed by mixed command/control events.
They still need their own bounded worker treatment,
correct pointer and countdown finalization, and a
real-cycle IRQ scheduler before native PCM rendering.
No new Akira WAV or acoustic parity is claimed.

## v0938 — fifth and sixth Akira packets, both class 9

Local private VF2.1 sound ROM audit confirmed that after
the fourth ring packet (9F 48 77) the original music
stream contains **9F 2E C0** and **9F 33 FF**, at
0x615780 and 0x615783. The original 68000 class-9
applier at 0x2080 strips bit 7 of third bytes and
sets descriptor status bit0. As a result, at its
SIXTH arrival at PC 0x212C the ring's first 6
three-byte packets are:

1. BE 50 7E
2. BE 20 00
3. BE 10 00 (saved descriptor+0E = 72)
4. 9F 48 77
5. **9F 2E 40**
6. **9F 33 7F**

The temporary 68000 A3 has advanced 19 stream bytes
from 0x615773 to **0x615786**, while original
descriptor+4 stays unchanged. Status is 0x89,
count is six, write cursor is 24 for an empty
starting ring. Fourth bytes are never overwritten.

New ROM-free primitive
`vf2_sound_board_probe_akira_l2_next_two_9f`
requires the entire 19-byte original source pattern,
a valid primary descriptor, the four previously
emitted packet slots, two available FIFO slots
and an aligned ring cursor. Any mismatch refuses
atomically before touching RAM/SCSP. It writes
both new packets together, including the wraparound
case, and reports ephemeral A3. The ordinary L2
scheduler is left unchanged and fail-closed for
this still-unfinished music worker.

Public tests cover output, exact preserved fourth
bytes, bad ROM, FIFO exhaustion, re-entry and
the 0xFFC/0x000 split. An opt-in owner-ROM test
replays REAL 68000 instructions in one-instruction
slices until the sixth visit to PC 0x212C and
checks full RAM, modeled SCSP and A3 against the
four original-derived native micro-steps. This
owner-ROM test has not run in public CI.

The immediately following original stream begins
with **C1 81**, introducing a different command
class and no longer the plain B-/9F three-byte
grammar. The next frontier must recover that
class-C handler and complete the original L2
worker return / stream pointer update; only
after full L2 transaction parity can it drive
a real-time native music renderer. No new WAV
or hardware-fidelity claim is made.

## v0939 — 7th/8th/9th original music packets (new class C)

The owner's VF2.1 sound-ROM L2 dispatch table at
0x1FBC maps 68000 classes **C** and D to **0x20AC**
and class 9 to **0x2080**. Original stream bytes at
0x615786..0x61578C are:
`C1 81 | 9F 2B EE | C3 82`. The class-C handler
strips the top bit of the second byte, sets status
bit0 and forwards a **ZERO third byte** to the
shared 0x211C packet emitter. Thus the new three
packets at original ninth stop-PC 0x212C are:

7. C1 01 00
8. 9F 2B 6E
9. C3 02 00

The 68000's A3 is now 0x61578D, the descriptor+4
stream pointer remains 0x615773, status stays 0x89,
pending ring count becomes nine and cursor 36
in the initially empty ring.

An explicit opt-in C micro-probe
`vf2_sound_board_probe_akira_l2_first_mixed_c`
validates 26 bytes of original root content,
all six prior ring packets, status/count/cursor
and FIFO capacity **before mutation**, emitting the
three packets atomically. It is not connected to
the normal stream scheduler. ROM-free regressions
check exact output, fourth-byte preservation,
source corruption / capacity refusal, repeat
invocation refusal and 4KiB circular wrap.
A private-ROM differential runs genuine 68000
one-instruction slices until the ninth original
packet's 0x212C instruction boundary and compares
complete RAM/SCSP and ephemeral A3 against the
five native micro-probes. It is opt-in and not part
of public ROM-free CI.

The next original event is class-C **C8 8D**. The
full music L2 worker and hardware-correct SCSP
audio remain unimplemented. A nine-packet
prefix is not a playable song nor proof of a
completed original Timer-A interrupt.

## v0940 — tenth original Akira L2 class-C packet (candidate)

The previous ROM-backed audit identifies the following source pair after
packet #9: **C8 8D** at ephemeral A3 `0x61578D`. The already documented
original class-C branch at 68000 PC `0x20AC` masks the argument's top bit,
sets descriptor status bit0, and emits a zero third byte via `0x211C`.
The expected tenth ring entry is therefore **C8 0D 00**; after emission
A3 is expected at `0x61578F`, count 10 and cursor 40 in an empty ring.

The opt-in `vf2_sound_board_probe_akira_l2_tenth_c` validates the full
28-byte Akira source prefix, descriptor identity and state, exact nine
previous 3-byte ring entries, one available ring slot and the circular
write cursor before touching RAM. It preserves the fourth byte of the
ring entry and the descriptor stream pointer (which the original CPU
has not yet committed at this instruction boundary). ROM-free tests
cover normal state, source corruption, a corrupt prior packet, a full
queue, repeated invocation, descriptor preservation and FFC -> 000
wraparound.

The private `vf2_m68k_cdispatch_tests` executable now includes an
original-instruction 68000 differential stopping at **the tenth**
`0x212C` emission (not its first visit). It compares *all* sound RAM,
modeled SCSP registers and ephemeral A3 against the six native micro-steps.
This test needs the owner's ROM locally; its pass is **not claimed**
without running it. ROM-free/public CI similarly must be verified from
the resulting build rather than assumed.

This is an isolated prefix recovery only. Next frontier: recover the
post-tenth stream grammar and entire L2 worker return/descriptor-pointer
commit under original 68000 differential, then connect the proven
worker to IRQ-driven music rendering. No audio/WAV or hardware parity
claim is made here.

## v0941 — Akira 11th/12th genuine source entries (B0 and 0 classes)

Owner ROM `epr-17574.30`, CPU-visible 16-bit word-swapped, SHA-1
`b6c0c3f0473bd7fc3ef4f5146110dfcc899a5af9`, independently
inspected on October 26, 2026. At ephemeral A3 `0x61578F`, the
next six source bytes are two successive original events:

- Packet #11: B0 0A C0, class B -> original handler `0x1FFC`.
  With bit7 stripped: **B0 0A 40**.
- Packet #12: 07 FF C0, class 0 -> original handler `0x2080`.
  With bit7 stripped: **07 FF 40**.

The real ROM's class table at `0x1FBC` has two PC-relative
`JMP` entries targeting exactly these two handlers; the owner-ROM
differential checks both routes explicitly before execution.
Both source third bytes have bit7 set, retaining descriptor bit0.
The twelfth shared emitter boundary `0x212C` is expected with
ephemeral A3 `0x615795`, packet count 12 and empty-ring cursor 48.

The native opt-in `vf2_sound_board_probe_akira_l2_next_b0_07` validates
34 original source bytes, the whole ten-packet pending history, exact
descriptor state and two aligned circular FIFO slots before **any** write.
It atomically appends both packets, preserves fourth bytes, and keeps the
persistent descriptor+4 pointer untouched. ROM-free C regressions cover
success, source corruption, missing prior packet, full queue, repeat call
and FFC-to-000 wrap. The owner-only `vf2_m68k_cdispatch_tests` comparison
now walks genuine original 68000 instructions until count 12 at PC 212C
and compares all 512 KiB of RAM, complete modeled SCSP and temporary A3.
**Do not claim differential parity until the local original-ROM test passes.**

Immediately next in the original source is a class-8 command at A3
`0x615795`. The class-8 jump table selects handler `0x2052`; its
complete argument consumption, control-flow and any extra side effects
must be recovered before implementing packet #13. The whole L2 worker
and native sound remain unsupported; no music/WAV fidelity claim.

## v0942 — Akira 13th/14th: class-8 fixed payload and retry-bit clearing

Independent inspection of owner-owned VF2.1 sound CPU ROM
`epr-17574.30` in LOAD16_WORD_SWAP order identified a new 68000
handler at original PC `0x2052` for class 8. It consumes exactly
**two source bytes**, files the opcode and bit7-masked argument
and then uses `MOVEQ #0x7F,D2` before the shared ring emitter.
At `0x615795` the next sequence is **80 B6 | 07 F8 0A**:

- 13th packet (class 8, handler `0x2052`): **80 36 7F**,
  A3 becomes `0x615797`, descriptor status bit0 is set.
- 14th packet (class 0, handler `0x2080`): **07 F8 0A**,
  A3 becomes `0x61579A`, no input high-bit tag. The original
  `0x1F96` retry clear leaves descriptor status **0x88**.

At the 14th `0x212C` boundary, the empty-ring count is 14,
write cursor 56 and descriptor's persistent stream pointer
remains `0x615773`. Since bit0 now clears, the original CPU
will take a **different control-flow arm** after `0x212C`,
not another automatic packet retry.

The bounded native `vf2_sound_board_probe_akira_l2_next_80_07`
requires the exact 39-byte original prefix, previous 12 ring
packets, valid status/counter/slot alignment and room for two
writes before mutating anything. It preserves both fourth bytes.
ROM-free C regression: normal execution, refused corrupted source,
refused prior packet, capacity rejection, re-entry refusal, and
FFC-to-000 circular wrap. A new opt-in owner-ROM differential
single-steps genuine original 68000 instructions until the
**14th** arrival at PC `0x212C`; checks both jump-table targets,
A3, all RAM, modeled SCSP, status bit and emitted count.
The private differential has **not** been run yet.

**Next frontier:** trace the original post-14th no-retry arm
`0x2138` and determine which bytes it consumes, how it alters
descriptor+4 and whether it returns to the L2 scheduler.
No continuous music or DAC-accurate PCM is claimed.

## v0943 — first Akira music descriptor reaches original pre-RTS pointer commit

In the verified owner ROM, after packet #14 the original 68000
enters the non-retry path at `0x2138`, rather than `0x1F96`.
It reads a nonzero variable-length countdown. Source bytes at
A3 `0x61579A` are **AC C7**. Original 68000 opcodes at
`0x2144..0x2158` strip bit7 of the first byte, apply
`LSL.W #7,D2`, `OR.B (A3)+,D2`, then commit via
`MOVE.W D2,2(A2)` and `MOVE.L A3,4(A2)`.

The resulting music descriptor updates, **before RTS at 0x2158**:
- Countdown word +2 becomes `0x16C7` (5831).
- Persistent source pointer +4 becomes `0x61579C`.
- Fourteen prior ring packets, their fourth bytes, status 0x88,
  count 14, cursor 56 and descriptor +0E=72 stay unchanged.

An opt-in native `vf2_sound_board_probe_akira_l2_first_return`
now checks the 41-byte original source fingerprint/prefix and all
fourteen prior ring slots before writing either descriptor field.
ROM-free tests include success, source corruption, corrupted prior
ring history, refusal on repeat, byte-for-byte preserved FIFO and
a circular-history case. Owner-ROM differential executes original
68000 opcodes starting at worker PC 0x1F7C until PC `0x2158`,
then compares full sound RAM, modeled SCSP and temporary A3 against
all nine bounded native probes. This needs a local original-ROM test
run before claiming parity; public CI is ROM-free.

**Scope:** recovery of one complete initial music **descriptor worker
body through the pre-RTS boundary**. The higher-level L2 scan loop,
other descriptors, Timer-A scheduling, SCSP rendering and arcade DAC
are *not* recovered by this change. Next prove the worker's integration
inside a whole interrupt tick, without prematurely declaring playback.
