#ifndef VF2_FIGHTER_CANDIDATE_H
#define VF2_FIGHTER_CANDIDATE_H

#include <stdint.h>
#include <stddef.h>

/*
 * Provisional fighter/object layout inferred from repeated
 * base+offset memory accesses in vf2probe --memory-trace streams.
 *
 * Evidence:
 *  - out/state8-case.jsonl (fighter0=0x00510980, fighter1=0x00512980,
 *    flags=0x42/0x2, countdown 0, threshold 0) 88 accesses, 39 unmatched
 *  - out/state8-case-bilateral.jsonl (0x140/0x140) same 88 accesses
 *  - out/state8-case-high.jsonl (0x200140/0x400140) same 88 accesses
 *  - out/state4-case.jsonl (state 4, 0x4000/0x4000) 86 accesses
 *  All four traces via make_game_info_probe_scenario.py boundary
 *  (native-fifth-dispatch derived snapshot via build_boundary).
 *  Inferred via tools/python/infer_structs.py --scenario --window 0x2000
 *  with bases fighter0/fighter1.
 *
 * Criteria for inclusion (AGENTS.md: neutral naming until semantic proof):
 *  - same offset from fighter0 and fighter1 (base_count==2)
 *  - same access width across traces
 *  - repeated access from same guest IPs
 *  - stable R/W role across state transitions where measured
 *
 * Offsets observed in all four traces (stable):
 *  +0x0000  4B R  ips 0x000189b0,0x00018a04,0x00018a08 (6x)
 *  +0x01a4  4B R  ips 0x00018648,0x0001864c (4x)  -- documented fighter state/flags
 *  +0x019f  1B R  ip  0x00018a28 (2x)
 *  +0x01f4  4B R  ips 0x0001865c,0x00018664,0x000186a0,0x000186a8 (8x)
 *  +0x01fc  4B R  ips 0x0001866c,0x00018674,0x00018690,0x00018698 (8x)
 *  +0x05b4  2B R  ips 0x00018738,0x00018750[,0x00018788 state8] (4-6x)
 *  +0x05b8  4B RW ips 0x000189c4,0x000189c8,0x00018a24 (4R+2W state8, 0R+2W state4)
 *  +0x05f4  4B RW ip  0x00018680 R + 0x000189f8 W (2R+2W / 1R+2W)
 *  +0x0844  4B R  ips 0x0001897c,0x0001899c (4x)
 *  +0x1200  1B W  ips 0x000164d4,0x000164e0 (2W) -- fa_game_info first-dispatch zero
 *
 * Additional evidence in state-8 only (not yet cross-state stable):
 *  - fighter+0x5b6 1B comparison (single probe at 0x00018698, see AGENTS example taint target)
 *  - fighter+0xb24 2B RW bit15 accumulation for masks 0x00210000/0x00218000
 *    (decomp/i960/notes/game_info_1645c_full_state8_bit16_compounds_v0126.md)
 *
 * v0302 additions (decomp/i960/notes/fighter_candidate_layout_v0302.md):
 *  Coli mid-body bilateral (bases 0x00510980 / 0x00512980):
 *   +0x0004 1B R  ip 0x00022404
 *   +0x01a8 2B R  ip 0x00022410
 *   +0x06dc 2B W  ip 0x000223b4
 *   +0x0821 1B R  ip 0x000222a4
 *  fa_player 0x29414 g7 corridor (single object 0x00510980, types 6/8/10):
 *   +0x0084 4B R  ip 0x00029538
 *   +0x017c 2B RW ips 0x000294e8/0x000294f0 (path A)
 *   +0x018a 2B RW ips 0x000294dc/0x000294e4 (path A)
 *   +0x01aa 2B R  ip  0x000294a0 (unsigned window)
 *   +0x01b1 1B R  ip  0x00029414 (type 0/6/8/10)
 *   +0x0614 2B R  ip  0x00029504 (path B mask 0x9000)
 *   +0x0c50 4B W  ip  0x00029544 (float result)
 *  Taint: branch 0x0002949c depends on fighter0 + 0x01a4 bit 19
 *
 * v0351 live coli midbody g0=1 (out/coli-live-midbody-g01, bases
 * 0x00510980 / 0x00512980, g13=0x00514940) bilateral via
 * tools/python/fighter_offsets.py --fighter-base:
 *   +0x0026 2B R, +0x01a4 4B R, +0x01a8 2B R, +0x06dc 2B RW,
 *   +0x0000 4B R, +0x0004 1B R, +0x0821 1B R.
 * Unilateral on this drive: +0x0828, +0x1234, +0x06d4/+0x06d8,
 * +0x0700, +0x082a, +0x019f, +0x01aa, +0x05b4/+0x05b8.
 *
 * The window 0x2000 covers all above (max offset 0x1200).
 * Field names remain field_XXXX until independent behavioral proof
 * assigns semantic names (health, animation_state, etc. are forbidden
 * until evidence-backed). Do not rename without separate differential proof.
 */

