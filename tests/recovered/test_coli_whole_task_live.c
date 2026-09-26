#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/i960/snapshot.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"

static int failures=0;
#define CHECK(e) do{ if(!(e)){fprintf(stderr,"FAILED %s:%d: %s\n",__FILE__,__LINE__,#e); ++failures;} }while(0)

static vf2_status apply_f0(vf2_model2a *m){
    vf2_status s=vf2_model2a_write_u32(m, 0x00510b24, 0x00000100);
    if(s!=VF2_OK) return s;
    uint8_t v=1; s=vf2_model2a_write(m,0x005111a0,&v,1);
    if(s!=VF2_OK) return s;
    return vf2_model2a_write_u32(m, 0x005149cc, 0x0000ffff);
}
static vf2_status apply_f1(vf2_model2a *m){
    vf2_status s=vf2_model2a_write_u32(m, 0x00512b24, 0x00000100);
    if(s!=VF2_OK) return s;
    uint8_t v=1; s=vf2_model2a_write(m,0x005111a0,&v,1);
    if(s!=VF2_OK) return s;
    return vf2_model2a_write_u32(m, 0x005149cc, 0x0000ffff);
}
static vf2_status apply_both(vf2_model2a *m){
    vf2_status s=vf2_model2a_write_u32(m, 0x00510b24, 0x00000100);
    if(s!=VF2_OK) return s;
    s=vf2_model2a_write_u32(m, 0x00512b24, 0x00000100);
    if(s!=VF2_OK) return s;
    uint8_t v=1; s=vf2_model2a_write(m,0x005111a0,&v,1);
    if(s!=VF2_OK) return s;
    return vf2_model2a_write_u32(m, 0x005149cc, 0x0000ffff);
}
static vf2_status apply_both_high(vf2_model2a *m){
    vf2_status s=vf2_model2a_write_u32(m, 0x00510b24, 0x00000100);
    if(s!=VF2_OK) return s;
    s=vf2_model2a_write_u32(m, 0x00512b24, 0x00000100);
    if(s!=VF2_OK) return s;
    uint8_t v=5; s=vf2_model2a_write(m,0x005111a1,&v,1);
    if(s!=VF2_OK) return s;
    uint8_t zero[2]={0,0}; s=vf2_model2a_write(m,0x005111a2,zero,sizeof(zero));
    if(s!=VF2_OK) return s;
    return vf2_model2a_write_u32(m, 0x00510d84, 0);
}

