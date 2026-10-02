#ifndef VF2_HYBRID_COLI_H
#define VF2_HYBRID_COLI_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

/* Recover the measured fa_coli bit-mask helper at 0x22298.
 * The CPU must already be inside the callee (IP == entry, frame pushed).
 * Warm (bit 8 clear): 7 insns, store 0 into g7+0x6dc. Sibling (bits 8
 * and 1 set): 8 insns, same store. Other siblings fail closed. */
vf2_status vf2_hybrid_coli_bitmask_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured fa_coli contact query at 0x22404.
 * The CPU must already be inside the callee (IP == entry, frame pushed).
 * Warm (bit 8 clear): 14 insns, snapshot store, clear pending bit,
 * g0 = 0. Sibling (bit 8 set, equal snapshots, pending clear, helper
 * r3 = 0, empty scan mask): 30 insns, store 0 into g8+0x6d4, g0 = 0.
 * Other siblings fail closed. */
vf2_status vf2_hybrid_coli_contact_query_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured warm-path fa_coli g3-scan helper at 0x238a4.
 * The CPU must already be inside the callee (IP == entry, frame pushed).
 * Bit 8 of g7+0x1a4 clear sets g3 = 0; siblings fail closed. */
vf2_status vf2_hybrid_coli_238a4_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured 0x23238 helper (v0310/v0313): g0 != 0x2ce
 * early-out (body 2); g0 == 0x2ce compares g8+0x1f8 as float. */
vf2_status vf2_hybrid_coli_23238_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured bit-26 compact path of fa_coli helper 0x230d4
 * (v0311) and the measured bit-26-clear long path (v0314). */
vf2_status vf2_hybrid_coli_230d4_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured fa_coli digit-parse helper at 0x502a4
 * (v0344-B). The CPU must already be inside the caller (frame depth
 * >= 1, balx performs no push) with entry registers staged. Exit
 * registers and the computed bx target are written back; ip is set
 * to the bx target with no frame pop. */
vf2_status vf2_hybrid_coli_502a4_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured byte-expand leaf at 0x7fc0 (v0345-A). Real
 * call/ret callee: the CPU must already be inside the caller frame
 * (depth >= 1); completion pops the frame to the return address. */
vf2_status vf2_hybrid_coli_7fc0_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured fa_coli resolver at 0x225cc (v0304 compact
 * bit-3 sibling; v0314 long body). */
vf2_status vf2_hybrid_coli_225cc_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured 0x1ab34 table-walk helper (v0312). */
vf2_status vf2_hybrid_coli_1ab34_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured warm-path fa_coli bit-remap helper at 0x23878.
 * The CPU must already be inside the callee (IP == entry, frame pushed).
 * Bits 0..29 of g3 are remapped through the ROM table at 0x02007b76. */
vf2_status vf2_hybrid_coli_23878_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured warm-path fa_coli nested bit-scan at 0x238f8.
 * The CPU must already be inside the callee (IP == entry, frame pushed).
 * Warm PUNCH source at 0x91f880 is all-zero (no stores). */
vf2_status vf2_hybrid_coli_238f8_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured warm-path fa_coli flag builder at 0x233d0.
 * The CPU must already be inside the callee (IP == entry, frame pushed).
 * Warm PUNCH copies the ROM row at 0x232c4 into g13+0xb4.. and leaves
 * g6 = 0; unmeasured siblings fail closed. */
vf2_status vf2_hybrid_coli_233d0_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured warm-path fa_coli FIFO delta push at 0x2364c.
 * The CPU must already be inside the callee (IP == entry, frame pushed).
 * Warm PUNCH has both fighters' +0x1f4 triples zero; the leaf writes
 * command 0x18003030 plus three zero deltas to FIFO 0x884000 and stores
 * the three FIFO replies at g13+0xc8. Non-zero positions fail closed. */
vf2_status vf2_hybrid_coli_2364c_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured warm-path fa_coli poly cluster builder at 0x2396c.
 * The CPU must already be inside the callee (IP == entry, frame pushed).
 * Two PUNCH-driven invocations; inlines three 0x23878 bit-remaps.
 * Unmeasured threshold/min/max siblings fail closed. */
vf2_status vf2_hybrid_coli_2396c_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured warm-path fa_coli poly shell at 0x23524.
 * The CPU must already be inside the callee (IP == entry, frame pushed).
 * Covers the 179-insn shell plus bal 0x23694, inlining every recovered
 * child for accounting. Unmeasured siblings fail closed. */
vf2_status vf2_hybrid_coli_23524_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

/* Recover the measured warm-path fa_coli mid-body/tail at 0x22210.
 * Covers two 0x22298 bitmask calls, two 0x22404 contact-query calls
 * and the both-zero exit through ret to 0x10dcc. The 0x225cc resolver
 * is not reached on the warm path. Unmeasured siblings fail closed. */
vf2_status vf2_hybrid_coli_midbody_tail_execute(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu
);

#endif
