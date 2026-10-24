# v0918 — VF2.1 SCSP Timer-A cadence: 49 frames, not 50

Date: 2026-10-24. Original owner sound chip: `epr-17574.30`,
reconstructed as LOAD16_WORD_SWAP from the user's private VF2.1 ROM.
No game ROM, PCM, instruction trace dump or WAV is committed.

## Source-of-truth ROM evidence

All locations below are **MC68000 program offsets**; A5 is the
SCSP register window at 0x100000.

| PC / vector | Original instruction / data | Meaning |
| --- | --- | --- |
| vector 26 (0x68) | 0x601f16 | ISR entry at audio ROM offset 0x1f16 |
| 0x02f4 | `MOVE.W #0x00ce,0x0418(A5)` | Timer-A boot load |
| 0x1f1a | `MOVE.W #0x00ce,0x0418(A5)` | Timer-A ISR reload |
| 0x1f20 | `MOVE.W #0x0040,0x0422(A5)` | Timer-A interrupt enabled |

The opcode verifier rejects unexpected vector targets, instructions,
destination offsets, reload mismatches and disabled interrupt bits.
It accepts both private VF2 ZIP sets and a directory containing
`epr-17574.30` and emits **derived JSON only**, never ROM bytes.

```sh
python3 tools/python/audio_scsp_tima_rom.py /private/vf2-rom.zip
python3 tools/python/test_audio_scsp_tima_rom.py
```

The SCSP User's Manual §4.2.11 says the three timer counters
increment at 44,100Hz / 2^prescaler and expire at 0xFF.
The low byte of 0x00CE is decimal 206, and prescaler is zero:

```text
(0xFF - 0xCE) * (1 << 0) = 49 sample frames
44,100 / 49 = 900 possible Timer-A expiries per second
441,000 / 49 = 9,000 possible ticks during ten seconds
```

**This corrects the former arbitrary experiment of 50 frames /
882 ticks per second** (50 frames vs the ROM-programmed 49).
The register-derived period is hardware-supported. Precise IRQ
delivery, interrupt-entry 68000 cycle overhead, instruction
preemption relative to PCM, and the complete SCSP model
**remain unverified**. Never call this a captured board recording.

Public documentation, treated as independent hardware reference
rather than verbatim code to copy:

- SEGA SCSP User's Manual, §4.2.11 Timer:
  https://www.infochunk.com/saturn/segahtml_en/hard/scsp/hon/p04_2b.htm
- MAME SCSP device source, timer callback and reload semantics:
  https://github.com/mamedev/mame/blob/master/src/devices/sound/scsp.cpp

## Newly measured owner-only 900Hz Akira replay

The existing 10-second owner-ROM test now has **separate opt-in modes**
that keep the old 882Hz path intact:

```sh
vf2_m68k_l3_selection_tests /private/vf2-rom --verify-akira-tima-10s
vf2_m68k_l3_selection_tests /private/vf2-rom \
    --akira-tima-native-wav-dir /private/out-c
vf2_m68k_l3_selection_tests /private/vf2-rom \
    --akira-tima-original-wav-dir /private/out-68000
```

Before enabling the 49-frame interval it compares the ROM opcode
locations and boot/live SCSP Timer-A register value with the
model's timer formula. Then it executes the unchanged original
L2 ISR and command-drain interpreter, feeding either the
**native C** or original-MC68000 `0x2242` PCM walker. SCSP
renders 49 frames after each tick (no forced time stretching).

Measured locally against the owner's ROM and CI Linux binary for
commit `032ea7a` on 2026-10-24:

| Metric | Old 50-frame experiment | Timer-derived 49-frame mode |
| --- | ---: | ---: |
| Ticks in 10s | 8,820 | **9,000** |
| 32-voice descriptor steps | 282,240 | **288,000** |
| Original 68000 walker instructions | 4,984,294 | **5,078,020** |
| Original 512-byte PCM refills | 2,160 | **2,205** |
| Original audio command drains | 456 | **468** |
| SCSP-model stereo frames | 441,000 | 441,000 |
| Full C vs original RAM/SCSP mismatches | 0 | **0** |

The strict 49-frame test exited **0**:
`AKIRA_COMPOSITE_10S_STRICT parity=1 ticks=9000 passes=288000
original_pages=2205 original_steps=5078020 modeled_cadence=900Hz
realtime_verified=0`.

Both 900Hz WAV previews exited 0 and are **bitwise identical**
(1,764,044 bytes each, stereo 44,100Hz). SHA-256 of *both*:

```text
463d20c14383c408fd5b55dd78c36675ed3ca3c3bc06a76a92dd0e5f36de5b3e
```

The 900Hz audio is **not byte-identical** to the 882Hz preview:
419,726 of 441,000 stereo frames differ, as expected from
different note and refill event times. This is a significant
timing-model correction, not an audio fidelity comparison
to the arcade hardware. Neither ROM nor WAV is published in Git.

## Next research boundary

The exact Timer-A register reload and nominal interval are
established; **not yet established** is whether the actual driver
runs one complete 32-descriptor main-loop scan per Timer-A
interrupt (or many scans between interrupts). Avoid pushing
the 900Hz rate into the production audio session until the
MC68000 main-loop cycle budget and interrupt/preemption state
are recovered. The independent SCSP FM/DSP reference
remains necessary to establish true arcade sound accuracy.
