#ifndef VF2_HYBRID_TEST_ENTRIES_H
#define VF2_HYBRID_TEST_ENTRIES_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

/* Test-only direct child entry for the measured fa_game_info 0x18644
 * corridor. The caller must park the CPU at the requested return address
 * (0x164b0 or 0x164c4) with the original call frame still present. */
vf2_status vf2_hybrid_game_info_child_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t return_address
);

/* v0389: test-only entry to the recovered 0x14288 -> 0x19ef8 corridor
 * unit (setup `call 0x1a1e4`, scratch `call 0x26ef0`, clear/return
 * `call 0x27130`, tail `ret 0x1428c`).  The CPU must be parked at
 * 0x14288 with g0 == 0x505, g7 the player base and a pushed frame;
 * entry +0x1a4 must be 0.  On success ip == 0x1428c after 1622 steps /
 * 4 calls / 4 rets.  Anything else is VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_19ef8_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0690: test-only access to the measured 0x19ef8 large-mask family
 * (vf2_hybrid_player_19ef8_measured_masks in the recovered corridor).
 * Admitted() answers whether a branch-bit-masked state word is one of the
 * individually measured rows; the count/at accessors expose the
 * enumeration itself.  The masks carry no semantics and must stay
 * evidence-backed. */
int vf2_hybrid_player_19ef8_mask_admitted_for_test(
    uint32_t non_branch_state_flags
);
size_t vf2_hybrid_player_19ef8_measured_mask_count_for_test(void);
uint32_t vf2_hybrid_player_19ef8_measured_mask_at_for_test(size_t index);

/* v0673: test-only selector-1 setup boundary from the nonzero fa_rob arm.
 * The CPU must be parked at 0x1a044 with g0 == 1 and g7 the fighter base.
 * On success the measured 0x1a1e4 call returns at 0x1a048 after 167
 * instructions / +1 call / +1 return. */
vf2_status vf2_hybrid_player_selector1_setup_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0674: test-only selector-1 return tail parked at 0x144b8 after the
 * 0x1a048 continuation.  The measured neutral state-exchange arm reaches
 * 0x1463c after 35 instructions with no additional call/return. */
vf2_status vf2_hybrid_player_selector1_return_tail_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0676: test-only selector-1 continuation from 0x1a048 through the
 * measured 0x26ef0/0x27130 calls, stopping at 0x144b8. */
vf2_status vf2_hybrid_player_selector1_continuation_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0390: test-only entry to the measured 0x1428c head
 * (setbit-26 + `call 0x270d4` five-slot wrapper + 0x1429c tail).
 * The CPU must be parked at 0x1428c with g7 the player base,
 * record/scratch live (record 0x0201c2fc, scratch nonzero) and a
 * pushed frame.  On success ip == 0x142c0 after 9247 steps /
 * +6 calls / +6 rets.  Anything else is VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_1428c_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* liftkit-guided 0x27ce0 gate.  The CPU must be parked at 0x1abf4 with
 * g7 pointing at the player record.  The measured equal-selector shape
 * returns at 0x1abf8; other shapes continue to the existing 0x27d00
 * bridge and remain subject to its gates. */