static void test_one(const uint8_t *rom,size_t rs,const uint8_t *data,size_t ds,int f1){
    vf2_model2a ref_m={0}, nat_m={0};
    vf2_i960_cpu ref_cpu={0}, nat_cpu={0};
    vf2_i960_snapshot snap; vf2_i960_snapshot_init(&snap);
    (void)vf2_model2a_initialize(&ref_m);
    (void)vf2_model2a_initialize(&nat_m);
    CHECK(vf2_model2a_attach_main_rom(&ref_m,rom,rs)==VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nat_m,rom,rs)==VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_m,data,ds)==VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nat_m,data,ds)==VF2_OK);
    int ok = (vf2_i960_snapshot_read_file(&snap,"out/coli-parked-221e8.vf2snap")==VF2_OK ||
              vf2_i960_snapshot_read_file(&snap,"D:/ia/vf2-decomp/out/coli-parked-221e8.vf2snap")==VF2_OK);
    CHECK(ok);
    CHECK(vf2_i960_snapshot_restore(&snap,&ref_cpu,&ref_m)==VF2_OK);
    CHECK(vf2_i960_snapshot_restore(&snap,&nat_cpu,&nat_m)==VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&ref_m,rom,rs)==VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nat_m,rom,rs)==VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_m,data,ds)==VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nat_m,data,ds)==VF2_OK);
    if(f1==11) {
        CHECK(apply_both_high(&ref_m)==VF2_OK);
        CHECK(apply_both_high(&nat_m)==VF2_OK);
        CHECK(vf2_model2a_write_u32(&ref_m, 0x00510d84, 0x00008000u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&nat_m, 0x00510d84, 0x00008000u) == VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a2, (const uint8_t *)"\x01\x00", 2u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a2, (const uint8_t *)"\x01\x00", 2u) == VF2_OK);
    }
    else if(f1==10) {
        CHECK(apply_both(&ref_m)==VF2_OK);
        CHECK(apply_both(&nat_m)==VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a0, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a0, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a1, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a1, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a2, (const uint8_t *)"\x01\x00", 2u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a2, (const uint8_t *)"\x01\x00", 2u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&ref_m, 0x00510d84, 0x00008000u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&nat_m, 0x00510d84, 0x00008000u) == VF2_OK);
    }
    else if(f1==9) {
        CHECK(apply_f1(&ref_m)==VF2_OK);
        CHECK(apply_f1(&nat_m)==VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a0, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a0, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a2, (const uint8_t *)"\x01\x00", 2u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a2, (const uint8_t *)"\x01\x00", 2u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&ref_m, 0x00510d84, 0x00008000u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&nat_m, 0x00510d84, 0x00008000u) == VF2_OK);
    }
    else if(f1==8 || f1==7) {
        CHECK((f1==7 ? apply_f0(&ref_m) : apply_f1(&ref_m))==VF2_OK);
        CHECK((f1==7 ? apply_f0(&nat_m) : apply_f1(&nat_m))==VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a0, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a0, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a1, (const uint8_t *)"\x05", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a1, (const uint8_t *)"\x05", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a2, (const uint8_t *)"\x10\x00", 2u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a2, (const uint8_t *)"\x10\x00", 2u) == VF2_OK);
    }
    else if(f1==6) {
        CHECK(apply_both(&ref_m)==VF2_OK);
        CHECK(apply_both(&nat_m)==VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a0, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a0, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a1, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a1, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a2, (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a2, (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&ref_m, 0x00512d84, 0x00008000u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&nat_m, 0x00512d84, 0x00008000u) == VF2_OK);
    }
    else if(f1==5) {
        CHECK(apply_both(&ref_m)==VF2_OK);
        CHECK(apply_both(&nat_m)==VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a0, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a0, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a1, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a1, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a2, (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a2, (const uint8_t *)"\x00\x00", 2u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&ref_m, 0x00510d84, 0x00008000u) == VF2_OK);
        CHECK(vf2_model2a_write_u32(&nat_m, 0x00510d84, 0x00008000u) == VF2_OK);
    }
    else if(f1==4) {
        CHECK(apply_f0(&ref_m)==VF2_OK);
        CHECK(apply_f0(&nat_m)==VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a0, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a0, (const uint8_t *)"\x00", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&ref_m, 0x005111a1, (const uint8_t *)"\x05", 1u) == VF2_OK);
        CHECK(vf2_model2a_write(&nat_m, 0x005111a1, (const uint8_t *)"\x05", 1u) == VF2_OK);
    }
    else if(f1==3) {CHECK(apply_both_high(&ref_m)==VF2_OK); CHECK(apply_both_high(&nat_m)==VF2_OK);}
    else if(f1==2) {CHECK(apply_both(&ref_m)==VF2_OK); CHECK(apply_both(&nat_m)==VF2_OK);}
    else if(f1) {CHECK(apply_f1(&ref_m)==VF2_OK); CHECK(apply_f1(&nat_m)==VF2_OK);}
    else {CHECK(apply_f0(&ref_m)==VF2_OK); CHECK(apply_f0(&nat_m)==VF2_OK);}
    // reference whole-task stepping
    size_t steps=0;
    while(ref_cpu.ip != 0x00010dcc && steps<10000){
        CHECK(vf2_i960_step(&ref_cpu,&ref_m,NULL)==VF2_OK);
        steps++;
    }
    CHECK(ref_cpu.ip==0x00010dcc);
    uint64_t ref_ins = ref_cpu.executed_instructions - snap.cpu.executed_instructions;
    uint64_t ref_calls = ref_cpu.procedure_calls - snap.cpu.procedure_calls;
    uint64_t ref_rets = ref_cpu.procedure_returns - snap.cpu.procedure_returns;
    // nat via hybrid_first_dispatch (uses hybrid_execute_coli_body)
    uint32_t registry = nat_cpu.registers[29];
    // ensure ip is coli entry
    CHECK(nat_cpu.ip==0x000221e8);
    vf2_hybrid_task_report rep; memset(&rep,0,sizeof(rep));
    vf2_status ns = vf2_hybrid_first_dispatch_task_execute(&nat_m,&nat_cpu,registry,&rep);
    CHECK(ns==VF2_OK);
    CHECK(nat_cpu.ip==0x00010dcc);
    uint64_t nat_ins = nat_cpu.executed_instructions - snap.cpu.executed_instructions;
    uint64_t nat_calls = nat_cpu.procedure_calls - snap.cpu.procedure_calls;
    uint64_t nat_rets = nat_cpu.procedure_returns - snap.cpu.procedure_returns;
    const char *mode_str = f1==11 ? "both-high-bit15-8221" : f1==10 ? "both-bit15-8221" : f1==9 ? "f1-bit15-8221" : f1==8 ? "f1-scan5-82216" : f1==7 ? "f0-scan5-82216" : f1==6 ? "both-bit15-f1" : f1==5 ? "both-bit15" : f1==4 ? "f0-scan5" : f1==3 ? "both-high" : f1==2 ? "both" : f1 ? "f1" : "f0";
    printf("mode %s: ref %llu/%llu/%llu nat %llu/%llu/%llu\n", mode_str,
        (unsigned long long)ref_ins,(unsigned long long)ref_calls,(unsigned long long)ref_rets,
        (unsigned long long)nat_ins,(unsigned long long)nat_calls,(unsigned long long)nat_rets);
    if(f1==11 || f1==10 || f1==9) {
        CHECK(ref_ins==(f1==11 ? 9526 : f1==10 ? 9520 : 9385)); CHECK(ref_calls==(f1==11 || f1==10 ? 18 : 17)); CHECK(ref_rets==(f1==11 || f1==10 ? 19 : 18));
    } else if(f1==8 || f1==7) {
        CHECK(ref_ins==(f1==7 ? 9391 : 9385)); CHECK(ref_calls==17); CHECK(ref_rets==18);
    } else if(f1==6) {
        CHECK(ref_ins==9520); CHECK(ref_calls==18); CHECK(ref_rets==19);
    } else if(f1==5) {
        CHECK(ref_ins==9520); CHECK(ref_calls==18); CHECK(ref_rets==19);
    } else if(f1==4) {
        CHECK(ref_ins==9391); CHECK(ref_calls==17); CHECK(ref_rets==18);
    } else if(f1==0){
        CHECK(ref_ins==9393); CHECK(ref_calls==17); CHECK(ref_rets==18);
    } else if(f1==1) {
        CHECK(ref_ins==9385); CHECK(ref_calls==17); CHECK(ref_rets==18);
    } else if(f1==2) {
        CHECK(ref_ins==9528); CHECK(ref_calls==18); CHECK(ref_rets==19);
    } else {
        CHECK(ref_ins==9526); CHECK(ref_calls==18); CHECK(ref_rets==19);
    }
    CHECK(nat_ins==ref_ins);
    CHECK(nat_calls==ref_calls);
    CHECK(nat_rets==ref_rets);
    vf2_i960_snapshot_diff d; memset(&d,0,sizeof(d));
    CHECK(vf2_i960_compare_live_state(&ref_cpu,&ref_m,&nat_cpu,&nat_m,&d)==VF2_OK);
    if(!d.equal){
        fprintf(stderr,"live state mismatch %s: %s off 0x%zx exp 0x%08x act 0x%08x bytes %zu\n", mode_str, d.component, d.first_offset, d.expected_value, d.actual_value, d.differing_bytes);
        ++failures;
    }
    vf2_i960_snapshot_destroy(&snap);
    vf2_model2a_shutdown(&ref_m);
    vf2_model2a_shutdown(&nat_m);
}