#define VF2_FIGHTER_CANDIDATE_WINDOW 0x2000u

/* Stable offsets measured above */
#define VF2_FIGHTER_OFF_0000 0x0000u
#define VF2_FIGHTER_OFF_0004 0x0004u
#define VF2_FIGHTER_OFF_0026 0x0026u
#define VF2_FIGHTER_OFF_0084 0x0084u
#define VF2_FIGHTER_OFF_017C 0x017cu
#define VF2_FIGHTER_OFF_018A 0x018au
#define VF2_FIGHTER_OFF_019F 0x019Fu
#define VF2_FIGHTER_OFF_01A4 0x01a4u
#define VF2_FIGHTER_OFF_01A8 0x01a8u
#define VF2_FIGHTER_OFF_01AA 0x01aau
#define VF2_FIGHTER_OFF_01B1 0x01b1u
#define VF2_FIGHTER_OFF_01F4 0x01f4u
#define VF2_FIGHTER_OFF_01FC 0x01fcu
#define VF2_FIGHTER_OFF_05B4 0x05b4u
#define VF2_FIGHTER_OFF_05B8 0x05b8u
#define VF2_FIGHTER_OFF_05F4 0x05f4u
#define VF2_FIGHTER_OFF_0614 0x0614u
#define VF2_FIGHTER_OFF_06DC 0x06dcu
#define VF2_FIGHTER_OFF_0821 0x0821u
#define VF2_FIGHTER_OFF_0844 0x0844u
#define VF2_FIGHTER_OFF_0C50 0x0c50u
#define VF2_FIGHTER_OFF_1200 0x1200u

/* Widths as observed in the measured corridor (state-8, 0x18644 prefix) */
#define VF2_FIGHTER_WIDTH_0000 4u
#define VF2_FIGHTER_WIDTH_0004 1u
#define VF2_FIGHTER_WIDTH_0026 2u
#define VF2_FIGHTER_WIDTH_0084 4u
#define VF2_FIGHTER_WIDTH_017C 2u
#define VF2_FIGHTER_WIDTH_018A 2u
#define VF2_FIGHTER_WIDTH_019F 1u
#define VF2_FIGHTER_WIDTH_01A4 4u
#define VF2_FIGHTER_WIDTH_01A8 2u
#define VF2_FIGHTER_WIDTH_01AA 2u
#define VF2_FIGHTER_WIDTH_01B1 1u
#define VF2_FIGHTER_WIDTH_01F4 4u
#define VF2_FIGHTER_WIDTH_01FC 4u
#define VF2_FIGHTER_WIDTH_05B4 2u
#define VF2_FIGHTER_WIDTH_05B8 4u
#define VF2_FIGHTER_WIDTH_05F4 4u
#define VF2_FIGHTER_WIDTH_0614 2u
#define VF2_FIGHTER_WIDTH_06DC 2u
#define VF2_FIGHTER_WIDTH_0821 1u
#define VF2_FIGHTER_WIDTH_0844 4u
#define VF2_FIGHTER_WIDTH_0C50 4u
#define VF2_FIGHTER_WIDTH_1200 1u

/*
 * Provisional in-memory layout. The struct is intentionally padded to
 * exact byte offsets and must not be assumed to be the final game
 * object layout — it is a navigation aid for the next taint/Z3 step:
 *   branch 0x00018698 depends on fighter0 + 0x1a4 bit 6 / fighter0 + 0x5b6
 *   branch 0x0002949c depends on fighter0 + 0x1a4 bit 19
 */