vf2_status vf2_hybrid_player_27ce0_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0707: test-only entry to the 0x27d00 -> 0x28184 chain head.  The CPU
 * must be parked at 0x27d00 with g7 pointing at the player record.
 * Accepted shapes end at 0x28268 (bbc-17-taken, bbc-0-taken, or
 * cmpobe-taken siblings, all bit-20-clear with null table and status
 * bit0 clear); the float fall-through and every other shape is
 * VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_28184_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0392: test-only entry to the measured 0x142c0 geometry-expansion
 * body (command 0x550000 family + bit-21 flag set + 0x4b5d0 table
 * lookup).  The CPU must be parked at 0x142c0 with g7 the player base
 * and g1 in {0,1}; frame phase (byte 0x00530005) must be 0 and game
 * phase (byte 0x0050002b) must not be 6 or 7.  On success ip == 0x14310
 * after 56 steps / +3 calls / +3 rets (g1 == 0), with r15 == game phase
 * and r14 == frame phase.  Anything else is VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_142c0_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0393: test-only entry to the measured fa_rob fighter-exchange body
 * 0x1442c (called at 0x14388 when the instance byte +0x04(g7) == 0).
 * The CPU must be parked at 0x1442c with g7/g8 the two fighter bases,
 * both +0x197 not in {16,24,25,27}, and a pushed frame.  On the accepted
 * live (neutral) path it runs the two 0x14640 no-op helper calls and
 * clears both fighters' +0x198 to 0, ending ip == 0x1463c after 51 steps
 * / +2 calls / +2 rets.  Anything else is VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_1442c_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0393: test-only entry to the measured fa_rob collision/state helper
 * 0x14640 no-op path.  The CPU must be parked at 0x14640 with g7 the
 * fighter base and a pushed frame; +0x198 == 0, +0x654 == 0, +0x197 not
 * 27/28, (g7) bit 4 clear and +0x194 == 0.  On success it rets after
 * 12 steps / +1 return.  Anything else is VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_14640_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0395: test-only entry to the measured fa_rob state-25 arm 0x144b0.
 * The CPU must be parked at 0x144b0 with g7/g8 the fighter bases, r10/r11
 * their originals and a pushed frame; +0x194(g7) == 0, +0x197(g7) == 25,
 * +0x197(g8) not 16/24/25/27 and the cmpobl taken.  On success it lands
 * at 0x1463c after 53 steps / +1 call / +1 return.  Anything else is
 * VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_144b0_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0404/v0415/v0416/v0419/v0420/v0421/v0422/v0423/v0424/v0429: test-only entry to the measured fa_rob state-27/state-16
 * arms at 0x1453c/0x14570.
 * The CPU must be parked at 0x14528 with g7/g8 the fighter bases, r10/r11
 * their originals and a pushed frame. Either r7 == 27 (direct, 52 steps) or
 * r7 != 27 with r8 == 27 (the measured g7/g8 swap, 56 steps) is accepted;
 * the measured state-16 joins are also accepted for (r7,r8) = (16,0),
 * (0,16), or (16,16) with the observed 0x500028 bit-0 gate. +0x194(g7)
 * must index a valid type-5 record chain; the direct (16,0) shape also has a
 * measured bit-0-set first-scaling variant at 55 steps. The remaining short
 * path requires bit 0 of +0x1a4(g8) clear, bit 6 of the 0x50016c+0x3351 byte
 * clear and bit 9 of 0x508000 set. These short/scaling accepted paths have
 * +1 call / +1 return and land at 0x1463c. The direct (16,0) shape also admits the
 * measured +0x3351 bit-6-set / g8 bit-29-clear second gate at 54 steps.
 * It also admits the measured direct (16,0) bit-29-set later scaling arm at
 * 57 steps, which selects the 0x1b982 table. The measured board-bit-9-clear
 * text tail is also admitted for all 35 accepted state/scaling shapes; it adds
 * 74 instructions and one call/return to each corresponding short path.
 * The first-scaling bit-0 arm is also admitted for the measured direct and
 * swapped state-27/state-16 compositions (55/59/58/57/61 steps).
 * The bit-6 second gate with g8 bit 29 clear is also admitted for the
 * measured direct/swapped state-27/state-16 compositions (54/58/57/56/60
 * steps).
 * The bit-29 later scaling arm is also admitted for the measured direct/
 * swapped state-27/state-16 compositions (57/61/60/59/63 steps).
 * The measured mixed first/later scaling compositions are also admitted at
 * 57/60/61/64/60/63/59/62/63/66 steps in the same shape order.
 * Anything else is VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_1453c_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0405/v0425/v0428/v0432: test-only entry to the measured fa_rob state-27 arm 0x14640.
 * The CPU must be parked at 0x14640 with g7 the state-27 fighter base and a
 * pushed frame; +0x198 == 0, +0x197 == 27, +0x194(g7) must index a valid
 * type-15 record chain and bit 20 of 0x500068 must be clear. The original
 * +0x654 == 0 shape lands at 0x146c4 after 41 steps; the measured compare-
 * prefix sibling accepts +0x654 != 0 with unequal signed +0x1aa/+0x62a and
 * lands there after 44 steps. The measured bit-20-set sibling lands there
 * after 45 steps and leaves its measured condition state. All have +1 call /
 * +1 return, with the ret unconsumed. Anything else is VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_14640_state27_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0406/v0426/v0428: test-only entry to the measured fa_rob state-28 arm 0x14640.
 * The CPU must be parked at 0x14640 with g7 the state-28 fighter base and a
 * pushed frame; +0x198 == 0 and +0x197 == 28. The original +0x654 == 0
 * shape lands at 0x146c4 after 13 steps; the measured compare-prefix sibling
 * accepts +0x654 != 0 with unequal signed +0x1aa/+0x62a and lands there after 16
 * steps. Both have +0 call / +0 return, add 3 to +0x1aa and clear +0x194;
 * the ret remains unconsumed. Anything else is VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_14640_state28_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0407: test-only entry to the measured fa_rob bit-4-set neutral arm
 * 0x14640.  The CPU must be parked at 0x14640 with g7 the fighter base and a
 * pushed frame; +0x198 == 0, +0x654 == 0, +0x197 not 27/28/13 and bit 4 of
 * (g7) SET.  On success it lands at 0x146c4 after 12 steps / +0 call / +0
 * return (no walker), having cleared +0x194(g7), leaving the 0x146c4 ret
 * unconsumed.  Anything else is VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_14640_bit4set_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0408: test-only entry to the measured fa_rob escape arm 0x14640.  The CPU
 * must be parked at 0x14640 with g7 the fighter base and a pushed frame;
 * +0x198 != 0.  On success it lands at 0x146e8 after 5 steps / +0 call / +0
 * return (no walker), having stored r3 (= +0x198) to +0x194(g7) and cleared
 * +0x654(g7), leaving the 0x146e8 ret unconsumed.  Anything else is
 * VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_14640_escape_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0409/v0414: test-only entry to measured fa_rob compare-prefix greater
 * tails at 0x14640.  The CPU must be parked at 0x14640 with g7 the fighter
 * base and a pushed frame; +0x198 == 0, +0x654 != 0 and
 * s16(+0x1aa) > s16(+0x62a).  The measured neutral bit-4-set shape lands at
 * 0x146c4 after 15 steps and clears +0x194.  The measured neutral bit-4-clear
 * shapes land at 0x146d8 after 14 steps when +0x194 == 0 (leaving +0x654
 * unchanged), or 16 steps when +0x194 != 0 (clearing +0x654).  The measured
 * state-13 shapes land at 0x146d8 after 16 steps with bit 4 clear or 17 steps
 * with bit 4 set, and clear +0x654.  All returns remain unconsumed.  Anything
 * else is VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_14640_compare_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0410: test-only entry to the measured fa_rob compare-prefix escape arm
 * 0x14640.  The CPU must be parked at 0x14640 with g7 the fighter base and a
 * pushed frame; +0x198 == 0, +0x654 != 0 and s16(+0x1aa) == s16(+0x62a).  On
 * success it lands at 0x146e8 after 10 steps / +0 call / +0 return (no
 * walker), having stored r3 (= the +0x654 value) to +0x194(g7) and cleared
 * +0x654(g7), leaving the 0x146e8 ret unconsumed.  Anything else is
 * VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_14640_compare_escape_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0412/v0413/v0427/v0428: test-only entry to measured fa_rob compare-prefix
 * neutral tails at 0x14640.  The CPU must be parked at 0x14640 with g7 the
 * fighter base and a pushed frame; +0x198 == 0, +0x654 != 0 and
 * unequal s16(+0x1aa)/s16(+0x62a).  The v0412 neutral shape requires +0x197 not
 * 27/28/13, bit 4 clear and +0x194 == 0; it lands at 0x146d8 after 14 steps
 * and leaves +0x654 unchanged. The measured neutral bit-4-clear sibling with
 * nonzero +0x194 also lands there after 16 steps and clears +0x654. The
 * measured neutral bit-4-set sibling returns at 0x146c4 after 15 steps; the
 * v0413 state-13 shape requires bit 4 set
 * and +0x194 != 0; it lands at 0x146d8 after 17 steps and clears +0x654.
 * Both leave the return unconsumed. Anything else is unsupported. */
vf2_status vf2_hybrid_player_14640_compare_less_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* v0411: test-only entry to the measured fa_rob state-13 neutral-tail arm
 * 0x14640.  The CPU must be parked at 0x14640 with g7 the fighter base and a
 * pushed frame; +0x198 == 0, +0x654 == 0, +0x197 == 13 and bit 4 of (g7) SET.
 * On success it lands at 0x146d8 after 14 steps / +0 call / +0 return (no
 * walker), having cleared +0x654(g7) (the +0x146cc cmpobe 0, r14 not taken
 * because +0x197 == 13 forces +0x194 non-zero), leaving the 0x146d8 ret
 * unconsumed.  Anything else is VF2_ERROR_UNSUPPORTED. */
vf2_status vf2_hybrid_player_14640_state13_execute_for_test(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

#endif