static vf2_status apply_matrix_case_for_fighters(
    vf2_model2a *m,
    uint32_t f0_flag,
    uint32_t f1_flag,
    uint32_t f0_804,
    uint8_t f0_821,
    uint16_t f0_822,
    uint32_t f1_804,
    uint8_t f1_821,
    uint16_t f1_822
)
{
    vf2_status status = vf2_model2a_write_u32(m, 0x00510b24, f0_flag);
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(m, 0x00512b24, f1_flag);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(m, 0x00510d84, f0_804);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(m, 0x005111a1, &f0_821, 1u);
    }
    if (status == VF2_OK) {
        const uint8_t bytes[2] = {
            (uint8_t)f0_822,
            (uint8_t)(f0_822 >> 8u)
        };
        status = vf2_model2a_write(m, 0x005111a2, bytes, sizeof(bytes));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(m, 0x00512d84, f1_804);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write(m, 0x005131a1, &f1_821, 1u);
    }
    if (status == VF2_OK) {
        const uint8_t bytes[2] = {
            (uint8_t)f1_822,
            (uint8_t)(f1_822 >> 8u)
        };
        status = vf2_model2a_write(m, 0x005131a2, bytes, sizeof(bytes));
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(m, 0x005149cc, 0x0000ffffu);
    }
    return status;
}