struct vf2_fighter_candidate {
    uint32_t field_0000;                 /* +0x0000  R 4B */
    uint8_t  field_0004;                 /* +0x0004  R 1B coli slot */
    uint8_t  _pad_0005[0x0084 - 0x0005];
    uint32_t field_0084;                 /* +0x0084  R 4B */
    uint8_t  _pad_0088[0x017c - 0x0088];
    uint16_t field_017c;                 /* +0x017c  RW 2B */
    uint8_t  _pad_017e[0x018a - 0x017e];
    uint16_t field_018a;                 /* +0x018a  RW 2B */
    uint8_t  _pad_018c[0x019F - 0x018c];
    uint8_t  field_019f;                 /* +0x019f  R 1B */
    uint8_t  _pad_01a0[0x01a4 - 0x01a0];
    uint32_t field_01a4;                 /* +0x01a4  R 4B state/flags */
    uint16_t field_01a8;                 /* +0x01a8  R 2B */
    uint16_t field_01aa;                 /* +0x01aa  R 2B unsigned window */
    uint8_t  _pad_01ac[0x01b1 - 0x01ac];
    uint8_t  field_01b1;                 /* +0x01b1  R 1B type 0/6/8/10 */
    uint8_t  _pad_01b2[0x01f4 - 0x01b2];
    uint32_t field_01f4;                 /* +0x01f4  R 4B */
    uint8_t  _pad_01f8[0x01fc - 0x01f8];
    uint32_t field_01fc;                 /* +0x01fc  R 4B */
    uint8_t  _pad_0200[0x05b4 - 0x0200];
    uint16_t field_05b4;                 /* +0x05b4  R 2B */
    uint8_t  _pad_05b6[0x05b8 - 0x05b6];
    uint32_t field_05b8;                 /* +0x05b8  RW 4B */
    uint8_t  _pad_05bc[0x05f4 - 0x05bc];
    uint32_t field_05f4;                 /* +0x05f4  RW 4B */
    uint8_t  _pad_05f8[0x0614 - 0x05f8];
    uint16_t field_0614;                 /* +0x0614  R 2B path B mask */
    uint8_t  _pad_0616[0x06dc - 0x0616];
    uint16_t field_06dc;                 /* +0x06dc  W 2B coli contact clear */
    uint8_t  _pad_06de[0x0821 - 0x06de];
    uint8_t  field_0821;                 /* +0x0821  R 1B coli scan */
    uint8_t  _pad_0822[0x0844 - 0x0822];
    uint32_t field_0844;                 /* +0x0844  R 4B */
    uint8_t  _pad_0848[0x0c50 - 0x0848];
    uint32_t field_0c50;                 /* +0x0c50  W 4B float result */
    uint8_t  _pad_0c54[0x1200 - 0x0c54];
    uint8_t  field_1200;                 /* +0x1200  W 1B */
    uint8_t  _pad_1201[VF2_FIGHTER_CANDIDATE_WINDOW - 0x1201];
};

_Static_assert(offsetof(struct vf2_fighter_candidate, field_0000) == 0x0000, "fighter field_0000 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_0004) == 0x0004, "fighter field_0004 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_0084) == 0x0084, "fighter field_0084 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_017c) == 0x017c, "fighter field_017c offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_018a) == 0x018a, "fighter field_018a offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_019f) == 0x019F, "fighter field_019f offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_01a4) == 0x01a4, "fighter field_01a4 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_01a8) == 0x01a8, "fighter field_01a8 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_01aa) == 0x01aa, "fighter field_01aa offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_01b1) == 0x01b1, "fighter field_01b1 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_01f4) == 0x01f4, "fighter field_01f4 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_01fc) == 0x01fc, "fighter field_01fc offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_05b4) == 0x05b4, "fighter field_05b4 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_05b8) == 0x05b8, "fighter field_05b8 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_05f4) == 0x05f4, "fighter field_05f4 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_0614) == 0x0614, "fighter field_0614 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_06dc) == 0x06dc, "fighter field_06dc offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_0821) == 0x0821, "fighter field_0821 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_0844) == 0x0844, "fighter field_0844 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_0c50) == 0x0c50, "fighter field_0c50 offset");
_Static_assert(offsetof(struct vf2_fighter_candidate, field_1200) == 0x1200, "fighter field_1200 offset");
_Static_assert(sizeof(struct vf2_fighter_candidate) == VF2_FIGHTER_CANDIDATE_WINDOW, "fighter window");

/* Helper: byte offset validation for a given fighter base */
static inline int vf2_fighter_candidate_offset_valid(uint32_t offset, uint32_t width)
{
    return offset + width <= VF2_FIGHTER_CANDIDATE_WINDOW;
}

#endif /* VF2_FIGHTER_CANDIDATE_H */