static void test_matrix_case(
    const uint8_t *rom,
    size_t rs,
    const uint8_t *data,
    size_t ds,
    const vf2_i960_snapshot *source_snapshot,
    uint32_t f0_flag,
    uint32_t f1_flag,
    uint32_t f0_804,
    uint8_t f0_821,
    uint16_t f0_822,
    uint32_t f1_804,
    uint8_t f1_821,
    uint16_t f1_822
)
{
    vf2_model2a ref_m = {0};
    vf2_model2a nat_m = {0};
    vf2_i960_cpu ref_cpu = {0};
    vf2_i960_cpu nat_cpu = {0};
    vf2_i960_snapshot_diff diff = {0};
    vf2_hybrid_task_report report = {0};
    uint64_t ref_ins = 0u;
    uint64_t ref_calls = 0u;
    uint64_t ref_rets = 0u;
    uint64_t nat_ins = 0u;
    uint64_t nat_calls = 0u;
    uint64_t nat_rets = 0u;
    size_t steps = 0u;
    const uint32_t active_flag = f0_flag != 0u ? f0_flag : f1_flag;
    const uint64_t expected_ins =
        f0_flag == 0u && f1_flag == 0u ? UINT64_C(9214) :
        f0_flag != 0u && f1_flag != 0u ?
            (f0_821 == 5u && f1_821 == 5u ? UINT64_C(9532) :
             f0_821 == 5u || f1_821 == 5u ? UINT64_C(9526) : UINT64_C(9520)) :
        (active_flag != 0u &&
         (f0_flag != 0u ? f0_821 : f1_821) == 5u
            ? UINT64_C(9391) : UINT64_C(9385));
    const uint64_t expected_calls =
        f0_flag != f1_flag ? UINT64_C(17) : UINT64_C(18);
    const uint64_t expected_rets = expected_calls + UINT64_C(1);

    CHECK(vf2_model2a_initialize(&ref_m));
    CHECK(vf2_model2a_initialize(&nat_m));
    CHECK(vf2_model2a_attach_main_rom(&ref_m, rom, rs) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nat_m, rom, rs) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_m, data, ds) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nat_m, data, ds) == VF2_OK);
    CHECK(vf2_i960_snapshot_restore(source_snapshot, &ref_cpu, &ref_m) == VF2_OK);
    CHECK(vf2_i960_snapshot_restore(source_snapshot, &nat_cpu, &nat_m) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&ref_m, rom, rs) == VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nat_m, rom, rs) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&ref_m, data, ds) == VF2_OK);
    CHECK(vf2_model2a_attach_main_data(&nat_m, data, ds) == VF2_OK);
    CHECK(apply_matrix_case_for_fighters(
        &ref_m, f0_flag, f1_flag, f0_804, f0_821, f0_822,
        f1_804, f1_821, f1_822
    ) == VF2_OK);
    CHECK(apply_matrix_case_for_fighters(
        &nat_m, f0_flag, f1_flag, f0_804, f0_821, f0_822,
        f1_804, f1_821, f1_822
    ) == VF2_OK);

    while (ref_cpu.ip != 0x00010dcc && steps < 10000u) {
        CHECK(vf2_i960_step(&ref_cpu, &ref_m, NULL) == VF2_OK);
        ++steps;
    }
    CHECK(ref_cpu.ip == 0x00010dcc);
    ref_ins = ref_cpu.executed_instructions - source_snapshot->cpu.executed_instructions;
    ref_calls = ref_cpu.procedure_calls - source_snapshot->cpu.procedure_calls;
    ref_rets = ref_cpu.procedure_returns - source_snapshot->cpu.procedure_returns;

    CHECK(nat_cpu.ip == 0x000221e8);
    vf2_status native_status = vf2_hybrid_first_dispatch_task_execute(
        &nat_m, &nat_cpu, nat_cpu.registers[29], &report
    );
    CHECK(native_status == VF2_OK);
    CHECK(nat_cpu.ip == 0x00010dcc);
    nat_ins = nat_cpu.executed_instructions - source_snapshot->cpu.executed_instructions;
    nat_calls = nat_cpu.procedure_calls - source_snapshot->cpu.procedure_calls;
    nat_rets = nat_cpu.procedure_returns - source_snapshot->cpu.procedure_returns;

    CHECK(ref_ins == expected_ins);
    CHECK(ref_calls == expected_calls);
    CHECK(ref_rets == expected_rets);
    CHECK(nat_ins == ref_ins);
    CHECK(nat_calls == ref_calls);
    CHECK(nat_rets == ref_rets);
    CHECK(vf2_i960_compare_live_state(
        &ref_cpu, &ref_m, &nat_cpu, &nat_m, &diff
    ) == VF2_OK);
    CHECK(diff.equal);

    vf2_model2a_shutdown(&ref_m);
    vf2_model2a_shutdown(&nat_m);
}

static void run_rom(const char *dir){
    uint8_t *rom=NULL,*data=NULL; size_t rs=0,ds=0;
    vf2_i960_snapshot matrix_snapshot;
    CHECK(vf2_romset_build_region(dir,VF2_REGION_MAINCPU,&rom,&rs)==VF2_OK);
    CHECK(vf2_romset_build_region(dir,VF2_REGION_MAIN_DATA,&data,&ds)==VF2_OK);
    test_one(rom,rs,data,ds,0);
    test_one(rom,rs,data,ds,1);
    test_one(rom,rs,data,ds,2);
    test_one(rom,rs,data,ds,3);
    test_one(rom,rs,data,ds,4);
    test_one(rom,rs,data,ds,5);
    test_one(rom,rs,data,ds,6);
    test_one(rom,rs,data,ds,7);
    test_one(rom,rs,data,ds,8);
    test_one(rom,rs,data,ds,9);
    test_one(rom,rs,data,ds,10);
    test_one(rom,rs,data,ds,11);
    vf2_i960_snapshot_init(&matrix_snapshot);
    CHECK(
        vf2_i960_snapshot_read_file(
            &matrix_snapshot, "out/coli-parked-221e8.vf2snap"
        ) == VF2_OK ||
        vf2_i960_snapshot_read_file(
            &matrix_snapshot, "D:/ia/vf2-decomp/out/coli-parked-221e8.vf2snap"
        ) == VF2_OK
    );
    for (uint32_t f0_flag = 0u; f0_flag <= 0x100u; f0_flag += 0x100u) {
        for (uint32_t f1_flag = 0u; f1_flag <= 0x100u; f1_flag += 0x100u) {
            for (uint32_t f0_804 = 0u; f0_804 <= 0x8000u; f0_804 += 0x8000u) {
                const uint8_t scans[] = {0u, 1u, 4u, 5u};
                const uint16_t fields[] = {0u, 1u, 16u, 256u};
                for (size_t scan = 0u; scan < sizeof(scans); ++scan) {
                    for (size_t field = 0u; field < sizeof(fields) / sizeof(fields[0]); ++field) {
                        test_matrix_case(
                            rom, rs, data, ds, &matrix_snapshot,
                            f0_flag, f1_flag, f0_804,
                            scans[scan], fields[field],
                            0u, 0u, 0u
                        );
                    }
                }
            }
        }
    }
    {
        const uint8_t scans[] = {0u, 1u, 4u, 5u};
        for (size_t f0_scan = 0u; f0_scan < sizeof(scans); ++f0_scan) {
            for (size_t f1_scan = 0u; f1_scan < sizeof(scans); ++f1_scan) {
                test_matrix_case(
                    rom, rs, data, ds, &matrix_snapshot,
                    0x100u, 0x100u, 0u, scans[f0_scan], 0u,
                    0u, scans[f1_scan], 0u
                );
            }
        }
    }
    for (uint32_t f0_flag = 0u; f0_flag <= 0x100u; f0_flag += 0x100u) {
        for (uint32_t f1_flag = 0u; f1_flag <= 0x100u; f1_flag += 0x100u) {
            for (uint32_t f1_804 = 0u; f1_804 <= 0x8000u; f1_804 += 0x8000u) {
                const uint8_t scans[] = {0u, 1u, 4u, 5u};
                const uint16_t fields[] = {0u, 1u, 16u, 256u};
                for (size_t scan = 0u; scan < sizeof(scans); ++scan) {
                    for (size_t field = 0u; field < sizeof(fields) / sizeof(fields[0]); ++field) {
                        test_matrix_case(
                            rom, rs, data, ds, &matrix_snapshot,
                            f0_flag, f1_flag, 0u, 0u, 0u,
                            f1_804, scans[scan], fields[field]
                        );
                    }
                }
            }
        }
    }
    vf2_i960_snapshot_destroy(&matrix_snapshot);
    free(rom); free(data);
}
int main(int argc,char **argv){
    if(argc!=2){ puts("coli-whole-task-live ROM-independent passed"); return failures?1:0; }
    run_rom(argv[1]);
    if(failures){ fprintf(stderr,"%d coli-whole-task-live failed\n",failures); return 1; }
    puts("coli-whole-task-live differential tests passed");
    return 0;
}
