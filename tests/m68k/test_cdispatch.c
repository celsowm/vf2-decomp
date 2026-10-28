#include "vf2/m68k_step.h"
#include "vf2/game.h"
#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"
#include "vf2/sound_board.h"
#include "vf2/status.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define EXPECT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures; \
        } \
    } while (0)

/* S7a: drain class-switch routing + e0-head path (see
 * audio_switch_audit_v0819.md). The ROM switch reads the filed
 * $150e byte and jumps: $90->$858, $80->$10d0, $e0->$12b4,
 * $b0->$13ca, $c0->$1e5a, $a0->$1e74, anything else->rts at $856
 * (byte-verified 4e 75). S2b retired the C's default->90 mapping:
 * unlisted classes ignore (handler 0x856) with drain consume.
 */

static void make_bus(
    vf2_m68k_bus *bus,
    vf2_scsp *scsp,
    uint8_t *ram,
    const uint8_t *rom,
    size_t rom_size
)
{
    bus->ram = ram;
    bus->ram_size = VF2_SOUND_RAM_BYTES;
    bus->rom = rom;
    bus->rom_size = rom_size;
    bus->sample_rom = NULL;
    bus->sample_rom_size = 0u;
    bus->scsp = scsp;
    bus->event_count = 0u;
}

static void reset_regs(vf2_m68k_regs *regs)
{
    vf2_m68k_regs_reset(regs);
    regs->a[5] = UINT32_C(0x100000);
    regs->a[6] = UINT32_C(0x1000);
    regs->a[7] = UINT32_C(0x7f00);
    regs->pc = UINT32_C(0x81a);
}

static int scsp_equal(const vf2_scsp *a, const vf2_scsp *b)
{
    size_t i = 0u;
    if (memcmp(a->registers, b->registers, sizeof(a->registers)) != 0) {
        return 0;
    }
    for (i = 0u; i < VF2_SCSP_SLOT_COUNT; ++i) {
        if (a->slot_address[i] != b->slot_address[i] ||
            a->slot_step[i] != b->slot_step[i] ||
            a->slot_envelope[i] != b->slot_envelope[i] ||
            a->slot_active[i] != b->slot_active[i] ||
            a->slot_envelope_state[i] != b->slot_envelope_state[i] ||
            a->slot_backward[i] != b->slot_backward[i]) {
            return 0;
        }
    }
    return (memcmp(a->midi_input, b->midi_input, sizeof(a->midi_input)) ==
            0 &&
            a->midi_read_index == b->midi_read_index &&
            a->midi_write_index == b->midi_write_index);
}

/* Owner-ROM differential for B0 selector 0x12 (0x18A8).
 * Compare the 68000 program's register-independent destination
 * stores against bounded native C dispatch. This is opt-in only
 * and does not run in public CI without the original sound ROM.
 * No ROM bytes or derived proprietary artifacts are committed. */
static void test_b0_prescaler_setup_original(
    const uint8_t *audio_rom, size_t audio_rom_size
)
{
    static const uint8_t arguments[] = {0u, 8u, UINT8_C(0xff)};
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report report;
    size_t i;
    uint8_t actual_setup_selector = UINT8_C(0xff);
    /* Independently assert the selector -> handler mapping in
     * the private original ROM (JMP d16,PC from 0x13DE table).
     * Do not only compare against a directly injected handler PC:
     * that would not prove whether a BE 12 or BE 1A selects it. */
    {
        const size_t a1a = UINT32_C(0x13de) + UINT32_C(0x1a) * 4u;
        const size_t a12 = UINT32_C(0x13de) + UINT32_C(0x12) * 4u;
        if (audio_rom_size <= a1a + 3u) {
            EXPECT_TRUE(0 && "original ROM lacks B0 jump table");
            return;
        }
        {
            uint16_t op1a = (uint16_t)((uint16_t)audio_rom[a1a] << 8u |
                                        audio_rom[a1a + 1u]);
            uint16_t op12 = (uint16_t)((uint16_t)audio_rom[a12] << 8u |
                                        audio_rom[a12 + 1u]);
            uint32_t disp1a = (uint32_t)audio_rom[a1a + 2u] << 8u |
                              audio_rom[a1a + 3u];
            uint32_t disp12 = (uint32_t)audio_rom[a12 + 2u] << 8u |
                              audio_rom[a12 + 3u];
            int32_t target1a = (int32_t)(a1a + 2u) +
                (disp1a >= 0x8000u ? (int32_t)disp1a - 0x10000 :
                                     (int32_t)disp1a);
            int32_t target12 = (int32_t)(a12 + 2u) +
                (disp12 >= 0x8000u ? (int32_t)disp12 - 0x10000 :
                                     (int32_t)disp12);
            fprintf(stderr, "B0_ROM_TABLE selector_1a=%05x "
                            "selector_12=%05x\n",
                    (unsigned)target1a, (unsigned)target12);
            EXPECT_TRUE(op1a == UINT16_C(0x4efa) &&
                        op12 == UINT16_C(0x4efa));
            /* v0788 and v0799 disagreed about this index.
             * Both layouts are explicitly representable, but
             * exactly one must select the measured 0x18A8 body
             * and the other must be the default RTS. */
            EXPECT_TRUE(
                (target1a == INT32_C(0x18a8) &&
                 target12 == INT32_C(0x13dc)) ||
                (target12 == INT32_C(0x18a8) &&
                 target1a == INT32_C(0x13dc)));
            if (target1a == INT32_C(0x18a8)) {
                actual_setup_selector = UINT8_C(0x1a);
            } else if (target12 == INT32_C(0x18a8)) {
                actual_setup_selector = UINT8_C(0x12);
            }
        }
    }
    if (actual_setup_selector == UINT8_C(0xff)) {
        return;
    }
    for (i = 0u; i < sizeof(arguments); ++i) {
        const uint8_t value = arguments[i];
        const uint8_t packet[4] = {
            UINT8_C(0xbe), actual_setup_selector, value, UINT8_C(0xa5)
        };
        uint32_t steps = 0u;
        vf2_status status;
        memset(&board, 0, sizeof(board));
        board.audio_rom = audio_rom;
        board.audio_rom_size = audio_rom_size;
        vf2_scsp_initialize(&board.scsp);
        board.sound_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE] =
            UINT8_C(0x80);
        board.sound_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 1u] =
            UINT8_C(0x0e);
        board.sound_ram[VF2_SOUND_DESCRIPTOR_BASE + 0x0cu] =
            UINT8_C(0x55);
        board.sound_ram[VF2_SOUND_DESCRIPTOR_BASE + 0x0du] =
            UINT8_C(0x33);
        EXPECT_TRUE(vf2_sound_board_emit_command(
            &board, packet) == VF2_OK);
        memcpy(step_ram, board.sound_ram, sizeof(step_ram));
        memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
        make_bus(&bus, &step_scsp, step_ram,
                 audio_rom, audio_rom_size);
        reset_regs(&regs);
        regs.pc = UINT32_C(0x18a8);
        regs.d[0] = value;
        status = vf2_m68k_run(&bus, &regs, 0u, 1, 32u, &steps);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(steps == 4u);
        EXPECT_TRUE(bus.event_count == 0u);
        EXPECT_TRUE(vf2_sound_board_dispatch_next(
            &board, &report) == VF2_OK);
        EXPECT_TRUE(report.handler_address == UINT32_C(0x18a8) &&
                    report.selected_value == actual_setup_selector &&
                    report.descriptors_matched == 1u &&
                    report.handled == 1);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_DESCRIPTOR_BASE + 0x0cu] ==
                    value &&
                    board.sound_ram[VF2_SOUND_DESCRIPTOR_BASE + 0x0du] ==
                    value);
        EXPECT_TRUE(memcmp(step_ram + VF2_SOUND_DESCRIPTOR_BASE,
                           board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE,
                           VF2_SOUND_STREAM_DESCRIPTOR_BYTES) == 0);
        EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
        fprintf(stderr, "B0_PRESCALER_ORIGINAL_DIFF selector=%02x argument=%02x "
                        "stepper_steps=%u setup_parity=%u\n",
                (unsigned)actual_setup_selector,
                (unsigned)value, (unsigned)steps,
                (unsigned)(memcmp(step_ram + VF2_SOUND_DESCRIPTOR_BASE,
                     board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE,
                     VF2_SOUND_STREAM_DESCRIPTOR_BYTES) == 0));
    }
}


/* Opt-in owner-ROM examination of the as-yet-untranslated
 * volume/duration updater at PC 0x181C. Deliberately does NOT
 * compute or accept an unmeasured C substitute. Isolated
 * synthetic record seeds are replayed through actual original
 * 68000 opcodes; report only derived delta counts, not ROM bytes
 * or private music data. The test probes branch structure, it
 * is not a natural Akira scheduler/SCSP audio experiment. */

/* Full original 68000 vs native C differential of the real B0/10
 * volume handler, using the privately supplied original ROM and
 * synthetic descriptor/voice records. This gates the exact
 * 0x181C arithmetic, bit6 voice admission, SCSP slot+0D writes,
 * ten outer iterations, 32 inner iterations, zero/nonzero
 * product arms and unchanged RAM. It is NOT public ROM-free CI. */

/* Owner-ROM differential for the B0/50 byte2=7E shutdown.
 * Compare every RAM byte and the complete modeled SCSP state
 * after actual original opcodes at PC 0x1C0A vs independent
 * bounded native C. This test is compiled publicly but only
 * runs when the owner supplies the original sound ROM.
 * Never embed private ROM/music bytes in this source. */

/* Opt-in differential: original 68000 vs native C 7D / 7F
 * on private owner ROM. Full sound RAM and SCSP model parity
 * (no ROM or original program bytes in the repository). */
/* Private-ROM differential for the original low-header A0
 * music entry. Start at PC 0x1E98 with the row-entry pointer
 * already resolved by the A0 max gate and stop before 1ED2 RTS.
 * Compare the full 32-byte native descriptor with original
 * opcode execution across the first, Akira and last song rows.
 * The 68000's JSR return-stack scratch is intentionally
 * excluded; no proprietary bytes are committed. */
/* Original 68000 stop-PC differential for the FIRST Akira
 * B-class music packet. Stop at 0x212C BEFORE the worker
 * loops to parse another command. This does not claim the
 * entire tick has finished, nor that music now plays in C.
 * Owner-only, requires the exact original sound ROM. */
/* Opt-in user-ROM differential: execute ACTUAL 68000 opcodes
 * across the first three Akira B-class music commands.
 * 0x212C is visited after every packet, so single-step
 * until count=3 at that exact instruction boundary.
 * Compare full RAM and SCSP; ephemeral A3 must be +10. */
/* Original ROM 68000: replay from PC 0x1F7C to the fourth
 * command emission at 0x212C. Each visit to 0x212C is a
 * real instruction boundary, so run one step at a time and
 * require pending-count 4 before stopping. The owner-ROM
 * test is opt-in, not run by public CI. */
/* Owner ROM differential through sixth Akira music emission.
 * Stop at ORIGINAL instruction PC 0x212C only when FIFO count
 * reaches six. A simple run(stop_pc) exits after packet #1!
 * Uses real ROM instructions and compares entire RAM/SCSP to
 * four separately bounded native prefix helpers.
 * This is an opt-in private asset test, not public ROM-free CI. */
/* Owner-only genuine 68000 microstep differential to the
 * NINTH 0x212C emission, not the first 0x212C. Compares the
 * entire RAM/SCSP image with the five native micro-probes.
 * ROM/sample bytes never leave the local owner's asset dir. */
/* Original owner-ROM tenth Akira packet: stop at the TENTH
 * visit to 0x212C, compare full 512KiB RAM and modeled SCSP
 * with six independent C micro-steps. Does not claim a full
 * L2 tick or a rendered song. */
/* Private-ROM verification of the eleventh/twelfth Akira L2
 * packets. The original 68000 must reach its TWELFTH visit to
 * shared emitter PC 0x212C. This is a packet prefix only. */
/* Private-ROM 68000 differential at original 14th shared-emitter
 * boundary. Class 8 -> 0x2052, class 0 -> 0x2080.
 * Descriptor status 0x88 after class 0 is critical:
 * the next original control path is not another 0x1F96 retry. */
/* First original Akira music descriptor's **actual worker
 * return prelude**, immediately BEFORE the 0x2158 RTS. This
 * checks the packed countdown and committed A3 pointer against
 * genuine 68000 instructions, not a guessed scheduler outcome. */
static void test_akira_first_worker_return_original(
    const uint8_t *rom,size_t rom_size
)
{
    static vf2_sound_board board;
    static vf2_sound_board atomic_board;
    static uint8_t actual_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp actual_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_music_worker_report worker = {0};
    vf2_sound_music_prefix_report p0;
    vf2_sound_music_followup_report p1;
    vf2_sound_music_9f_report p2;
    vf2_sound_music_9f_pair_report p3;
    vf2_sound_music_mixed_report p4;
    vf2_sound_music_c_followup_report p5;
    vf2_sound_music_b0_07_report p6;
    vf2_sound_music_80_07_report p7;
    vf2_sound_music_first_return_report p8;
    uint8_t *d;
    vf2_status status=VF2_OK;
    uint32_t instructions=0u;
    int reached=0;
    size_t i;
    memset(&board,0,sizeof(board));
    board.audio_rom=rom;
    board.audio_rom_size=rom_size;
    vf2_scsp_initialize(&board.scsp);
    d=board.sound_ram+VF2_SOUND_DESCRIPTOR_BASE;
    d[0]=0x88u;d[3]=1u;
    d[4]=0u;d[5]=0x61u;d[6]=0x57u;d[7]=0x73u;
    d[8]=0u;d[9]=0x60u;d[10]=0x9cu;d[11]=0x3eu;
    for(i=0u;i<56u;i+=4u) {
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE+i+3u]=
            (uint8_t)(0x80u+i);
    }
    memcpy(&atomic_board,&board,sizeof(board));
    memcpy(actual_ram,board.sound_ram,sizeof(actual_ram));
    memcpy(&actual_scsp,&board.scsp,sizeof(actual_scsp));
    make_bus(&bus,&actual_scsp,actual_ram,rom,rom_size);
    reset_regs(&regs);
    regs.pc=UINT32_C(0x1f7c);
    regs.a[2]=VF2_SOUND_DESCRIPTOR_BASE;
    for(i=0u;i<11000u;++i) {
        uint32_t done=0u;
        int suspended=0;
        if(regs.pc==UINT32_C(0x2158) &&
           actual_ram[VF2_SOUND_COMMAND_COUNT]==14u) {
            reached=1;
            break;
        }
        status=vf2_m68k_run_slice(
            &bus,&regs,UINT32_MAX,0,1u,&done,&suspended);
        if(status!=VF2_OK || done!=1u || !suspended) break;
        ++instructions;
    }
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_b(
        &board,&p0)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_b(
        &board,&p1)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_9f(
        &board,&p2)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_9f(
        &board,&p3)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_mixed_c(
        &board,&p4)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_tenth_c(
        &board,&p5)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_b0_07(
        &board,&p6)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_80_07(
        &board,&p7)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_return(
        &board,&p8)==VF2_OK);
    fprintf(stderr,
        "AKIRA_FIRST_WORKER_RETURN reached=%d cpu=%d steps=%u "
        "pc=%05x a3=%06x pointer=%06x duration=%04x "
        "ram=%u scsp=%u\n",
        reached,(int)status,(unsigned)instructions,
        (unsigned)regs.pc,(unsigned)regs.a[3],
        (unsigned)p8.stream_after,(unsigned)p8.countdown_after,
        (unsigned)(memcmp(actual_ram,board.sound_ram,
                          sizeof(actual_ram))==0),
        (unsigned)scsp_equal(&actual_scsp,&board.scsp));
    EXPECT_TRUE(reached && status==VF2_OK);
    EXPECT_TRUE(regs.pc==UINT32_C(0x2158) &&
                regs.a[3]==UINT32_C(0x61579c));
    EXPECT_TRUE(p8.stream_after==UINT32_C(0x61579c) &&
                p8.countdown_after==UINT16_C(0x16c7));
    EXPECT_TRUE(bus.event_count==0u);
    EXPECT_TRUE(actual_ram[VF2_SOUND_COMMAND_COUNT]==14u);
    EXPECT_TRUE(memcmp(actual_ram,board.sound_ram,
                       sizeof(actual_ram))==0);
    EXPECT_TRUE(scsp_equal(&actual_scsp,&board.scsp));
    /* Original 68000 independent of both native paths. */
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_worker(
        &atomic_board,&worker)==VF2_OK);
    EXPECT_TRUE(worker.stream_after==UINT32_C(0x61579c) &&
                worker.countdown_after==UINT16_C(0x16c7) &&
                worker.packets_emitted==14u);
    EXPECT_TRUE(memcmp(actual_ram,atomic_board.sound_ram,
                       sizeof(actual_ram))==0);
    EXPECT_TRUE(scsp_equal(&actual_scsp,&atomic_board.scsp));
}

static void test_akira_fourteen_initial_packets_original(
    const uint8_t *rom,size_t rom_size
)
{
    static vf2_sound_board board;
    static uint8_t actual_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp actual_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_music_prefix_report p0;
    vf2_sound_music_followup_report p1;
    vf2_sound_music_9f_report p2;
    vf2_sound_music_9f_pair_report p3;
    vf2_sound_music_mixed_report p4;
    vf2_sound_music_c_followup_report p5;
    vf2_sound_music_b0_07_report p6;
    vf2_sound_music_80_07_report p7;
    vf2_status status=VF2_OK;
    uint8_t *d;
    uint32_t instructions=0u;
    int reached=0;
    size_t i;
    if(rom_size<UINT32_C(0x1fe8)) {
        EXPECT_TRUE(0 && "missing original class table");
        return;
    }
    {
        static const uint8_t classes[2]={8u,0u};
        static const uint32_t targets[2]={
            UINT32_C(0x2052),UINT32_C(0x2080)
        };
        for(i=0u;i<2u;++i) {
            const size_t at=UINT32_C(0x1fbc)+4u*classes[i];
            const uint32_t op=((uint32_t)rom[at]<<8u)|rom[at+1u];
            const int16_t disp=(int16_t)(
                ((uint16_t)rom[at+2u]<<8u)|rom[at+3u]);
            const int32_t target=(int32_t)(at+2u)+(int32_t)disp;
            EXPECT_TRUE(op==UINT32_C(0x4efa) &&
                        target==(int32_t)targets[i]);
            if(op!=UINT32_C(0x4efa) ||
               target!=(int32_t)targets[i]) return;
        }
    }
    memset(&board,0,sizeof(board));
    board.audio_rom=rom;
    board.audio_rom_size=rom_size;
    vf2_scsp_initialize(&board.scsp);
    d=board.sound_ram+VF2_SOUND_DESCRIPTOR_BASE;
    d[0]=0x88u;d[3]=1u;
    d[4]=0u;d[5]=0x61u;d[6]=0x57u;d[7]=0x73u;
    d[8]=0u;d[9]=0x60u;d[10]=0x9cu;d[11]=0x3eu;
    for(i=0u;i<56u;i+=4u) {
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE+i+3u]=
            (uint8_t)(0x80u+i);
    }
    memcpy(actual_ram,board.sound_ram,sizeof(actual_ram));
    memcpy(&actual_scsp,&board.scsp,sizeof(actual_scsp));
    make_bus(&bus,&actual_scsp,actual_ram,rom,rom_size);
    reset_regs(&regs);
    regs.pc=UINT32_C(0x1f7c);
    regs.a[2]=VF2_SOUND_DESCRIPTOR_BASE;
    for(i=0u;i<9800u;++i) {
        uint32_t done=0u;
        int suspended=0;
        if(regs.pc==UINT32_C(0x212c) &&
           actual_ram[VF2_SOUND_COMMAND_COUNT]==14u) {
            reached=1;
            break;
        }
        status=vf2_m68k_run_slice(
            &bus,&regs,UINT32_MAX,0,1u,&done,&suspended);
        if(status!=VF2_OK || done!=1u || !suspended) break;
        ++instructions;
    }
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_b(
        &board,&p0)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_b(
        &board,&p1)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_9f(
        &board,&p2)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_9f(
        &board,&p3)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_mixed_c(
        &board,&p4)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_tenth_c(
        &board,&p5)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_b0_07(
        &board,&p6)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_80_07(
        &board,&p7)==VF2_OK);
    fprintf(stderr,
        "AKIRA_14_PACKET_DIFF reached=%d cpu=%d steps=%u "
        "pc=%05x a3=%06x ram=%u scsp=%u count=%u status=%02x\n",
        reached,(int)status,(unsigned)instructions,
        (unsigned)regs.pc,(unsigned)regs.a[3],
        (unsigned)(memcmp(actual_ram,board.sound_ram,
                          sizeof(actual_ram))==0),
        (unsigned)scsp_equal(&actual_scsp,&board.scsp),
        (unsigned)actual_ram[VF2_SOUND_COMMAND_COUNT],
        (unsigned)actual_ram[VF2_SOUND_DESCRIPTOR_BASE]);
    EXPECT_TRUE(reached && status==VF2_OK);
    EXPECT_TRUE(regs.pc==UINT32_C(0x212c) &&
                regs.a[3]==p7.a3_after &&
                p7.a3_after==UINT32_C(0x61579a));
    EXPECT_TRUE(actual_ram[VF2_SOUND_DESCRIPTOR_BASE]==0x88u);
    EXPECT_TRUE(bus.event_count==0u);
    EXPECT_TRUE(actual_ram[VF2_SOUND_COMMAND_COUNT]==14u &&
                p7.cursor_after==56u);
    EXPECT_TRUE(memcmp(actual_ram,board.sound_ram,
                       sizeof(actual_ram))==0);
    EXPECT_TRUE(scsp_equal(&actual_scsp,&board.scsp));
}

static void test_akira_twelve_initial_packets_original(
    const uint8_t *rom,size_t rom_size
)
{
    static vf2_sound_board board;
    static uint8_t actual_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp actual_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_music_prefix_report p0;
    vf2_sound_music_followup_report p1;
    vf2_sound_music_9f_report p2;
    vf2_sound_music_9f_pair_report p3;
    vf2_sound_music_mixed_report p4;
    vf2_sound_music_c_followup_report p5;
    vf2_sound_music_b0_07_report p6;
    uint8_t *d;
    vf2_status status=VF2_OK;
    uint32_t instructions=0u;
    int reached=0;
    size_t i;

    if(rom_size<UINT32_C(0x1fe8)) {
        EXPECT_TRUE(0 && "missing original L2 class table");
        return;
    }
    /* Original 16-way jump table at 0x1FBC: class 0 -> 2080
     * and B -> 1FFC, both JMP (d16,PC). No assumed handler. */
    {
        static const uint8_t classes[2]={0u,11u};
        static const uint32_t targets[2]={
            UINT32_C(0x2080),UINT32_C(0x1ffc)
        };
        for(i=0u;i<2u;++i) {
            const size_t at=UINT32_C(0x1fbc)+4u*classes[i];
            const uint32_t op=((uint32_t)rom[at]<<8u)|rom[at+1u];
            const int16_t disp=(int16_t)(
                ((uint16_t)rom[at+2u]<<8u)|rom[at+3u]);
            const int32_t target=(int32_t)(at+2u)+(int32_t)disp;
            EXPECT_TRUE(op==UINT32_C(0x4efa) &&
                        target==(int32_t)targets[i]);
            if(op!=UINT32_C(0x4efa) ||
               target!=(int32_t)targets[i]) return;
        }
    }

    memset(&board,0,sizeof(board));
    board.audio_rom=rom;
    board.audio_rom_size=rom_size;
    vf2_scsp_initialize(&board.scsp);
    d=board.sound_ram+VF2_SOUND_DESCRIPTOR_BASE;
    d[0]=0x88u;d[3]=1u;
    d[4]=0u;d[5]=0x61u;d[6]=0x57u;d[7]=0x73u;
    d[8]=0u;d[9]=0x60u;d[10]=0x9cu;d[11]=0x3eu;
    for(i=0u;i<48u;i+=4u) {
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE+i+3u]=
            (uint8_t)(0x80u+i);
    }
    memcpy(actual_ram,board.sound_ram,sizeof(actual_ram));
    memcpy(&actual_scsp,&board.scsp,sizeof(actual_scsp));
    make_bus(&bus,&actual_scsp,actual_ram,rom,rom_size);
    reset_regs(&regs);
    regs.pc=UINT32_C(0x1f7c);
    regs.a[2]=VF2_SOUND_DESCRIPTOR_BASE;
    for(i=0u;i<8500u;++i) {
        uint32_t done=0u;
        int suspended=0;
        if(regs.pc==UINT32_C(0x212c) &&
           actual_ram[VF2_SOUND_COMMAND_COUNT]==12u) {
            reached=1;
            break;
        }
        status=vf2_m68k_run_slice(
            &bus,&regs,UINT32_MAX,0,1u,&done,&suspended);
        if(status!=VF2_OK || done!=1u || !suspended) break;
        ++instructions;
    }
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_b(
        &board,&p0)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_b(
        &board,&p1)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_9f(
        &board,&p2)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_9f(
        &board,&p3)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_mixed_c(
        &board,&p4)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_tenth_c(
        &board,&p5)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_b0_07(
        &board,&p6)==VF2_OK);
    fprintf(stderr,
        "AKIRA_12_PACKET_DIFF reached=%d cpu=%d steps=%u "
        "pc=%05x a3=%06x ram=%u scsp=%u count=%u\n",
        reached,(int)status,(unsigned)instructions,
        (unsigned)regs.pc,(unsigned)regs.a[3],
        (unsigned)(memcmp(actual_ram,board.sound_ram,
                          sizeof(actual_ram))==0),
        (unsigned)scsp_equal(&actual_scsp,&board.scsp),
        (unsigned)actual_ram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(reached && status==VF2_OK);
    EXPECT_TRUE(regs.pc==UINT32_C(0x212c) &&
                regs.a[3]==p6.a3_after &&
                p6.a3_after==UINT32_C(0x615795));
    EXPECT_TRUE(bus.event_count==0u);
    EXPECT_TRUE(actual_ram[VF2_SOUND_COMMAND_COUNT]==12u &&
                p6.cursor_after==48u);
    EXPECT_TRUE(memcmp(actual_ram,board.sound_ram,
                       sizeof(actual_ram))==0);
    EXPECT_TRUE(scsp_equal(&actual_scsp,&board.scsp));
}

static void test_akira_ten_initial_packets_original(
    const uint8_t *rom,size_t rom_size
)
{
    static vf2_sound_board board;
    static uint8_t actual_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp actual_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_music_prefix_report p0;
    vf2_sound_music_followup_report p1;
    vf2_sound_music_9f_report p2;
    vf2_sound_music_9f_pair_report p3;
    vf2_sound_music_mixed_report p4;
    vf2_sound_music_c_followup_report p5;
    uint8_t *d;
    uint32_t instructions=0u;
    vf2_status status=VF2_OK;
    int reached=0;
    size_t i;

    memset(&board,0,sizeof(board));
    board.audio_rom=rom;
    board.audio_rom_size=rom_size;
    vf2_scsp_initialize(&board.scsp);
    d=board.sound_ram+VF2_SOUND_DESCRIPTOR_BASE;
    d[0]=0x88u;d[3]=1u;
    d[4]=0u;d[5]=0x61u;d[6]=0x57u;d[7]=0x73u;
    d[8]=0u;d[9]=0x60u;d[10]=0x9cu;d[11]=0x3eu;
    for(i=0u;i<40u;i+=4u) {
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE+i+3u]=
            (uint8_t)(0x80u+i);
    }
    memcpy(actual_ram,board.sound_ram,sizeof(actual_ram));
    memcpy(&actual_scsp,&board.scsp,sizeof(actual_scsp));
    make_bus(&bus,&actual_scsp,actual_ram,rom,rom_size);
    reset_regs(&regs);
    regs.pc=UINT32_C(0x1f7c);
    regs.a[2]=VF2_SOUND_DESCRIPTOR_BASE;
    for(i=0u;i<7000u;++i) {
        uint32_t done=0u;
        int suspended=0;
        if(regs.pc==UINT32_C(0x212c) &&
           actual_ram[VF2_SOUND_COMMAND_COUNT]==10u) {
            reached=1;
            break;
        }
        status=vf2_m68k_run_slice(
            &bus,&regs,UINT32_MAX,0,1u,&done,&suspended);
        if(status!=VF2_OK || done!=1u || !suspended) break;
        ++instructions;
    }
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_b(
        &board,&p0)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_b(
        &board,&p1)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_9f(
        &board,&p2)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_9f(
        &board,&p3)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_mixed_c(
        &board,&p4)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_tenth_c(
        &board,&p5)==VF2_OK);
    fprintf(stderr,
        "AKIRA_10_PACKET_DIFF reached=%d cpu=%d steps=%u "
        "pc=%05x a3=%06x ram=%u scsp=%u count=%u\n",
        reached,(int)status,(unsigned)instructions,
        (unsigned)regs.pc,(unsigned)regs.a[3],
        (unsigned)(memcmp(actual_ram,board.sound_ram,
                          sizeof(actual_ram))==0),
        (unsigned)scsp_equal(&actual_scsp,&board.scsp),
        (unsigned)actual_ram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(reached && status==VF2_OK);
    EXPECT_TRUE(regs.pc==UINT32_C(0x212c) &&
                regs.a[3]==p5.a3_after &&
                p5.a3_after==UINT32_C(0x61578f));
    EXPECT_TRUE(bus.event_count==0u);
    EXPECT_TRUE(actual_ram[VF2_SOUND_COMMAND_COUNT]==10u &&
                p5.cursor_after==40u);
    EXPECT_TRUE(memcmp(actual_ram,board.sound_ram,
                       sizeof(actual_ram))==0);
    EXPECT_TRUE(scsp_equal(&actual_scsp,&board.scsp));
}

static void test_akira_nine_initial_packets_original(
    const uint8_t *rom,size_t rom_size
)
{
    static vf2_sound_board board;
    static uint8_t actual_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp actual_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_music_prefix_report p0;
    vf2_sound_music_followup_report p1;
    vf2_sound_music_9f_report p2;
    vf2_sound_music_9f_pair_report p3;
    vf2_sound_music_mixed_report p4;
    uint8_t *d;
    uint32_t instructions=0u;
    vf2_status status=VF2_OK;
    int reached=0;
    size_t i;
    memset(&board,0,sizeof(board));
    board.audio_rom=rom;
    board.audio_rom_size=rom_size;
    vf2_scsp_initialize(&board.scsp);
    d=board.sound_ram+VF2_SOUND_DESCRIPTOR_BASE;
    d[0]=0x88u;d[3]=1u;
    d[4]=0u;d[5]=0x61u;d[6]=0x57u;d[7]=0x73u;
    d[8]=0u;d[9]=0x60u;d[10]=0x9cu;d[11]=0x3eu;
    for(i=0u;i<36u;i+=4u) {
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE+i+3u]=
            (uint8_t)(0xa0u+i);
    }
    memcpy(actual_ram,board.sound_ram,sizeof(actual_ram));
    memcpy(&actual_scsp,&board.scsp,sizeof(actual_scsp));
    make_bus(&bus,&actual_scsp,actual_ram,rom,rom_size);
    reset_regs(&regs);
    regs.pc=UINT32_C(0x1f7c);
    regs.a[2]=VF2_SOUND_DESCRIPTOR_BASE;
    for(i=0u;i<6000u;++i) {
        uint32_t done=0u;
        int suspended=0;
        if(regs.pc==UINT32_C(0x212c) &&
           actual_ram[VF2_SOUND_COMMAND_COUNT]==9u) {
            reached=1;
            break;
        }
        status=vf2_m68k_run_slice(
            &bus,&regs,UINT32_MAX,0,1u,&done,&suspended);
        if(status!=VF2_OK || done!=1u || !suspended) break;
        ++instructions;
    }
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_b(
        &board,&p0)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_b(
        &board,&p1)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_9f(
        &board,&p2)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_9f(
        &board,&p3)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_mixed_c(
        &board,&p4)==VF2_OK);
    fprintf(stderr,
        "AKIRA_9_PACKET_DIFF reached=%d cpu=%d steps=%u "
        "pc=%05x a3=%06x ram=%u scsp=%u count=%u\n",
        reached,(int)status,(unsigned)instructions,
        (unsigned)regs.pc,(unsigned)regs.a[3],
        (unsigned)(memcmp(actual_ram,board.sound_ram,
                          sizeof(actual_ram))==0),
        (unsigned)scsp_equal(&actual_scsp,&board.scsp),
        (unsigned)actual_ram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(reached && status==VF2_OK);
    EXPECT_TRUE(regs.pc==UINT32_C(0x212c) &&
                regs.a[3]==p4.a3_after &&
                p4.a3_after==UINT32_C(0x61578d));
    EXPECT_TRUE(bus.event_count==0u);
    EXPECT_TRUE(actual_ram[VF2_SOUND_COMMAND_COUNT]==9u &&
                p4.cursor_after==36u);
    EXPECT_TRUE(memcmp(actual_ram,board.sound_ram,
                       sizeof(actual_ram))==0);
    EXPECT_TRUE(scsp_equal(&actual_scsp,&board.scsp));
}

static void test_akira_six_initial_packets_original(
    const uint8_t *rom,size_t rom_size
)
{
    static vf2_sound_board board;
    static uint8_t actual_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp actual_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_music_prefix_report first;
    vf2_sound_music_followup_report follow;
    vf2_sound_music_9f_report fourth;
    vf2_sound_music_9f_pair_report sixth;
    uint8_t *d;
    uint32_t instructions=0u;
    vf2_status status=VF2_OK;
    int reached=0;
    size_t i;
    memset(&board,0,sizeof(board));
    board.audio_rom=rom;
    board.audio_rom_size=rom_size;
    vf2_scsp_initialize(&board.scsp);
    d=board.sound_ram+VF2_SOUND_DESCRIPTOR_BASE;
    d[0]=0x88u;d[3]=1u;
    d[4]=0u;d[5]=0x61u;d[6]=0x57u;d[7]=0x73u;
    d[8]=0u;d[9]=0x60u;d[10]=0x9cu;d[11]=0x3eu;
    for(i=0u;i<24u;i+=4u) {
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE+i+3u]=
            (uint8_t)(0xa1u+i);
    }
    memcpy(actual_ram,board.sound_ram,sizeof(actual_ram));
    memcpy(&actual_scsp,&board.scsp,sizeof(actual_scsp));
    make_bus(&bus,&actual_scsp,actual_ram,rom,rom_size);
    reset_regs(&regs);
    regs.pc=UINT32_C(0x1f7c);
    regs.a[2]=VF2_SOUND_DESCRIPTOR_BASE;
    for(i=0u;i<4096u;++i) {
        uint32_t done=0u;
        int suspended=0;
        if(regs.pc==UINT32_C(0x212c) &&
           actual_ram[VF2_SOUND_COMMAND_COUNT]==6u) {
            reached=1;
            break;
        }
        status=vf2_m68k_run_slice(
            &bus,&regs,UINT32_MAX,0,1u,&done,&suspended);
        if(status!=VF2_OK || done!=1u || !suspended) break;
        ++instructions;
    }
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_b(
        &board,&first)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_b(
        &board,&follow)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_9f(
        &board,&fourth)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_9f(
        &board,&sixth)==VF2_OK);
    fprintf(stderr,
        "AKIRA_6_PACKET_DIFF reached=%d cpu=%d steps=%u "
        "pc=%05x a3=%06x ram=%u scsp=%u count=%u\n",
        reached,(int)status,(unsigned)instructions,
        (unsigned)regs.pc,(unsigned)regs.a[3],
        (unsigned)(memcmp(actual_ram,board.sound_ram,
                          sizeof(actual_ram))==0),
        (unsigned)scsp_equal(&actual_scsp,&board.scsp),
        (unsigned)actual_ram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(reached && status==VF2_OK);
    EXPECT_TRUE(regs.pc==UINT32_C(0x212c) &&
                regs.a[3]==sixth.a3_after &&
                sixth.a3_after==UINT32_C(0x615786));
    EXPECT_TRUE(bus.event_count==0u);
    EXPECT_TRUE(actual_ram[VF2_SOUND_COMMAND_COUNT]==6u &&
                sixth.cursor_after==24u);
    EXPECT_TRUE(memcmp(actual_ram,board.sound_ram,
                       sizeof(actual_ram))==0);
    EXPECT_TRUE(scsp_equal(&actual_scsp,&board.scsp));
}

static void test_akira_first_9f_original(
    const uint8_t *rom, size_t rom_size
)
{
    static vf2_sound_board board;
    static uint8_t cpu_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp cpu_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_music_prefix_report b0;
    vf2_sound_music_followup_report b12;
    vf2_sound_music_9f_report n9;
    uint8_t *d;
    uint32_t executed = 0u;
    vf2_status status = VF2_OK;
    int reached = 0;
    size_t i;
    memset(&board,0,sizeof(board));
    board.audio_rom=rom;
    board.audio_rom_size=rom_size;
    vf2_scsp_initialize(&board.scsp);
    d=board.sound_ram+VF2_SOUND_DESCRIPTOR_BASE;
    d[0]=0x88u;d[3]=1u;
    d[4]=0u;d[5]=0x61u;d[6]=0x57u;d[7]=0x73u;
    d[8]=0u;d[9]=0x60u;d[10]=0x9cu;d[11]=0x3eu;
    for(i=0u;i<16u;i+=4u) {
        board.sound_ram[VF2_SOUND_COMMAND_RING_BASE+i+3u] =
            (uint8_t)(0xa1u+i);
    }
    memcpy(cpu_ram,board.sound_ram,sizeof(cpu_ram));
    memcpy(&cpu_scsp,&board.scsp,sizeof(cpu_scsp));
    make_bus(&bus,&cpu_scsp,cpu_ram,rom,rom_size);
    reset_regs(&regs);
    regs.pc=UINT32_C(0x1f7c);
    regs.a[2]=VF2_SOUND_DESCRIPTOR_BASE;
    for(i=0u;i<3000u;++i) {
        uint32_t done=0u;
        int suspended=0;
        if(regs.pc==UINT32_C(0x212c) &&
           cpu_ram[VF2_SOUND_COMMAND_COUNT]==4u) {
            reached=1;
            break;
        }
        status=vf2_m68k_run_slice(
            &bus,&regs,UINT32_MAX,0,1u,&done,&suspended);
        if(status!=VF2_OK || done!=1u || !suspended) break;
        ++executed;
    }
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_b(
        &board,&b0)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_b(
        &board,&b12)==VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_9f(
        &board,&n9)==VF2_OK);
    fprintf(stderr,
      "AKIRA_FIRST_9F_ORIGINAL reached=%d status=%d steps=%u "
      "pc=%05x a3=%06x ram_equal=%u scsp_equal=%u\n",
      reached,(int)status,(unsigned)executed,(unsigned)regs.pc,
      (unsigned)regs.a[3],
      (unsigned)(memcmp(cpu_ram,board.sound_ram,sizeof(cpu_ram))==0),
      (unsigned)scsp_equal(&cpu_scsp,&board.scsp));
    EXPECT_TRUE(reached && status==VF2_OK);
    EXPECT_TRUE(regs.pc==UINT32_C(0x212c) &&
                regs.a[3]==n9.a3_after &&
                n9.a3_after==UINT32_C(0x615780));
    EXPECT_TRUE(cpu_ram[VF2_SOUND_COMMAND_COUNT]==4u &&
                n9.packet[0]==0x9fu &&
                n9.packet[1]==0x48u &&
                n9.packet[2]==0x77u);
    EXPECT_TRUE(bus.event_count==0u);
    EXPECT_TRUE(memcmp(cpu_ram,board.sound_ram,sizeof(cpu_ram))==0);
    EXPECT_TRUE(scsp_equal(&cpu_scsp,&board.scsp));
}

static void test_akira_l2_three_b_original(
    const uint8_t *rom, size_t rom_size
)
{
    static vf2_sound_board board;
    static uint8_t cpu_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp cpu_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_music_prefix_report first;
    vf2_sound_music_followup_report follow;
    uint8_t *d;
    size_t i;
    uint32_t steps = 0u;
    vf2_status status = VF2_OK;
    int reached = 0;
    memset(&board, 0, sizeof(board));
    board.audio_rom = rom;
    board.audio_rom_size = rom_size;
    vf2_scsp_initialize(&board.scsp);
    d = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
    d[0] = UINT8_C(0x88); d[3] = 1u;
    d[4] = 0u; d[5] = 0x61u;
    d[6] = 0x57u; d[7] = 0x73u;
    d[8] = 0u; d[9] = 0x60u;
    d[10] = 0x9cu; d[11] = 0x3eu;
    board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 3u] = 0xa7u;
    board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 7u] = 0xa8u;
    board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 11u] = 0xa9u;
    memcpy(cpu_ram, board.sound_ram, sizeof(cpu_ram));
    memcpy(&cpu_scsp, &board.scsp, sizeof(cpu_scsp));
    make_bus(&bus, &cpu_scsp, cpu_ram, rom, rom_size);
    reset_regs(&regs);
    regs.pc = UINT32_C(0x1f7c);
    regs.a[2] = VF2_SOUND_DESCRIPTOR_BASE;
    for (i = 0u; i < 2048u; ++i) {
        uint32_t retired = 0u;
        int suspended = 0;
        if (regs.pc == UINT32_C(0x212c) &&
            cpu_ram[VF2_SOUND_COMMAND_COUNT] == 3u) {
            reached = 1;
            break;
        }
        status = vf2_m68k_run_slice(
            &bus, &regs, UINT32_MAX, 0,
            1u, &retired, &suspended);
        if (status != VF2_OK || retired != 1u ||
            suspended == 0) {
            break;
        }
        ++steps;
    }
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_first_b(
        &board, &first) == VF2_OK);
    EXPECT_TRUE(vf2_sound_board_probe_akira_l2_next_two_b(
        &board, &follow) == VF2_OK);
    fprintf(stderr,
        "AKIRA_L2_THREE_B_ORIGINAL reached=%d status=%d "
        "pc=%05x steps=%u a3=%06x full_ram_equal=%u "
        "scsp_equal=%u packets=%u\n",
        reached, (int)status, (unsigned)regs.pc,
        (unsigned)steps, (unsigned)regs.a[3],
        (unsigned)(memcmp(cpu_ram, board.sound_ram,
                           sizeof(cpu_ram)) == 0),
        (unsigned)scsp_equal(&cpu_scsp, &board.scsp),
        (unsigned)cpu_ram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(reached && status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x212c));
    EXPECT_TRUE(regs.a[3] == follow.a3_after &&
                follow.a3_after == UINT32_C(0x61577d));
    EXPECT_TRUE(follow.cursor_after == 12u &&
                follow.duration_byte == 0x72u);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(memcmp(cpu_ram, board.sound_ram,
                       sizeof(cpu_ram)) == 0);
    EXPECT_TRUE(scsp_equal(&cpu_scsp, &board.scsp));
}

static void test_akira_l2_first_b_original(
    const uint8_t *rom, size_t rom_size
)
{
    static vf2_sound_board board;
    static uint8_t cpu_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp cpu_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_music_prefix_report report;
    uint8_t *desc;
    uint32_t steps = 0u;
    vf2_status cpu_status, native_status;
    size_t i;
    memset(&board, 0, sizeof(board));
    board.audio_rom = rom;
    board.audio_rom_size = rom_size;
    vf2_scsp_initialize(&board.scsp);
    desc = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
    desc[0] = UINT8_C(0x88);
    desc[3] = 1u;
    desc[4] = 0u; desc[5] = UINT8_C(0x61);
    desc[6] = UINT8_C(0x57); desc[7] = UINT8_C(0x73);
    desc[8] = 0u; desc[9] = UINT8_C(0x60);
    desc[10] = UINT8_C(0x9c); desc[11] = UINT8_C(0x3e);
    board.sound_ram[VF2_SOUND_COMMAND_RING_BASE + 3u] =
        UINT8_C(0xa7);
    memcpy(cpu_ram, board.sound_ram, sizeof(cpu_ram));
    memcpy(&cpu_scsp, &board.scsp, sizeof(cpu_scsp));
    make_bus(&bus, &cpu_scsp, cpu_ram, rom, rom_size);
    reset_regs(&regs);
    regs.pc = UINT32_C(0x1f7c);
    regs.a[2] = VF2_SOUND_DESCRIPTOR_BASE;
    cpu_status = vf2_m68k_run(
        &bus, &regs, UINT32_C(0x212c), 0, 2048u, &steps);
    native_status = vf2_sound_board_probe_akira_l2_first_b(
        &board, &report);
    fprintf(stderr,
            "AKIRA_L2_FIRST_B_ORIGINAL cpu=%d native=%d steps=%u "
            "stop=%05x a3=%06x ram_parity=%u scsp_parity=%u\n",
            (int)cpu_status, (int)native_status, (unsigned)steps,
            (unsigned)regs.pc, (unsigned)regs.a[3],
            (unsigned)(memcmp(cpu_ram, board.sound_ram,
                              sizeof(cpu_ram)) == 0),
            (unsigned)scsp_equal(&cpu_scsp, &board.scsp));
    EXPECT_TRUE(cpu_status == VF2_OK && native_status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x212c) &&
                steps > 20u && steps < 2048u);
    EXPECT_TRUE(regs.a[3] == report.a3_after &&
                report.a3_after == UINT32_C(0x615776));
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(memcmp(cpu_ram, board.sound_ram,
                       sizeof(cpu_ram)) == 0);
    EXPECT_TRUE(scsp_equal(&cpu_scsp, &board.scsp));
    for (i = 0u; i < VF2_SOUND_STREAM_DESCRIPTOR_BYTES; ++i) {
        EXPECT_TRUE(cpu_ram[VF2_SOUND_DESCRIPTOR_BASE + i] ==
                    board.sound_ram[VF2_SOUND_DESCRIPTOR_BASE + i]);
    }
}

static void test_a0_music_low_header_original(
    const uint8_t *audio_rom, size_t audio_rom_size
)
{
    static const uint8_t songs[] = {
        UINT8_C(0x00), UINT8_C(0x04), UINT8_C(0x1f)
    };
    static vf2_sound_board board;
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    size_t k;
    /* Original sound-ROM AD10 row has 32 selectors, but
     * selectors 0x19..0x1E are all-FF sentinel records.
     * Selector 0x1F aliases the first valid music record.
     * A0 low-body executable C must refuse sentinels.
     * This census requires the PRIVATE original ROM. */
    {
        size_t idx, valid = 0u, sentinels = 0u;
        const uint32_t row = UINT32_C(0x8f24);
        EXPECT_TRUE(audio_rom_size > row + 2u + 32u*4u);
        for (idx = 0u; idx < 32u; ++idx) {
            const size_t at = (size_t)row + 2u + 4u*idx;
            uint32_t record =
                ((uint32_t)audio_rom[at] << 24u) |
                ((uint32_t)audio_rom[at+1u] << 16u) |
                ((uint32_t)audio_rom[at+2u] << 8u) |
                 (uint32_t)audio_rom[at+3u];
            const size_t roff =
                (size_t)(record - VF2_SOUND_AUDIO_ROM_BASE);
            uint32_t root = 0u;
            EXPECT_TRUE(record >= VF2_SOUND_AUDIO_ROM_BASE &&
                        roff + 8u < audio_rom_size);
            if (record < VF2_SOUND_AUDIO_ROM_BASE ||
                roff + 8u >= audio_rom_size) continue;
            root = ((uint32_t)audio_rom[roff+4u] << 24u) |
                   ((uint32_t)audio_rom[roff+5u] << 16u) |
                   ((uint32_t)audio_rom[roff+6u] << 8u) |
                    (uint32_t)audio_rom[roff+7u];
            if (root == UINT32_MAX) {
                vf2_status result;
                size_t j;
                uint8_t *desc;
                memset(&board, 0, sizeof(board));
                board.audio_rom = audio_rom;
                board.audio_rom_size = audio_rom_size;
                desc = board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE;
                memset(desc, UINT8_C(0x73),
                       VF2_SOUND_STREAM_DESCRIPTOR_BYTES);
                result = vf2_sound_board_apply_a0_low_music_record(
                    &board, record);
                EXPECT_TRUE(result == VF2_ERROR_UNSUPPORTED);
                for (j = 0u;
                     j < VF2_SOUND_STREAM_DESCRIPTOR_BYTES; ++j) {
                    EXPECT_TRUE(desc[j] == UINT8_C(0x73));
                }
                ++sentinels;
            } else if (root >= VF2_SOUND_AUDIO_ROM_BASE &&
                       (uint64_t)root <
                       (uint64_t)VF2_SOUND_AUDIO_ROM_BASE +
                       audio_rom_size) {
                ++valid;
            }
        }
        EXPECT_TRUE(valid == 26u && sentinels == 6u);
        fprintf(stderr,
                "A0_MUSIC_ROW_ORIGINAL_CENSUS selectors=32 "
                "valid=%u sentinel=%u\n",
                (unsigned)valid, (unsigned)sentinels);
    }
    for (k = 0u; k < sizeof(songs); ++k) {
        const uint8_t song = songs[k];
        uint32_t record;
        uint32_t steps = 0u;
        vf2_status original;
        vf2_status native;
        size_t i;
        memset(&board, 0, sizeof(board));
        board.audio_rom = audio_rom;
        board.audio_rom_size = audio_rom_size;
        vf2_scsp_initialize(&board.scsp);
        memset(board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE,
               UINT8_C(0xa5), VF2_SOUND_STREAM_DESCRIPTOR_BYTES);
        record = ((uint32_t)audio_rom[0x8f26u + 4u*song] << 24u) |
                 ((uint32_t)audio_rom[0x8f27u + 4u*song] << 16u) |
                 ((uint32_t)audio_rom[0x8f28u + 4u*song] << 8u) |
                  (uint32_t)audio_rom[0x8f29u + 4u*song];
        memcpy(step_ram, board.sound_ram, sizeof(step_ram));
        memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
        make_bus(&bus, &step_scsp, step_ram,
                 audio_rom, audio_rom_size);
        reset_regs(&regs);
        regs.pc = UINT32_C(0x1e98);
        regs.d[0] = song;
        regs.a[4] = UINT32_C(0x608f26);
        original = vf2_m68k_run(
            &bus, &regs, UINT32_C(0x1ed2), 0, 512u, &steps);
        native = vf2_sound_board_apply_a0_low_music_record(
            &board, record);
        fprintf(stderr,
                "A0_MUSIC_LOW_ORIGINAL song=%02x m68k=%d native=%d "
                "steps=%u descriptor_equal=%u\n",
                (unsigned)song, (int)original, (int)native,
                (unsigned)steps,
                (unsigned)(memcmp(
                    step_ram + VF2_SOUND_DESCRIPTOR_BASE,
                    board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE,
                    VF2_SOUND_STREAM_DESCRIPTOR_BYTES) == 0));
        EXPECT_TRUE(original == VF2_OK && native == VF2_OK);
        EXPECT_TRUE(regs.pc == UINT32_C(0x1ed2));
        EXPECT_TRUE(steps >= 60u && steps < 512u);
        EXPECT_TRUE(bus.event_count == 0u);
        EXPECT_TRUE(memcmp(
            step_ram + VF2_SOUND_DESCRIPTOR_BASE,
            board.sound_ram + VF2_SOUND_DESCRIPTOR_BASE,
            VF2_SOUND_STREAM_DESCRIPTOR_BYTES) == 0);
        EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
        /* No music stream may overwrite unrelated RAM,
         * other than the four bytes of emulated JSR stack. */
        for (i = 0u; i < VF2_SOUND_RAM_BYTES; ++i) {
            if ((i >= 0x7efcu && i < 0x7f00u) ||
                (i >= VF2_SOUND_DESCRIPTOR_BASE &&
                 i < VF2_SOUND_DESCRIPTOR_BASE +
                     VF2_SOUND_STREAM_DESCRIPTOR_BYTES)) continue;
            if (step_ram[i] != board.sound_ram[i]) {
                fprintf(stderr,
                        "A0_MUSIC_LOW_UNEXPECTED_RAM addr=%05x\n",
                        (unsigned)i);
                EXPECT_TRUE(0);
                break;
            }
        }
    }
}

static void test_b0_50_7d_7f_original(
    const uint8_t *audio_rom, size_t audio_rom_size
)
{
    static const uint8_t args[2] = {
        UINT8_C(0x7d), UINT8_C(0x7f)
    };
    static vf2_sound_board board;
    static uint8_t ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp chip;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    size_t arg_index;
    for (arg_index = 0u; arg_index < 2u; ++arg_index) {
        size_t scenario;
        const uint8_t argument = args[arg_index];
        for (scenario = 0u; scenario < 4u; ++scenario) {
            vf2_sound_dispatch_report report = {0};
            uint32_t steps = 0u;
            vf2_status a_status, c_status;
            size_t i;
            memset(&board, 0, sizeof(board));
            board.audio_rom = audio_rom;
            board.audio_rom_size = audio_rom_size;
            vf2_scsp_initialize(&board.scsp);
            for (i = 0u; i < VF2_SOUND_VOICE_COUNT; ++i) {
                uint8_t *v = board.sound_ram +
                    VF2_SOUND_VOICE_TABLE_BASE +
                    i * VF2_SOUND_VOICE_BYTES;
                const uint16_t slot = (uint16_t)(i * 0x20u);
                v[0] = (uint8_t)(slot >> 8u);
                v[1] = (uint8_t)slot;
                v[2] = scenario == 0u ? 0u :
                    (i % 4u == 0u ? UINT8_C(0x50) :
                                  UINT8_C(0x40));
                v[4] = (uint8_t)(i % 16u);
                v[9] = i % 7u == 0u ? UINT8_C(0xff) :
                    (uint8_t)((i * 9u + scenario * 5u) % 240u);
                board.sound_ram[UINT32_C(0x2000) + slot] =
                    UINT8_C(0xa3);
                EXPECT_TRUE(vf2_scsp_write_u8(
                    &board.scsp, (uint32_t)slot,
                    UINT8_C(0x20)) == VF2_OK);
                EXPECT_TRUE(vf2_scsp_write_u8(
                    &board.scsp, (uint32_t)slot + 0x0bu,
                    UINT8_C(0xb0)) == VF2_OK);
            }
            for (i = 0u; i < 32u; ++i) {
                board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE+i] =
                    (uint8_t)(0x40u+i);
            }
            for (i = 0u; i < 16u; ++i) {
                board.sound_ram[VF2_SOUND_CHANNEL_CONTROL_BASE+i] =
                    (uint8_t)(0x31u+i);
                board.sound_ram[
                    VF2_SOUND_CHANNEL_CONTROL_ALT_BASE+i] =
                    (uint8_t)(0x62u+i);
            }
            board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E] =
                UINT8_C(0x51);
            board.sound_ram[VF2_SOUND_SHARED_COUNTER_152E] =
                UINT8_C(0x72);
            memcpy(ram, board.sound_ram, sizeof(ram));
            memcpy(&chip, &board.scsp, sizeof(chip));
            make_bus(&bus, &chip, ram, audio_rom, audio_rom_size);
            reset_regs(&regs);
            regs.pc = UINT32_C(0x1c0a);
            regs.d[0] = argument;
            a_status = vf2_m68k_run(
                &bus, &regs, UINT32_MAX, 1, 15000u, &steps);
            c_status = vf2_sound_board_apply_b0_end_7d_7f(
                &board, argument, &report);
            fprintf(stderr,
                "B0_50_ORIGINAL_DIFF arg=%02x scenario=%u "
                "original=%d native=%d steps=%u ram=%u scsp=%u\n",
                (unsigned)argument, (unsigned)scenario,
                (int)a_status, (int)c_status, (unsigned)steps,
                (unsigned)(memcmp(ram, board.sound_ram,
                    sizeof(ram)) == 0),
                (unsigned)scsp_equal(&chip, &board.scsp));
            EXPECT_TRUE(a_status == VF2_OK && c_status == VF2_OK);
            EXPECT_TRUE(steps > 150u && steps < 15000u);
            EXPECT_TRUE(bus.event_count == 0u);
            EXPECT_TRUE(memcmp(ram, board.sound_ram,
                               sizeof(ram)) == 0);
            EXPECT_TRUE(scsp_equal(&chip, &board.scsp));
        }
    }
}

static void test_b0_50_7e_original(
    const uint8_t *audio_rom, size_t audio_rom_size
)
{
    static vf2_sound_board board;
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    size_t scenario;
    for (scenario = 0u; scenario < 3u; ++scenario) {
        vf2_sound_dispatch_report report = {0};
        vf2_status interpreter_status;
        vf2_status native_status;
        size_t i, expected_clears = 0u;
        uint32_t steps = 0u;
        memset(&board, 0, sizeof(board));
        board.audio_rom = audio_rom;
        board.audio_rom_size = audio_rom_size;
        vf2_scsp_initialize(&board.scsp);
        for (i = 0u; i < VF2_SOUND_VOICE_COUNT; ++i) {
            uint8_t *v = board.sound_ram +
                VF2_SOUND_VOICE_TABLE_BASE +
                i * VF2_SOUND_VOICE_BYTES;
            const uint16_t slot = (uint16_t)(i * 0x20u);
            const int armed = scenario != 0u &&
                (scenario == 1u || (i % 3u != 0u));
            const uint8_t ch = (uint8_t)(i % 16u);
            v[0] = (uint8_t)(slot >> 8u);
            v[1] = (uint8_t)slot;
            v[2] = armed ? UINT8_C(0x40) : 0u;
            v[4] = ch;
            v[8] = UINT8_C(0x36);
            board.sound_ram[UINT32_C(0x2000) + slot] =
                UINT8_C(0xa3);
            EXPECT_TRUE(vf2_scsp_write_u8(
                &board.scsp, (uint32_t)slot,
                UINT8_C(0x20)) == VF2_OK);
            EXPECT_TRUE(vf2_scsp_write_u8(
                &board.scsp, (uint32_t)slot + 0x0bu,
                UINT8_C(0xb0)) == VF2_OK);
            if (armed && (ch < 9u || ch == 15u)) {
                ++expected_clears;
            }
        }
        for (i = 0u; i < 20u; ++i) {
            board.sound_ram[VF2_SOUND_CHANNEL_TABLE_BASE + i] =
                UINT8_C(0x12);
        }
        for (i = 0u; i < 16u; ++i) {
            board.sound_ram[VF2_SOUND_CHANNEL_CONTROL_BASE + i] =
                UINT8_C(0x34);
            board.sound_ram[VF2_SOUND_CHANNEL_CONTROL_ALT_BASE + i] =
                UINT8_C(0x56);
        }
        board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E] =
            UINT8_C(0x7e);
        board.sound_ram[VF2_SOUND_SHARED_COUNTER_152E] =
            UINT8_C(0x68);
        memcpy(step_ram, board.sound_ram, sizeof(step_ram));
        memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
        make_bus(&bus, &step_scsp, step_ram,
                 audio_rom, audio_rom_size);
        reset_regs(&regs);
        regs.pc = UINT32_C(0x1c0a);
        regs.d[0] = UINT32_C(0x7e);
        interpreter_status = vf2_m68k_run(
            &bus, &regs, UINT32_MAX, 1, 8000u, &steps);
        native_status = vf2_sound_board_apply_b0_end_7e(
            &board, &report);
        fprintf(stderr,
                "B0_50_7E_DIFF scenario=%u original=%d C=%d "
                "steps=%u ram=%u scsp=%u cleared=%u\n",
                (unsigned)scenario,
                (int)interpreter_status, (int)native_status,
                (unsigned)steps,
                (unsigned)(memcmp(step_ram, board.sound_ram,
                    sizeof(step_ram)) == 0),
                (unsigned)scsp_equal(&step_scsp, &board.scsp),
                (unsigned)report.voices_cleared);
        EXPECT_TRUE(interpreter_status == VF2_OK &&
                    native_status == VF2_OK);
        EXPECT_TRUE(steps > 100u && steps < 8000u);
        EXPECT_TRUE(bus.event_count == 0u);
        EXPECT_TRUE(report.voices_cleared == expected_clears);
        EXPECT_TRUE(memcmp(step_ram, board.sound_ram,
                           sizeof(step_ram)) == 0);
        EXPECT_TRUE(scsp_equal(&step_scsp, &board.scsp));
        EXPECT_TRUE(board.sound_ram[
            VF2_SOUND_SHARED_COUNTER_152E] == 0x68u);
    }
    /* The live dispatcher also recognizes the authenticated
     * original B0/50 jump and retires BE 50 7E. */
    {
        vf2_sound_dispatch_report report = {0};
        const uint8_t packet[4] = {
            UINT8_C(0xbe), UINT8_C(0x50), UINT8_C(0x7e), 0u
        };
        memset(&board, 0, sizeof(board));
        board.audio_rom = audio_rom;
        board.audio_rom_size = audio_rom_size;
        vf2_scsp_initialize(&board.scsp);
        board.sound_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE] =
            UINT8_C(0x80);
        board.sound_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 1u] =
            UINT8_C(0x0e);
        board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E] =
            UINT8_C(0x77);
        EXPECT_TRUE(vf2_sound_board_emit_command(
            &board, packet) == VF2_OK);
        EXPECT_TRUE(vf2_sound_board_dispatch_next(
            &board, &report) == VF2_OK);
        EXPECT_TRUE(report.handler_address == UINT32_C(0x1c0a));
        EXPECT_TRUE(report.handled == 1 &&
                    report.selected_value == UINT8_C(0x50));
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
        EXPECT_TRUE(board.sound_ram[VF2_SOUND_SHARED_COUNTER_151E] == 0u);
    }
}

static void test_b0_181c_native_original(
    const uint8_t *audio_rom, size_t audio_rom_size
)
{
    static const uint8_t args[] = {
        0u, 1u, 4u, 64u, 127u, 128u, 255u
    };
    static vf2_sound_board board;
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    size_t sample;
    for (sample = 0u; sample < sizeof(args); ++sample) {
        vf2_sound_dispatch_report report = {0};
        size_t i;
        uint32_t steps = 0u;
        vf2_status original;
        vf2_status native;
        memset(&board, 0, sizeof(board));
        board.audio_rom = audio_rom;
        board.audio_rom_size = audio_rom_size;
        vf2_scsp_initialize(&board.scsp);
        for (i = 0u; i < VF2_SOUND_COMMAND_DESCRIPTOR_COUNT; ++i) {
            uint8_t *desc = board.sound_ram +
                VF2_SOUND_COMMAND_DESCRIPTOR_BASE +
                i * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES;
            desc[0] = UINT8_C(0x80);
            desc[1u] = (uint8_t)(i % 3u + 1u);
            desc[3u] = (uint8_t)((i * 23u + 19u) & 255u);
            desc[10u] = UINT8_C(0x57);
        }
        for (i = 0u; i < VF2_SOUND_VOICE_COUNT; ++i) {
            uint8_t *voice = board.sound_ram +
                VF2_SOUND_VOICE_TABLE_BASE +
                i * VF2_SOUND_VOICE_BYTES;
            const uint16_t slot = (uint16_t)(i * 0x20u);
            voice[0] = (uint8_t)(slot >> 8u);
            voice[1] = (uint8_t)slot;
            voice[2] = (uint8_t)(i % 4u != 0u ? 0x40u : 0u);
            voice[4] = (uint8_t)(i % 3u + 1u);
            voice[8] = (uint8_t)((i * 11u) & 255u);
        }
        board.sound_ram[VF2_SOUND_SHARED_FACTOR_151F] = 0xf0u;
        memcpy(step_ram, board.sound_ram, sizeof(step_ram));
        memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
        make_bus(&bus, &step_scsp, step_ram,
                 audio_rom, audio_rom_size);
        reset_regs(&regs);
        regs.pc = UINT32_C(0x181c);
        regs.d[0] = args[sample];
        regs.d[2] = UINT32_C(0x10);
        original = vf2_m68k_run(
            &bus, &regs, UINT32_MAX, 1, 8192u, &steps);
        native = vf2_sound_board_apply_b0_volume_181c(
            &board, args[sample], &report);
        fprintf(stderr,
                "B0_181C_NATIVE_PARITY arg=%02x S=%d C=%d "
                "steps=%u RAM=%u SCSP=%u desc=%u voicewrites=%u\n",
                (unsigned)args[sample], (int)original,
                (int)native, (unsigned)steps,
                (unsigned)(memcmp(step_ram, board.sound_ram,
                                   sizeof(step_ram)) == 0),
                (unsigned)scsp_equal(&board.scsp, &step_scsp),
                (unsigned)report.descriptors_updated,
                (unsigned)report.voices_updated);
        EXPECT_TRUE(original == VF2_OK && native == VF2_OK);
        EXPECT_TRUE(steps > 300u && steps < 8192u);
        EXPECT_TRUE(bus.event_count == 0u);
        EXPECT_TRUE(memcmp(step_ram, board.sound_ram,
                           sizeof(step_ram)) == 0);
        EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
        EXPECT_TRUE(report.descriptors_updated == 10u);
    }
}

static void probe_b0_181c_original(
    const uint8_t *audio_rom, size_t audio_rom_size
)
{
    static const uint8_t values[] = {
        0u, UINT8_C(0x04), UINT8_C(0x40), UINT8_C(0x7f)
    };
    static uint8_t ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    size_t variant;
    for (variant = 0u; variant < sizeof(values); ++variant) {
        const uint8_t value = values[variant];
        uint8_t old[VF2_SOUND_COMMAND_DESCRIPTOR_COUNT];
        size_t i;
        unsigned desc10_changes = 0u;
        unsigned desc3_changes = 0u;
        unsigned other_changes = 0u;
        uint32_t steps = 0u;
        vf2_status status;
        memset(ram, 0, sizeof(ram));
        vf2_scsp_initialize(&scsp);
        for (i = 0u; i < VF2_SOUND_COMMAND_DESCRIPTOR_COUNT; ++i) {
            uint8_t *desc = ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE +
                i * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES;
            desc[0] = UINT8_C(0x80);
            desc[1] = (uint8_t)(i & 15u);
            desc[3] = (uint8_t)(0x20u + (uint8_t)i);
            desc[10u] = UINT8_C(0x66);
            old[i] = desc[3];
        }
        ram[VF2_SOUND_SHARED_FACTOR_151F] = UINT8_C(0xff);
        make_bus(&bus, &scsp, ram, audio_rom, audio_rom_size);
        reset_regs(&regs);
        regs.pc = UINT32_C(0x181c);
        regs.d[0] = value;
        regs.d[2] = UINT32_C(0x10);
        status = vf2_m68k_run(&bus, &regs, 0u, 1, 8192u, &steps);
        for (i = 0u; i < VF2_SOUND_COMMAND_DESCRIPTOR_COUNT; ++i) {
            const uint8_t *desc =
                ram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE +
                i * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES;
            if (desc[3] != old[i]) ++desc3_changes;
            if (desc[10u] != UINT8_C(0x66)) ++desc10_changes;
        }
        for (i = 0u; i < 16u; ++i) {
            if (ram[UINT32_C(0x2510) + i] !=
                (i == 15u ? UINT8_C(0xff) : 0u)) {
                ++other_changes;
            }
        }
        printf("B0_181C_ORIGINAL_PROBE argument=%02x status=%d "
               "steps=%u stop_pc=%05x shared_factor=%02x "
               "desc3_changed=%u desc10_changed=%u "
               "shared_bytes_changed=%u scsp_events=%u "
               "native_parity_unproven=1\n",
               (unsigned)value, (int)status, (unsigned)steps,
               (unsigned)regs.pc,
               (unsigned)ram[VF2_SOUND_SHARED_FACTOR_151F],
               desc3_changes, desc10_changes, other_changes,
               (unsigned)bus.event_count);
    }
}

/* Switch routing: filed class -> arm entry. Step counts predicted
   from the linear cmpi/beq chain (2 steps per rejected class). */
static void test_switch_routing(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static const struct {
        uint8_t filed;
        uint32_t target;
        uint32_t steps;
    } legs[] = {
        { 0x90u, 0x858u, 4u },
        { 0x80u, 0x10d0u, 6u },
        { 0xe0u, 0x12b4u, 8u },
        { 0xb0u, 0x13cau, 10u },
        { 0xc0u, 0x1e5au, 12u },
        { 0xa0u, 0x1e74u, 15u },
        { 0x00u, 0x856u, 14u },
    };
    size_t i = 0u;
    for (i = 0u; i < sizeof(legs) / sizeof(legs[0]); ++i) {
        static uint8_t ram[VF2_SOUND_RAM_BYTES];
        static vf2_scsp scsp;
        vf2_m68k_bus bus;
        vf2_m68k_regs regs;
        uint32_t done = 0u;
        vf2_status status = VF2_OK;
        memset(ram, 0, sizeof(ram));
        vf2_scsp_initialize(&scsp);
        make_bus(&bus, &scsp, ram, audio_rom, audio_rom_size);
        reset_regs(&regs);
        ram[0x250eu] = legs[i].filed;
        status = vf2_m68k_run(&bus, &regs, legs[i].target, 0, 20000u,
                              &done);
        fprintf(stderr, "dispatch switch-%02x: status=%d pc=0x%05x steps=%u\n",
                (unsigned)legs[i].filed, (int)status,
                (unsigned)regs.pc, (unsigned)done);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.pc == legs[i].target);
        EXPECT_TRUE(done == legs[i].steps);
    }
}

/* e0 full head, channel 3, voices miss-all: stream d2/d0 prep,
   neg.w (transformed-bit8 clear), $1524 + $4206 stores, 32-voice
   scan with no match, rts at 0x1354. 164 steps. d1 ends 3 (filed
   channel), d2 ends $e9 (neg.b ran). */
static void test_e0_head_nomatch(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;

    memset(ram, 0, sizeof(ram));
    vf2_scsp_initialize(&scsp);
    make_bus(&bus, &scsp, ram, audio_rom, audio_rom_size);
    reset_regs(&regs);
    regs.pc = UINT32_C(0x12b4);
    regs.a[3] = UINT32_C(0x7300);
    regs.a[0] = UINT32_C(0x7400);
    ram[0x7300u] = 0xe3u;
    ram[0x7301u] = 0x05u;
    ram[0x7407u] = 0x00u;
    ram[0x250eu] = 0xe3u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1354), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch e0-nomatch: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x1354));
    EXPECT_TRUE(done == 164u);
    EXPECT_TRUE(regs.d[1] == 3u);
    EXPECT_TRUE(regs.d[2] == UINT32_C(0xe9));
    EXPECT_TRUE(ram[0x2524u] == 0xffu && ram[0x2525u] == 0xf2u);
    EXPECT_TRUE(ram[0x4206u] == 0xffu && ram[0x4207u] == 0xf2u);
    EXPECT_TRUE(bus.event_count == 0u);
}

/* e0 gate, channel >= 9: bcc exits via rts at 0x13c8 (NOT the
   computed jump — the C's "falls into the B0 primer" gloss is
   +4-shaped; the early-return behavior it models still stands). */
static void test_e0_gate_rts(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;

    memset(ram, 0, sizeof(ram));
    vf2_scsp_initialize(&scsp);
    make_bus(&bus, &scsp, ram, audio_rom, audio_rom_size);
    reset_regs(&regs);
    regs.pc = UINT32_C(0x12b4);
    regs.a[3] = UINT32_C(0x7300);
    regs.a[0] = UINT32_C(0x7400);
    ram[0x7300u] = 0xe9u;
    ram[0x7301u] = 0x05u;
    ram[0x7407u] = 0x00u;
    ram[0x250eu] = 0xe9u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x13c8), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch e0-gate: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x13c8));
    EXPECT_TRUE(done == 34u);
}

/* A0 primer, payload above row max: bgt exits via rts at 0x1eac.
   Row 0 max ($608e12[0] -> $6000128f) is $0003 in this ROM
   (measured, not assumed); payload $04 trips it. The C refuses
   the whole A0 path (stale-a2 join); this clean exit arm is a
   future micro-admission candidate (not taken here). */
static void test_a0_overmax(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp scsp;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    uint32_t rowptr = 0u;
    uint32_t rowoff = 0u;
    uint16_t maxv = 0u;
    uint8_t cmd = 0xa0u;
    uint8_t idx = (uint8_t)(cmd & 0x7fu);

    /* The row is indexed by command&$7f (0x1e7c), not row 0. */
    EXPECT_TRUE((uint32_t)0x8e12u + (uint32_t)idx * 4u + 4u <=
                audio_rom_size);
    rowptr = ((uint32_t)audio_rom[0x8e12u + (uint32_t)idx * 4u] << 24u) |
             ((uint32_t)audio_rom[0x8e13u + (uint32_t)idx * 4u] << 16u) |
             ((uint32_t)audio_rom[0x8e14u + (uint32_t)idx * 4u] << 8u) |
             (uint32_t)audio_rom[0x8e15u + (uint32_t)idx * 4u];
    rowoff = rowptr - UINT32_C(0x600000);

    EXPECT_TRUE(rowoff + 2u <= audio_rom_size);
    maxv = (uint16_t)(((uint16_t)audio_rom[rowoff] << 8u) |
                      audio_rom[rowoff + 1u]);
    fprintf(stderr, "dispatch a0-row: idx=0x%02x max=0x%04x\n",
            (unsigned)idx, (unsigned)maxv);
    EXPECT_TRUE(maxv != UINT32_C(0xffff));
    memset(ram, 0, sizeof(ram));
    vf2_scsp_initialize(&scsp);
    make_bus(&bus, &scsp, ram, audio_rom, audio_rom_size);
    reset_regs(&regs);
    regs.pc = UINT32_C(0x1e74);
    regs.a[3] = UINT32_C(0x7300);
    ram[0x7300u] = cmd;
    ram[0x7301u] = (uint8_t)(((uint32_t)maxv + 1u) & 0xffu);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1eac), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch a0-overmax: status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x1eac));
    EXPECT_TRUE(done == 10u);
    EXPECT_TRUE(regs.d[2] == (uint32_t)maxv);
}

/* S7b differential: C dispatch_next vs ROM e0 head+tail.
 *
 * Shared contract (asserted): $1524 word, $4200+2*ch word, voice
 * timer RAM[$201a+d4], SCSP voice word [$10+d4], report channel /
 * voices_matched / handler.
 * Excluded by construction (documented, NOT asserted): ring
 * cursor/count consume (C advances, ROM head doesn't traffic them),
 * $150e current-command write (C writes, ROM head only reads),
 * stepper regs / C scan counters (side-local).
 * Fixture: ring cmd 0xe3 payload 0x05, filed $150e=$e3, descriptor
 * $4000 (ch 3, table 0), voice0 match (slot 0), rest miss-all.
 * Stepper a3 stream mirrors ring[0..1] (controlled input). */
static void test_e0_differential(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *voice0 = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;
    uint16_t c_1524 = 0u;
    uint16_t s_1524 = 0u;
    uint16_t c_chw = 0u;
    uint16_t s_chw = 0u;
    uint16_t c_timer = 0u;
    uint16_t s_timer = 0u;

    /* NOTE (v0822): vf2_sound_board_initialize now attaches on the
       real 0x80000 audio region (T4 proves OK); the old failure was
       the uploader's misread row pointer. Bare-init SCSPs are still
       used here to keep both sides' start states identical without
       depending on boot. */
    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    /* Ring: cursor 0, count 1, cmd e3 payload 05. */
    cram[VF2_SOUND_COMMAND_RING_BASE] = 0xe3u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0x05u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_CURRENT_COMMAND] = 0xe3u;
    /* Descriptor $4000: active, byte-match channel 3 (S2a: the
       drain matches descriptor[1] == command&0xf), table 0. */
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0x03u;
    desc[3] = 0x00u;
    desc[5] = 0x02u;
    desc[7] = 0x00u;
    /* Voice0 $2800: slot 0, active, nibble $30, channel byte 3
       at [4] (ROM cmp.B matches the single byte; voice[5]
       ignored). */
    voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[0] = 0x00u;
    voice0[1] = 0x00u;
    voice0[2] = 0x01u;
    voice0[3] = 0x30u;
    voice0[4] = 0x03u;
    voice0[5] = 0x00u;
    voice0[10] = 0x00u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch e0-diff: c-status=%d ch=%u matched=%u handler=0x%05x\n",
            (int)cstatus, (unsigned)creport.channel,
            (unsigned)creport.voices_matched,
            (unsigned)creport.handler_address);
    EXPECT_TRUE(cstatus == VF2_OK);
    EXPECT_TRUE(creport.channel == 3u);
    EXPECT_TRUE(creport.voices_matched == 1u);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0x12b6));

    make_bus(&bus, &step_scsp, step_ram, audio_rom, audio_rom_size);
    reset_regs(&regs);
    regs.pc = UINT32_C(0x12b4);
    regs.a[3] = UINT32_C(0x7300);
    regs.a[0] = UINT32_C(0x4000);
    /* Primer-correct stream (S2a): the drain leaves a3 at ring+1,
       so the primer reads payload first, then ring byte 2 --
       NOT {command, payload}. */
    step_ram[0x7300u] = 0x05u;
    step_ram[0x7301u] = 0x00u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1354), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch e0-diff: s-status=%d pc=0x%05x steps=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x1354));

    c_1524 = (uint16_t)(((uint16_t)cram[0x2524u] << 8u) | cram[0x2525u]);
    s_1524 = (uint16_t)(((uint16_t)step_ram[0x2524u] << 8u) | step_ram[0x2525u]);
    c_chw = (uint16_t)(((uint16_t)cram[0x4206u] << 8u) | cram[0x4207u]);
    s_chw = (uint16_t)(((uint16_t)step_ram[0x4206u] << 8u) | step_ram[0x4207u]);
    c_timer = (uint16_t)(((uint16_t)cram[0x201au] << 8u) | cram[0x201bu]);
    s_timer = (uint16_t)(((uint16_t)step_ram[0x201au] << 8u) | step_ram[0x201bu]);
    fprintf(stderr, "dispatch e0-diff: 1524 c=%04x s=%04x chw c=%04x s=%04x timer c=%04x s=%04x\n",
            (unsigned)c_1524, (unsigned)s_1524,
            (unsigned)c_chw, (unsigned)s_chw,
            (unsigned)c_timer, (unsigned)s_timer);
    EXPECT_TRUE(c_1524 == s_1524);
    EXPECT_TRUE(c_chw == s_chw);
    EXPECT_TRUE(c_timer == s_timer);
    EXPECT_TRUE(board.scsp.registers[0x10u] == step_scsp.registers[0x10u]);
    EXPECT_TRUE(board.scsp.registers[0x11u] == step_scsp.registers[0x11u]);
}

/* S2a: drain-entry e0 differential -- the byte-match proof.
 *
 * The fixture discriminates byte vs word routing: desc[1] = 0x04
 * (byte-MATCHES channel 4), desc[2] = 0x00 (word be16 = 0x0400
 * MISMATCHES). Command 0xE4. Under the old word-match C the ring
 * is consumed with no handler; under byte-match (silicon: 0x7e6
 * `cmp.b $0001(a0),d0` after the 0x7d2 `andi.b #$f`) both sides
 * run the e0 head+tail through the real drain entry.
 * C: dispatch_next (ring E4/04, count 1, CURRENT pre-zeroed --
 * the drain files it). Stepper: drain entry 0x7a6 (a4 = $1506
 * count, standard a5/a6/a7; a3/a0/d regs set by the drain
 * itself), run to the tail exit 0x1354. Compared: the T1 segment
 * set + SCSP. Drain-only regions stay excluded (ring consume,
 * count/cursor, $150e file, stack window with the switch-jsr and
 * tail-jsr residue); a7 is unbalanced mid-call by construction
 * (drain movem + switch jsr + tail jsr outstanding) -- no assert.
 * Voice0 byte-matches ch 4 (V1 fixture shape). */
static void test_drain_e0_entry(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *voice0 = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0xe4u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 4u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x04u;
    desc[2] = 0x00u;
    desc[3] = 0x00u;
    desc[5] = 0x12u;
    desc[6] = 0x01u;
    desc[7] = 0x00u;
    voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[0] = 0x00u;
    voice0[1] = 0x20u;
    voice0[2] = 0x01u;
    voice0[3] = 0x3au;
    voice0[4] = 0x04u;
    voice0[5] = 0x04u;
    voice0[10] = 0x07u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch drain-e0: c-status=%d ch=%u matched=%u handler=0x%05x\n",
            (int)cstatus, (unsigned)creport.channel,
            (unsigned)creport.voices_matched,
            (unsigned)creport.handler_address);
    EXPECT_TRUE(cstatus == VF2_OK);
    EXPECT_TRUE(creport.channel == 4u);
    EXPECT_TRUE(creport.voices_matched == 1u);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0x12b6));

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1354), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch drain-e0: s-status=%d pc=0x%05x steps=%u events=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x1354));
    EXPECT_TRUE(bus.event_count == 0u);

    {
        static const uint32_t lo[] = { 0u, 0x1020u, 0x2526u, 0x4140u,
                                       0x4240u, 0x7f00u };
        static const uint32_t hi[] = { 0x1000u, 0x2500u, 0x4000u, 0x4200u,
                                       0x7eccu, VF2_SOUND_RAM_BYTES };
        size_t seg = 0u;
        for (seg = 0u; seg < 6u; ++seg) {
            size_t i = 0u;
            for (i = 0u; i < hi[seg] - lo[seg]; ++i) {
                if (board.sound_ram[lo[seg] + i] !=
                    step_ram[lo[seg] + i]) {
                    fprintf(stderr, "dispatch drain-e0: RAM mismatch [%05x,%05x) +0x%x: C=0x%02x S=0x%02x\n",
                            lo[seg], hi[seg], (unsigned)i,
                            (unsigned)board.sound_ram[lo[seg] + i],
                            (unsigned)step_ram[lo[seg] + i]);
                    EXPECT_TRUE(0);
                    break;
                }
            }
        }
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S2b: drain-entry 90-head differential (primer proof).
 *
 * Fixture: ring {0x93, 0x05, byte2}, desc[1] = 3 (byte-match),
 * desc[2] = 0xf5 (HIGH) or 0x03 (LOW), desc[4] = 0x10. Under the
 * old d2=command C, base would be 0xa3 and mode ROM[..+5]; under
 * the primer truth (d2 = payload, second = ring byte 2) base is
 * 0x15 and mode is ROM[0x60c6a6+2] (real ROM bytes -- both sides
 * share audio_rom, so no staging; the assert is C==S equality).
 * C: dispatch_next runs the arm to its door (HIGH: Loop-B
 * early 0xf12; LOW: the 0xa3e walk into the E1m allocator,
 * refuse at 0xfac), packet held. Stepper: drain 0x7a6 to the split target (0x884 HIGH
 * / 0xa3e LOW -- the step delta + target pin the split
 * direction on silicon).
 * * Compared: $150f/$1510 C==S, SCSP equal (head touches none),
 *   zero bus events. $1511 == 0 both sides on the LOW leg only
 *   (S pre-walk stop; C filed desc[6] = 0); the HIGH leg runs
 *   Loop-B (S2d) to the early 0xf12 door, so it compares $1515
 *   C==S instead. HIGH pins handler 0xfac (Loop-B into the
 *   allocator); LOW pins 0xfac (walk into the allocator).
 *   Drain-only state (count subq'd at entry, $150e
 *   file, stack residue) is excluded by construction -- the head
 *   path is 8 linear instructions past the switch, all accounted
 *   (base file, second peek+branch, mode-table file). Door
 *   coverage lives in the drain-90h/loopb legs; this leg keeps
 *   the payload-vs-command discrimination (base 0x15 from payload
 *   5, never 0xa3 from command 0x93). */
static void test_drain_90_head(
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    uint8_t desc2,
    uint32_t stop_pc,
    const char *tag
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 5u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 2u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = desc2;
    desc[3] = 0x00u;
    desc[4] = 0x10u;
    /* Allocator cursor (E1o): both arms flow into it now
     * (HIGH via Loop-B, LOW via the walk); staged pre-snapshot
     * (found voice0 on the zeroed voices). */
    cram[0x251au] = 0x00u;
    cram[0x251au + 1u] = 0x00u;
    cram[0x251au + 2u] = 0x28u;
    cram[0x251au + 3u] = 0x00u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch drain-90-%s: c-status=%d ch=%u handler=0x%05x base=%02x mode=%02x 15=%02x count=%u\n",
            tag, (int)cstatus, (unsigned)creport.channel,
            (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)cram[VF2_SOUND_LOOKUP_MODE],
            (unsigned)cram[0x2515u],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.channel == 3u);
    /* Handler by arm: both reach the E1o allocator now (HIGH
     * via Loop-B's 0x926 early door, LOW via the walk), so both
     * refuse at 0xfac with voice0 allocated (cursor 0x2810). */
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xfac));
    EXPECT_TRUE(cram[0x251au] == 0x00u);
    EXPECT_TRUE(cram[0x251au + 1u] == 0x00u);
    EXPECT_TRUE(cram[0x251au + 2u] == 0x28u);
    EXPECT_TRUE(cram[0x251au + 3u] == 0x10u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] == 0x15u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
    EXPECT_TRUE(creport.cursor_after == creport.cursor_before);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, stop_pc, 0, 20000u, &done);
    fprintf(stderr, "dispatch drain-90-%s: s-status=%d pc=0x%05x steps=%u events=%u base=%02x mode=%02x 15=%02x\n",
            tag, (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)step_ram[VF2_SOUND_LOOKUP_MODE],
            (unsigned)step_ram[0x2515u]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == stop_pc);
    EXPECT_TRUE(done == (desc2 >= 0xf0u ? 72u : 36u));
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(step_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x15u);
    if (desc2 < 0xf0u) {
        EXPECT_TRUE(step_ram[0x2511u] == 0u);
    }
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] ==
                step_ram[VF2_SOUND_LOOKUP_CURRENT]);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_MODE] ==
                step_ram[VF2_SOUND_LOOKUP_MODE]);
    EXPECT_TRUE(cram[0x2511u] == step_ram[0x2511u]);
    if (desc2 >= 0xf0u) {
        EXPECT_TRUE(cram[0x2515u] == step_ram[0x2515u]);
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S2b: 90 zero-second trap. Ring {0x93, 0x05, 0x00}: the head
 * files base 0x15, then 0x866/0x868 beq-takes into the 80 body
 * mid-instruction (0x10e2) -- trap on HW. C refuses with mode
 * untouched, packet held. Stepper stop: 0x10e2. */
static void test_drain_90_trap(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 5u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 0u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0xf5u;
    desc[4] = 0x10u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch drain-90-trap: c-status=%d handler=0x%05x base=%02x mode=%02x count=%u\n",
            (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)cram[VF2_SOUND_LOOKUP_MODE],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0x858));
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] == 0x15u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_MODE] == 0u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x10e2), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch drain-90-trap: s-status=%d pc=0x%05x steps=%u events=%u base=%02x mode=%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)step_ram[VF2_SOUND_LOOKUP_MODE]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x10e2));
    EXPECT_TRUE(done == 31u);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(step_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x15u);
    EXPECT_TRUE(step_ram[VF2_SOUND_LOOKUP_MODE] == 0u);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S2b: drain-entry 80-head differential. Ring {0x81, 0x06, x},
 * desc[1] = 1, desc[4] = 0x10: base = (0x06&0x7f)+0x10 = 0x16
 * (byte-add; desc[5] never participates). Old word-add C gave
 * (0x01)+be16(desc+4); old command-head gave 0x01+... -- both
 * dead. desc[2] = 0x03 takes the LOW arm (E1g): sel 3 ->
 * table 0x6055e6, walk stops at e0 (0x16 <= 0x52),
 * $1516 = 0x008a, $1515 = byte(0x60c3e6+0x29) = 0xa9; zeroed
 * voices miss-all, the dispatch completes and consumes.
 * S runs the full drain to the 0x265c exit (steps print-first,
 * locked after measurement) with full-RAM + SCSP equality. */
static void test_drain_80_head(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x81u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 6u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 0u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x01u;
    desc[2] = 0x03u;
    desc[4] = 0x10u;
    desc[5] = 0x20u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch drain-80: c-status=%d ch=%u handler=0x%05x base=%02x word=%04x byte=%02x handled=%d count=%u\n",
            (int)cstatus, (unsigned)creport.channel,
            (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)((unsigned)cram[VF2_SOUND_LOOKUP_WORD] << 8u |
                       (unsigned)cram[VF2_SOUND_LOOKUP_WORD + 1u]),
            (unsigned)cram[UINT32_C(0x2515)],
            creport.handled,
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_OK);
    EXPECT_TRUE(creport.channel == 1u);
    EXPECT_TRUE(creport.handled == 1);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] == 0x16u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_WORD] == 0x00u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_WORD + 1u] == 0x8au);
    EXPECT_TRUE(cram[UINT32_C(0x2515)] == 0xb5u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 0u);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch drain-80: s-status=%d pc=0x%05x steps=%u events=%u base=%02x word=%04x byte=%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)((unsigned)step_ram[VF2_SOUND_LOOKUP_WORD] << 8u |
                       (unsigned)step_ram[VF2_SOUND_LOOKUP_WORD + 1u]),
            (unsigned)step_ram[UINT32_C(0x2515)]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
    EXPECT_TRUE(done == 295u);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(step_ram[VF2_SOUND_LOOKUP_CURRENT] == 0x16u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] ==
                step_ram[VF2_SOUND_LOOKUP_CURRENT]);
    {
        uint32_t addr = 0u;
        for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
            if (addr == 0x2506u ||
                (addr >= 0x2508u && addr < 0x250au) ||
                addr >= 0x7e00u) {
                continue;
            }
            if (cram[addr] != step_ram[addr]) {
                fprintf(stderr, "dispatch drain-80: RAM mismatch @0x%05x: C=%02x S=%02x\n",
                        (unsigned)addr, (unsigned)cram[addr],
                        (unsigned)step_ram[addr]);
                EXPECT_TRUE(0);
                break;
            }
        }
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S2c: drain-entry 90-HIGH differential (Loop-B door).
 *
 * Fixture (F11 shape): ring {0x93, 0x00, 0x01}, desc[1] = 3,
 * desc[2] = 0xf5 (HIGH), desc[4] = 0, desc[6] = 0, desc[12] /
 * desc[13] per leg. Table path: a2 = be32(0x6032c6 + 5*4) +
 * (payload&0x7f)*6 on real ROM; gate word != 0x00ff (passes);
 * files $1511/$1512/$1516/$150d (+desc[13] clear); $1515 belongs
 * to Loop-B's own first block (F11: still 0 at the door).
 * C: HIGH body then Loop-B (S2d) to its door; the 0x902 fall-through
 * is no longer a refusal point. This fixture ($150e ch 3, $152e 0)
 * takes the 0x926 early door into the E1o allocator ($152e < 5): $1515 filed,
 * scan skipped, $152e untouched. Packet held. Stepper: drain 0x7a6
 * -> 0xf12.
 * Compared: $150f/$1510/$1511/$1512/$1516/$150d/$1515 + desc[12..13]
 * C==S, SCSP equal, zero events.
 * RED pre-fix: the C refused at the 0x902 door, so $1515 stayed 0
 * while silicon filed it. */
static void test_drain_90_high(
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    uint8_t desc12,
    uint8_t desc13,
    const char *tag
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0xf5u;
    desc[3] = 0x00u;
    desc[4] = 0x00u;
    desc[6] = 0x00u;
    desc[12] = desc12;
    desc[13] = desc13;
    /* Allocator cursor (E1o): the 0x926 early door flows into
     * it; stage pre-snapshot (found voice0 on the zeroed
     * voices). */
    cram[0x251au] = 0x00u;
    cram[0x251au + 1u] = 0x00u;
    cram[0x251au + 2u] = 0x28u;
    cram[0x251au + 3u] = 0x00u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch drain-90h-%s: c-status=%d handler=0x%05x 11=%02x 12=%02x%02x 16=%02x%02x 0d=%02x 15=%02x d13=%02x cursor=%02x%02x%02x%02x count=%u\n",
            tag, (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[0x2511u],
            (unsigned)cram[0x2512u], (unsigned)cram[0x2513u],
            (unsigned)cram[0x2516u], (unsigned)cram[0x2517u],
            (unsigned)cram[0x250du], (unsigned)cram[0x2515u],
            (unsigned)cram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 13u],
            (unsigned)cram[0x251au], (unsigned)cram[0x251au + 1u],
            (unsigned)cram[0x251au + 2u],
            (unsigned)cram[0x251au + 3u],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xfac));
    EXPECT_TRUE(creport.channel == 3u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] == 0x00u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
    EXPECT_TRUE(creport.cursor_after == creport.cursor_before);
    EXPECT_TRUE(cram[0x251au] == 0x00u);
    EXPECT_TRUE(cram[0x251au + 1u] == 0x00u);
    EXPECT_TRUE(cram[0x251au + 2u] == 0x28u);
    EXPECT_TRUE(cram[0x251au + 3u] == 0x10u);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xfac), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch drain-90h-%s: s-status=%d pc=0x%05x steps=%u events=%u 11=%02x 12=%02x%02x 16=%02x%02x 0d=%02x 15=%02x d13=%02x cursor=%02x%02x%02x%02x\n",
            tag, (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[0x2511u],
            (unsigned)step_ram[0x2512u], (unsigned)step_ram[0x2513u],
            (unsigned)step_ram[0x2516u], (unsigned)step_ram[0x2517u],
            (unsigned)step_ram[0x250du], (unsigned)step_ram[0x2515u],
            (unsigned)step_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 13u],
            (unsigned)step_ram[0x251au],
            (unsigned)step_ram[0x251au + 1u],
            (unsigned)step_ram[0x251au + 2u],
            (unsigned)step_ram[0x251au + 3u]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xfac));
    /* Step-pinned $150d sub-branches through the Loop-B early door
       (measured): desc[13]-clear 69, keep 71, table+5 72. Uses the
       params: the C's clear path zeroes desc[13] in RAM before this
       assert runs. */
    /* Steps: pre-allocator pins were 69 / 71 / 72 to the
     * 0xf12 door; the allocator tail (found voice0 first
     * iteration) adds a constant 10. */
    EXPECT_TRUE(done == (desc13 != 0u ? 79u :
                         ((desc12 & 0x07u) != 0u ? 81u : 82u)));
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] ==
                step_ram[VF2_SOUND_LOOKUP_CURRENT]);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_MODE] ==
                step_ram[VF2_SOUND_LOOKUP_MODE]);
    EXPECT_TRUE(cram[0x2511u] == step_ram[0x2511u]);
    EXPECT_TRUE(cram[0x2512u] == step_ram[0x2512u]);
    EXPECT_TRUE(cram[0x2513u] == step_ram[0x2513u]);
    EXPECT_TRUE(cram[0x2516u] == step_ram[0x2516u]);
    EXPECT_TRUE(cram[0x2517u] == step_ram[0x2517u]);
    EXPECT_TRUE(cram[0x250du] == step_ram[0x250du]);
    EXPECT_TRUE(cram[0x2515u] == step_ram[0x2515u]);
    EXPECT_TRUE(cram[0x251au] == step_ram[0x251au]);
    EXPECT_TRUE(cram[0x251au + 1u] == step_ram[0x251au + 1u]);
    EXPECT_TRUE(cram[0x251au + 2u] == step_ram[0x251au + 2u]);
    EXPECT_TRUE(cram[0x251au + 3u] == step_ram[0x251au + 3u]);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 12u] ==
                step_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 12u]);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 13u] ==
                step_ram[VF2_SOUND_COMMAND_DESCRIPTOR_BASE + 13u]);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S2c: drain-entry 90-HIGH gate-hit differential (the 0x8b0 rts).
 *
 * Ring {0x93, 0x6a, 0x01} (payload 0x6a hits a 0x00ff table word
 * in the desc-0xf5 table -- found by the committed sweep's
 * one-off run, hard-pinned here): the head files base/mode, the
 * gate takes the rts back to the drain scan, no other descriptor
 * matches, the drain consumes. C: OK, handler 0x858, head only,
 * consumed. Stepper: drain 0x7a6 -> main-loop 0x265c.
 * Compared: $150f/$1510 C==S, $1511/$1512/$1516/$150d still 0
 * both sides, SCSP equal, zero events. */
static void test_drain_90_gate(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0x6au;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0xf5u;
    desc[4] = 0x00u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch drain-90g: c-status=%d handler=0x%05x base=%02x mode=%02x 11=%02x 0d=%02x count=%u cursor=%u handled=%d\n",
            (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)cram[VF2_SOUND_LOOKUP_MODE],
            (unsigned)cram[0x2511u], (unsigned)cram[0x250du],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)creport.cursor_after, (int)creport.handled);
    EXPECT_TRUE(cstatus == VF2_OK);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0x858));
    EXPECT_TRUE(creport.channel == 3u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] == 0x6au);
    EXPECT_TRUE(cram[0x2511u] == 0u);
    EXPECT_TRUE(cram[0x2512u] == 0u && cram[0x2516u] == 0u);
    EXPECT_TRUE(cram[0x250du] == 0u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 0u);
    EXPECT_TRUE(creport.cursor_after == 4u);
    EXPECT_TRUE(creport.handled != 0);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch drain-90g: s-status=%d pc=0x%05x steps=%u events=%u base=%02x mode=%02x 11=%02x 0d=%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)step_ram[VF2_SOUND_LOOKUP_MODE],
            (unsigned)step_ram[0x2511u], (unsigned)step_ram[0x250du]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
    EXPECT_TRUE(done == 132u);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CURRENT] ==
                step_ram[VF2_SOUND_LOOKUP_CURRENT]);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_MODE] ==
                step_ram[VF2_SOUND_LOOKUP_MODE]);
    EXPECT_TRUE(step_ram[0x2511u] == 0u);
    EXPECT_TRUE(step_ram[0x2512u] == 0u && step_ram[0x2516u] == 0u);
    EXPECT_TRUE(step_ram[0x250du] == 0u);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S2d: drain-entry Loop-B differentials (doors past the 90 HIGH path).
 *
 * Shared fixture: ring {cmd, 0x00, 0x01}, desc[1] = cmd&0xf,
 * desc[2] = 0xf5 (HIGH), desc[4] = 0, $152e + voice0 staged per
 * leg. The 90 HIGH body runs identically every leg (11=00
 * 12=e000 16=0140); Loop-B then diverges:
 * - "early": $150e ch 3, $152e 0 -> 0x926 door into the
 *   allocator ($1515 filed, scan skipped, $152e untouched). Covered by the S2c
 *   legs; re-pinned here through the Loop-B C for the record.
 * - "miss": $150e ch 3, $152e 7, all voices +9 = 0 -> full
 *   32-scan, $152e zeroed, allocator door 0xfac.
 * - "arm": $150e ch 3, $152e 5, voice0 {slot 0x0020, +9 0xff,
 *   +5 0} -> scan hits voice0 at once; SCSP[0x2b] = 0x1f,
 *   SCSP[0x20] = 0x10, ram[0x2020] = 0, voice0+5 5->4
 *   (self-rebalance: the arm sets 5, the rebalance loop
 *   decrements every 0xff-aged voice including itself), $152e
 *   5->4. S3b rolls this leg forward: the armed voice is authored
 *   through the HIGH arm to the 0xcb2 copy loop (stop/door/steps
 *   updated; voice/SCSP coverage lives in the a8c-hi leg).
 * - "setup": $150e ch 0xf (cmd 0x9f, desc[1] 0x0f) -> 0x9a2
 *   branch on $1515 (door 0x9ba if b6/b8 else 0xf12).
 * C: Loop-B effects + door handler, UNSUPPORTED, packet held.
 * Compared: $1515/$152e/voice0 bytes + the HIGH set, C==S;
 * SCSP equal; zero events. Step counts printed, pinned after
 * the first measure run. */
static void test_drain_loopb(
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    uint8_t command,
    uint8_t payload,
    uint8_t preset_152e,
    uint8_t voice0_9,
    uint8_t voice0_5,
    uint32_t stop_pc,
    uint32_t door,
    uint32_t steps,
    const char *tag
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint8_t *cvoice0 = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = command;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = payload;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = (uint8_t)(command & 0x0fu);
    desc[2] = 0xf5u;
    desc[4] = 0x00u;
    desc[12] = 0x01u;
    desc[13] = 0x00u;
    cram[VF2_SOUND_SHARED_COUNTER_152E] = preset_152e;
    cvoice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    cvoice0[0] = 0x00u;
    cvoice0[1] = 0x20u;
    cvoice0[5] = voice0_5;
    cvoice0[9] = voice0_9;
    /* Allocator cursor (E1o): all three 0xf12 exits flow into
     * it; staged pre-snapshot (found voice0 on these
     * fixtures). */
    cram[0x251au] = 0x00u;
    cram[0x251au + 1u] = 0x00u;
    cram[0x251au + 2u] = 0x28u;
    cram[0x251au + 3u] = 0x00u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch loopb-%s: c-status=%d handler=0x%05x 15=%02x 2e=%02x v0=%02x%02x/%02x/%02x cursor=%02x%02x%02x%02x count=%u\n",
            tag, (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[0x2515u],
            (unsigned)cram[VF2_SOUND_SHARED_COUNTER_152E],
            (unsigned)cram[0x2800u], (unsigned)cram[0x2801u],
            (unsigned)cram[0x2805u], (unsigned)cram[0x2809u],
            (unsigned)cram[0x251au], (unsigned)cram[0x251au + 1u],
            (unsigned)cram[0x251au + 2u],
            (unsigned)cram[0x251au + 3u],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == door);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
    EXPECT_TRUE(creport.cursor_after == creport.cursor_before);
    /* Allocator-found cursor (E1o): the three 0xfac legs land
     * on free voice0. The arm leg (0xcb2 door) never runs it. */
    if (door == UINT32_C(0xfac)) {
        EXPECT_TRUE(cram[0x251au] == 0x00u);
        EXPECT_TRUE(cram[0x251au + 1u] == 0x00u);
        EXPECT_TRUE(cram[0x251au + 2u] == 0x28u);
        EXPECT_TRUE(cram[0x251au + 3u] == 0x10u);
    }

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, stop_pc, 0, 20000u, &done);
    fprintf(stderr, "dispatch loopb-%s: s-status=%d pc=0x%05x steps=%u events=%u 15=%02x 2e=%02x v0=%02x%02x/%02x/%02x scsp20=%02x scsp2b=%02x ram2020=%02x\n",
            tag, (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[0x2515u],
            (unsigned)step_ram[VF2_SOUND_SHARED_COUNTER_152E],
            (unsigned)step_ram[0x2800u], (unsigned)step_ram[0x2801u],
            (unsigned)step_ram[0x2805u], (unsigned)step_ram[0x2809u],
            (unsigned)step_scsp.registers[0x20u],
            (unsigned)step_scsp.registers[0x2bu],
            (unsigned)step_ram[0x2020u]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == stop_pc);
    EXPECT_TRUE(done == steps);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(cram[0x2515u] == step_ram[0x2515u]);
    EXPECT_TRUE(cram[0x251au] == step_ram[0x251au]);
    EXPECT_TRUE(cram[0x251au + 1u] == step_ram[0x251au + 1u]);
    EXPECT_TRUE(cram[0x251au + 2u] == step_ram[0x251au + 2u]);
    EXPECT_TRUE(cram[0x251au + 3u] == step_ram[0x251au + 3u]);
    EXPECT_TRUE(cram[VF2_SOUND_SHARED_COUNTER_152E] ==
                step_ram[VF2_SOUND_SHARED_COUNTER_152E]);
    EXPECT_TRUE(cram[0x2805u] == step_ram[0x2805u]);
    EXPECT_TRUE(cram[0x2809u] == step_ram[0x2809u]);
    EXPECT_TRUE(cram[0x2020u] == step_ram[0x2020u]);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S3a: drain-entry 0x9ba scan differential (setup teardown).
 *
 * Shared fixture: ring {0x9f, 0x2a, 0x01} (ch 0xf, payload 0x2a
 * -> base 0x2a -> $1515 0xb6 -> 0x9a2 takes the 0x9ba door),
 * desc[1] = 0x0f, desc[2] = 0xf5, $151e preset 0x20, voice0/1
 * staged per leg (rest zero). The scan tears down EVERY voice
 * with +2 != 0, +3 == 0xba, +4 == 0x0f (not just the first):
 * +2 = 0, d2 = +9, +9 = 0, SCSP[slot] = 0x10,
 * ram[0x2000+slot] = 0, $151e--, then the V1-shaped +9 rebalance
 * (skip 0xff slots; unsigned d2 >= mem skips; else mem--) with
 * the CURRENT d2. No key-off (no 0x1f write -- teardown is not
 * expiry). End: bra 0xf12 into the E1o allocator, packet held.
 * - "two": voice0 {slot 0x20, +2 0x40, +9 3} then voice1 {slot
 *   0x40, +2 1, +9 7}: v0 teardown (d2 = 3) rebalances v1 7->6;
 *   v1 teardown (d2 = 6) finds all +9 zero. Final: both +2/+9
 *   zero, $151e 0x1e, SCSP[0x20]/[0x40] = 0x10.
 * - "skip": v0 {+3 0xbb} + v1 {+4 0x0e, +2 0} mismatches stay
 *   fully staged; $151e untouched.
 * Compared: all 32 voices' +2/+9 C==S, $151e C==S, stream slots
 * 0x2020/0x2040 C==S, SCSP equal, zero events.
 * RED pre-fix: the C refuses at the 0x9ba door, so voices stay
 * staged while silicon tears them down. */
static void test_9ba_scan(
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    uint8_t v0_2,
    uint8_t v0_3,
    uint8_t v0_4,
    uint8_t v0_9,
    uint8_t v1_2,
    uint8_t v1_3,
    uint8_t v1_4,
    uint8_t v1_9,
    uint32_t steps,
    uint32_t expect_cursor,
    const char *tag
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint8_t *voice = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;
    size_t i = 0u;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x9fu;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0x2au;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x0fu;
    desc[2] = 0xf5u;
    desc[4] = 0x00u;
    desc[12] = 0x01u;
    desc[13] = 0x00u;
    cram[VF2_SOUND_SHARED_COUNTER_151E] = 0x20u;
    voice = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice[0] = 0x00u;
    voice[1] = 0x20u;
    voice[2] = v0_2;
    voice[3] = v0_3;
    voice[4] = v0_4;
    voice[9] = v0_9;
    voice[16] = 0x00u;
    voice[17] = 0x40u;
    voice[18] = v1_2;
    voice[19] = v1_3;
    voice[20] = v1_4;
    voice[25] = v1_9;
    /* Allocator cursor (E1o): the 9ba tail bras to 0xf12;
     * staged pre-snapshot (found = first +2 == 0 voice). */
    cram[0x251au] = 0x00u;
    cram[0x251au + 1u] = 0x00u;
    cram[0x251au + 2u] = 0x28u;
    cram[0x251au + 3u] = 0x00u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch 9ba-%s: c-status=%d handler=0x%05x 1e=%02x v0=%02x/%02x v1=%02x/%02x cursor=%02x%02x%02x%02x count=%u\n",
            tag, (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_SHARED_COUNTER_151E],
            (unsigned)cram[0x2802u], (unsigned)cram[0x2809u],
            (unsigned)cram[0x2812u], (unsigned)cram[0x2819u],
            (unsigned)cram[0x251au], (unsigned)cram[0x251au + 1u],
            (unsigned)cram[0x251au + 2u],
            (unsigned)cram[0x251au + 3u],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xfac));
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
    EXPECT_TRUE(creport.cursor_after == creport.cursor_before);
    EXPECT_TRUE(((uint32_t)cram[0x251au] << 24u |
                 ((uint32_t)cram[0x251au + 1u] << 16u) |
                 ((uint32_t)cram[0x251au + 2u] << 8u) |
                 cram[0x251au + 3u]) == expect_cursor);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xfac), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch 9ba-%s: s-status=%d pc=0x%05x steps=%u events=%u 1e=%02x v0=%02x/%02x v1=%02x/%02x cursor=%02x%02x%02x%02x\n",
            tag, (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[VF2_SOUND_SHARED_COUNTER_151E],
            (unsigned)step_ram[0x2802u], (unsigned)step_ram[0x2809u],
            (unsigned)step_ram[0x2812u], (unsigned)step_ram[0x2819u],
            (unsigned)step_ram[0x251au],
            (unsigned)step_ram[0x251au + 1u],
            (unsigned)step_ram[0x251au + 2u],
            (unsigned)step_ram[0x251au + 3u]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xfac));
    EXPECT_TRUE(done == steps);
    EXPECT_TRUE(bus.event_count == 0u);
    for (i = 0u; i < VF2_SOUND_VOICE_COUNT; ++i) {
        uint8_t *cv = cram + VF2_SOUND_VOICE_TABLE_BASE +
            i * VF2_SOUND_VOICE_BYTES;
        uint8_t *sv = step_ram + VF2_SOUND_VOICE_TABLE_BASE +
            i * VF2_SOUND_VOICE_BYTES;
        if (cv[2] != sv[2] || cv[9] != sv[9]) {
            fprintf(stderr, "dispatch 9ba-%s: voice %u +2/+9 differ: C=%02x/%02x S=%02x/%02x\n",
                    tag, (unsigned)i, (unsigned)cv[2], (unsigned)cv[9],
                    (unsigned)sv[2], (unsigned)sv[9]);
            EXPECT_TRUE(0);
        }
    }
    EXPECT_TRUE(cram[VF2_SOUND_SHARED_COUNTER_151E] ==
                step_ram[VF2_SOUND_SHARED_COUNTER_151E]);
    EXPECT_TRUE(cram[0x251au] == step_ram[0x251au]);
    EXPECT_TRUE(cram[0x251au + 1u] == step_ram[0x251au + 1u]);
    EXPECT_TRUE(cram[0x251au + 2u] == step_ram[0x251au + 2u]);
    EXPECT_TRUE(cram[0x251au + 3u] == step_ram[0x251au + 3u]);
    EXPECT_TRUE(cram[0x2020u] == step_ram[0x2020u]);
    EXPECT_TRUE(cram[0x2040u] == step_ram[0x2040u]);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S3b: drain-entry 0xa8c HIGH authoring differential.
 *
 * Fixture: ring {0x93, 0x00, 0x01} (ch 3), desc[1] = 3,
 * desc[2] = 0xf5 (HIGH), desc[4] = 0, desc[8..9] = 0x0201,
 * desc[10] = 0x80, $152e = 5, voice0 {slot 0x0020, +9 0xff,
 * +5 0}, ram[0x4223] = 0x07. Loop-B arms voice0 (0x95a), the
 * 0xa8c door authors it through the HIGH branch:
 * voice+2 0x40->0x50->0x58, +6 = $1516, +3 = $1515, +8 = $1510,
 * +4 = channel, +9 = 0xff / +5 = $152e (ch != 0xf, $152e 4->5),
 * volume SCSP[slot+0x0d] = ~((voice8*desc10)>>8)>>1 (0x09*0x80
 * -> 0x7d, by execution), jsr-0x1014 SCSP[slot+0x10],
 * SCSP[slot+4] = 0x00001fff, +0x0c = 0, +0x0e = 0,
 * SCSP[slot+0x12] = desc8w | ((chanword<<5)&0xe0),
 * +0x15 = $150d, +0x16 = ($1511&0x1f)|0x60. Refuse at 0xc1e
 * (common tail, S3c), packet held. All real-ROM/table values
 * asserted as C==S equality, never hand-derived.
 * RED pre-fix: the C refuses at the 0xa8c door, so voice0
 * keeps its armed shape while silicon authors it. */
static void test_a8c_high(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint8_t *voice0 = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;
    size_t i = 0u;
    static const uint32_t words[] = { 0x02u, 0x04u, 0x06u, 0x08u,
                                      0x0du, 0x0eu, 0x10u, 0x12u,
                                      0x15u, 0x16u };

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0xf5u;
    desc[4] = 0x00u;
    desc[8] = 0x02u;
    desc[9] = 0x01u;
    desc[10] = 0x80u;
    desc[12] = 0x01u;
    desc[13] = 0x00u;
    cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    cram[0x4223u] = 0x07u;
    voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[0] = 0x00u;
    voice0[1] = 0x20u;
    voice0[5] = 0x00u;
    voice0[9] = 0xffu;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch a8c-hi: c-status=%d handler=0x%05x v0=%02x/%02x/%02x/%02x/%02x/%02x 2e=%02x count=%u\n",
            (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[0x2802u], (unsigned)cram[0x2803u],
            (unsigned)cram[0x2804u], (unsigned)cram[0x2805u],
            (unsigned)cram[0x2806u], (unsigned)cram[0x2808u],
            (unsigned)cram[VF2_SOUND_SHARED_COUNTER_152E],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xcb2));
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
    EXPECT_TRUE(creport.cursor_after == creport.cursor_before);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xcb2), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch a8c-hi: s-status=%d pc=0x%05x steps=%u events=%u v0=%02x/%02x/%02x/%02x/%02x/%02x 2e=%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[0x2802u], (unsigned)step_ram[0x2803u],
            (unsigned)step_ram[0x2804u], (unsigned)step_ram[0x2805u],
            (unsigned)step_ram[0x2806u], (unsigned)step_ram[0x2808u],
            (unsigned)step_ram[VF2_SOUND_SHARED_COUNTER_152E]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xcb2));
    EXPECT_TRUE(done == 322u);
    EXPECT_TRUE(bus.event_count == 0u);
    for (i = 0u; i < VF2_SOUND_VOICE_BYTES; ++i) {
        if (cram[VF2_SOUND_VOICE_TABLE_BASE + i] !=
            step_ram[VF2_SOUND_VOICE_TABLE_BASE + i]) {
            fprintf(stderr, "dispatch a8c-hi: voice0[%u] differ: C=%02x S=%02x\n",
                    (unsigned)i,
                    (unsigned)cram[VF2_SOUND_VOICE_TABLE_BASE + i],
                    (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + i]);
            EXPECT_TRUE(0);
        }
    }
    EXPECT_TRUE(cram[VF2_SOUND_SHARED_COUNTER_152E] ==
                step_ram[VF2_SOUND_SHARED_COUNTER_152E]);
    EXPECT_TRUE(cram[VF2_SOUND_SHARED_COUNTER_151E] ==
                step_ram[VF2_SOUND_SHARED_COUNTER_151E]);
    for (i = 0u; i < sizeof(words) / sizeof(words[0]); ++i) {
        uint32_t off = 0x20u + words[i];
        if (board.scsp.registers[off] != step_scsp.registers[off]) {
            fprintf(stderr, "dispatch a8c-hi: SCSP[0x%02x] differ: C=%02x S=%02x\n",
                    (unsigned)off, (unsigned)board.scsp.registers[off],
                    (unsigned)step_scsp.registers[off]);
            EXPECT_TRUE(0);
        }
    }
    {
        uint16_t c10 = 0u;
        uint16_t s10 = 0u;
        (void)vf2_scsp_read_u16(&board.scsp, 0x30u, &c10);
        (void)vf2_scsp_read_u16(&step_scsp, 0x30u, &s10);
        fprintf(stderr, "dispatch a8c-hi: SCSP[0x30] C=%04x S=%04x\n",
                (unsigned)c10, (unsigned)s10);
        EXPECT_TRUE(c10 == s10);
    }
    /* The jsr's d2 (row-table word, discarded by the HIGH caller)
     * is pinned against the report's lookup word -- now modulo the
     * engine program's addi.w #$eba (S d2 at 0xcb2 is rate+0xeba
     * by construction; 16-bit wrap exact). */
    fprintf(stderr, "dispatch a8c-hi: d2 C=%04x S=%04x\n",
            (unsigned)creport.lookup_word, (unsigned)regs.d[2]);
    EXPECT_TRUE(creport.lookup_word ==
                (uint16_t)(regs.d[2] - UINT32_C(0xeba)));
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* E1i: 0xd92 second engine arm (staged gate). Same a8c-hi
 * fixture (d3 = 0, table at 0x5000) plus voice0[2] = 0x5a
 * (bclr witness) and table+0x0c = 0x00112233 (the gate long,
 * head-loaded into d0 -- synthetic scenario, ROM-exact logic,
 * E1c-gate precedent). C runs the arm and refuses at the
 * 0xdf8 dead loop with effects applied; S arrives at 0xdf8
 * (steps print-first, locked after measurement). Compared:
 * voice0 record incl. the bclr, stream RMW longs, rate/word
 * lanes, d2 lookup relation, full SCSP. */
static void test_engine_d92(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint8_t *voice0 = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;
    size_t i = 0u;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0xf5u;
    desc[4] = 0x00u;
    desc[8] = 0x02u;
    desc[9] = 0x01u;
    desc[10] = 0x80u;
    desc[12] = 0x01u;
    desc[13] = 0x00u;
    cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    cram[0x4223u] = 0x07u;
    voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[0] = 0x00u;
    voice0[1] = 0x20u;
    voice0[2] = 0x5au;
    voice0[5] = 0x00u;
    voice0[9] = 0xffu;
    /* Gate long: d3 = (voice+6 & 0x7ff) * 20 with setup-authored
     * +6 = 0x0140 -> d3 = 0x1900, table 0x6900, gate at 0x690c
     * (measured, not assumed -- the pre-dispatch +6 is zero). */
    cram[0x690cu] = 0x00u;
    cram[0x690du] = 0x11u;
    cram[0x690eu] = 0x22u;
    cram[0x690fu] = 0x33u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch d92: c-status=%d handler=0x%05x v0+2=%02x v0+6=%02x%02x str16=%02x%02x%02x%02x str12=%02x%02x%02x%02x str0a=%02x%02x%02x%02x count=%u\n",
            (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)voice0[VF2_SOUND_VOICE_FLAGS],
            (unsigned)voice0[VF2_SOUND_VOICE_SELECT],
            (unsigned)voice0[VF2_SOUND_VOICE_SELECT + 1u],
            (unsigned)cram[0x2036u], (unsigned)cram[0x2037u],
            (unsigned)cram[0x2038u], (unsigned)cram[0x2039u],
            (unsigned)cram[0x2032u], (unsigned)cram[0x2033u],
            (unsigned)cram[0x2034u], (unsigned)cram[0x2035u],
            (unsigned)cram[0x202au], (unsigned)cram[0x202bu],
            (unsigned)cram[0x202cu], (unsigned)cram[0x202du],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xdf8));
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
    EXPECT_TRUE(creport.cursor_after == creport.cursor_before);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xdf8), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch d92: s-status=%d pc=0x%05x steps=%u events=%u d0=%08x d1=%08x v0+2=%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count, (unsigned)regs.d[0],
            (unsigned)regs.d[1],
            (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + 2u]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xdf8));
    EXPECT_TRUE(done == 334u);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(regs.d[0] == UINT32_C(0x00112233));
    EXPECT_TRUE(regs.d[1] == UINT32_C(0));
    /* Arm effects, both sides (measured above): bclr bit4,
     * stream sample long (d5 = 0 - 0x200), the RMW pair. */
    EXPECT_TRUE(step_ram[VF2_SOUND_VOICE_TABLE_BASE + 2u] ==
                0x48u);
    EXPECT_TRUE(voice0[VF2_SOUND_VOICE_FLAGS] == 0x48u);
    EXPECT_TRUE(cram[0x2036u] == 0x00u);
    EXPECT_TRUE(cram[0x2037u] == 0x11u);
    EXPECT_TRUE(cram[0x2038u] == 0x22u);
    EXPECT_TRUE(cram[0x2039u] == 0x33u);
    EXPECT_TRUE(cram[0x2032u] == 0x00u);
    EXPECT_TRUE(cram[0x2035u] == 0x00u);
    EXPECT_TRUE(cram[0x202au] == 0xffu);
    EXPECT_TRUE(cram[0x202du] == 0x00u);
    EXPECT_TRUE(cram[0x2021u] == 0x10u);
    EXPECT_TRUE(board.scsp.registers[0x20u] == 0x10u);
    EXPECT_TRUE(board.scsp.registers[0x21u] == 0x30u);
    for (i = 0u; i < VF2_SOUND_VOICE_BYTES; ++i) {
        if (cram[VF2_SOUND_VOICE_TABLE_BASE + i] !=
            step_ram[VF2_SOUND_VOICE_TABLE_BASE + i]) {
            fprintf(stderr, "dispatch d92: voice0[%u] differ: C=%02x S=%02x\n",
                    (unsigned)i,
                    (unsigned)cram[VF2_SOUND_VOICE_TABLE_BASE + i],
                    (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + i]);
            EXPECT_TRUE(0);
        }
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* E1k: 90 LOW row walk (S2d backlog) + E1m allocator. a8c-hi-shaped
 * fixture with a LOW desc (0x80/0x03/0x05, ring {0x93, 0x05,
 * 0x01}) plus the allocator fixture ($151a = 0x2800, voice0
 * busy): base = 0x05, sel 5 walks to e0 (0x05 <= 0x7f) with
 * the wrapping addend (0x05 + 0xfa = 0xff -> index 0x7f),
 * files $1516/$1518/$1515/$1511, then the allocator skips
 * voice0 (+2 = 0x40) to voice1 (cursor 0x2820) and refuses
 * at the 0xfac setup authoring with the packet held. S runs
 * drain -> walk -> allocator to 0xfac (steps print-first,
 * locked after measurement) with full-RAM (drain exclusions)
 * + SCSP equality. Filed cells print-first. */
static void test_dispatch_90_low(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint8_t *voice0 = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0x05u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0x05u;
    desc[4] = 0x00u;
    desc[6] = 0x07u;
    desc[8] = 0x02u;
    desc[9] = 0x01u;
    desc[10] = 0x80u;
    desc[12] = 0x01u;
    desc[13] = 0x00u;
    cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    cram[0x4223u] = 0x07u;
    voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[0] = 0x00u;
    voice0[1] = 0x20u;
    voice0[5] = 0x00u;
    voice0[9] = 0xffu;
    /* Allocator fixture (E1m): cursor at the table base, voice0
     * busy so the scan skips to voice1 (found -> cursor
     * 0x2820, refuse at 0xfac). */
    cram[0x251au] = 0x00u;
    cram[0x251au + 1u] = 0x00u;
    cram[0x251au + 2u] = 0x28u;
    cram[0x251au + 3u] = 0x00u;
    voice0[VF2_SOUND_VOICE_FLAGS] = 0x40u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch 90-low: c-status=%d handler=0x%05x base=%02x word=%02x%02x meta=%02x byte=%02x ctl=%02x count=%u\n",
            (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_LOOKUP_CURRENT],
            (unsigned)cram[VF2_SOUND_LOOKUP_WORD],
            (unsigned)cram[VF2_SOUND_LOOKUP_WORD + 1u],
            (unsigned)cram[VF2_SOUND_LOOKUP_META],
            (unsigned)cram[UINT32_C(0x2515)],
            (unsigned)cram[VF2_SOUND_LOOKUP_CONTROL],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xfac));
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
    EXPECT_TRUE(creport.cursor_after == creport.cursor_before);
    /* Walk effects (sel-5 e0 stop, wrapping addend): $1516 =
     * 0x0090, $1518 = 0x00, $1515 = 0x27 (index 0x7f),
     * $1511 = desc[6] = 0x07. S prints identical cells. */
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_WORD] == 0x00u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_WORD + 1u] == 0x90u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_META] == 0x00u);
    EXPECT_TRUE(cram[UINT32_C(0x2515)] == 0x27u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_CONTROL] == 0x07u);
    /* Allocator effects (E1m): voice0 busy, voice1 free ->
     * cursor 0x2820. */
    EXPECT_TRUE(cram[0x251au] == 0x00u);
    EXPECT_TRUE(cram[0x251au + 1u] == 0x00u);
    EXPECT_TRUE(cram[0x251au + 2u] == 0x28u);
    EXPECT_TRUE(cram[0x251au + 3u] == 0x20u);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xfac), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch 90-low: s-status=%d pc=0x%05x steps=%u events=%u word=%02x%02x meta=%02x byte=%02x ctl=%02x cursor=%02x%02x%02x%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[VF2_SOUND_LOOKUP_WORD],
            (unsigned)step_ram[VF2_SOUND_LOOKUP_WORD + 1u],
            (unsigned)step_ram[VF2_SOUND_LOOKUP_META],
            (unsigned)step_ram[UINT32_C(0x2515)],
            (unsigned)step_ram[VF2_SOUND_LOOKUP_CONTROL],
            (unsigned)step_ram[0x251au],
            (unsigned)step_ram[0x251au + 1u],
            (unsigned)step_ram[0x251au + 2u],
            (unsigned)step_ram[0x251au + 3u]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xfac));
    EXPECT_TRUE(done == 67u);
    EXPECT_TRUE(bus.event_count == 0u);
    {
        uint32_t addr = 0u;
        for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
            if (addr == 0x2506u ||
                (addr >= 0x2508u && addr < 0x250au) ||
                addr >= 0x7e00u) {
                continue;
            }
            if (cram[addr] != step_ram[addr]) {
                fprintf(stderr, "dispatch 90-low: RAM mismatch @0x%05x: C=%02x S=%02x\n",
                        (unsigned)addr, (unsigned)cram[addr],
                        (unsigned)step_ram[addr]);
                EXPECT_TRUE(0);
                break;
            }
        }
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* E1o-exhausted + E1r completion: allocator dbf-exhaustion to
 * the 0xf50 reaper (all 32 voices busy, +2 = 0x40 / +9 =
 * 0x40), then the reaper's own dbf exhaustion at 0xf68. E1r
 * pinned the return: the rts pops the drain's jsr 0x81a return
 * and lands at 0x7f6, the descriptor scan resumes with no
 * writes, the drain tail advances the ring cursor by 4 at
 * 0x804 and control exits at 0x265c. The command completes
 * consumed with no authoring -- the C returns OK and
 * dispatch_next's consume block models the drain tail
 * (count 0, cursor 0 -> 4). S phases: arrival at 0xf50 (246
 * steps, print-first), the rts return (0x7f6), the drain tail
 * (0x265c). Full-RAM + SCSP equality with no drain exclusions
 * (both sides agree on count/cursor here). */
static void test_allocator_exhausted(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;
    size_t vi = 0u;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0x05u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0x05u;
    desc[4] = 0x00u;
    desc[6] = 0x07u;
    desc[8] = 0x02u;
    desc[9] = 0x01u;
    desc[10] = 0x80u;
    desc[12] = 0x01u;
    desc[13] = 0x00u;
    cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    cram[0x4223u] = 0x07u;
    cram[0x251au] = 0x00u;
    cram[0x251au + 1u] = 0x00u;
    cram[0x251au + 2u] = 0x28u;
    cram[0x251au + 3u] = 0x00u;
    for (vi = 0u; vi < VF2_SOUND_VOICE_COUNT; ++vi) {
        cram[VF2_SOUND_VOICE_TABLE_BASE +
             vi * VF2_SOUND_VOICE_BYTES +
             VF2_SOUND_VOICE_FLAGS] = 0x40u;
        /* Reaper-exhausted needs no age-zero voice either
         * (E1p: +9 == 0 would be found and authored). */
        cram[VF2_SOUND_VOICE_TABLE_BASE +
             vi * VF2_SOUND_VOICE_BYTES +
             VF2_SOUND_VOICE_AGE] = 0x40u;
    }
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch alloc-x: c-status=%d handler=0x%05x cursor=%02x%02x%02x%02x count=%u\n",
            (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[0x251au], (unsigned)cram[0x251au + 1u],
            (unsigned)cram[0x251au + 2u],
            (unsigned)cram[0x251au + 3u],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_OK);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xf50));
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 0u);
    EXPECT_TRUE(creport.cursor_before == 0u);
    EXPECT_TRUE(creport.cursor_after == 4u);
    EXPECT_TRUE(cram[0x251au] == 0x00u);
    EXPECT_TRUE(cram[0x251au + 1u] == 0x00u);
    EXPECT_TRUE(cram[0x251au + 2u] == 0x28u);
    EXPECT_TRUE(cram[0x251au + 3u] == 0x00u);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xf50), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch alloc-x: s-status=%d pc=0x%05x steps=%u events=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xf50));
    EXPECT_TRUE(done == 246u);
    EXPECT_TRUE(bus.event_count == 0u);
    /* The drain entry already consumed the count (0x7b4) and the
       jsr 0x81a return is still on the stack: a7 = 0x7f00 - 4
       (0x7b0 movem) - 16 (0x7ee movem) - 4 (jsr). */
    fprintf(stderr, "dispatch alloc-x: s-a7=0x%05x count=%u cursor=%02x%02x\n",
            (unsigned)regs.a[7],
            (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)step_ram[0x2508u],
            (unsigned)step_ram[0x2509u]);
    EXPECT_TRUE(step_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
    EXPECT_TRUE(step_ram[0x2508u] == 0u && step_ram[0x2509u] == 0u);
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7ee8));

    /* E1r phase 2: the reaper's dbf exhaustion falls into the
       0xf68 rts, which pops the drain's jsr return -- 0x7f6. */
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x7f6), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch alloc-x: s-rts pc=0x%05x steps=%u a7=0x%05x\n",
            (unsigned)regs.pc, (unsigned)done, (unsigned)regs.a[7]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x7f6));
    EXPECT_TRUE(done == 131u);
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7eec));

    /* E1r phase 3: the drain tail -- the descriptor scan resumes
       (descriptors 1..19 do not match), the ring cursor advances
       by 4 at 0x804 and control exits at 0x265c. */
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch alloc-x: s-tail pc=0x%05x steps=%u count=%u cursor=%02x%02x\n",
            (unsigned)regs.pc, (unsigned)done,
            (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)step_ram[0x2508u],
            (unsigned)step_ram[0x2509u]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
    EXPECT_TRUE(done == 84u);
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(step_ram[VF2_SOUND_COMMAND_COUNT] == 0u);
    EXPECT_TRUE(step_ram[0x2508u] == 0u && step_ram[0x2509u] == 0x04u);
    {
        uint32_t addr = 0u;
        for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
            if (addr >= 0x7e00u) {
                continue;
            }
            if (cram[addr] != step_ram[addr]) {
                fprintf(stderr, "dispatch alloc-x: RAM mismatch @0x%05x: C=%02x S=%02x\n",
                        (unsigned)addr, (unsigned)cram[addr],
                        (unsigned)step_ram[addr]);
                EXPECT_TRUE(0);
                break;
            }
        }
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* E1p: reaper-found differential (9/10 arc). 90-HIGH drain
 * fixture (a8c-hi shape: ring {0x93, 0x00, 1}, desc[2] 0xf5)
 * but Loop-B CANNOT arm: no voice carries +9 == 0xff with
 * +5 == 0 (voice0 +9 = 0, the rest +9 = 0x40), so the scan
 * misses ($152e 5->0), the allocator exhausts (all +2 =
 * 0x40, cursor 0x2800 untouched), and the reaper finds
 * voice0 on its first iteration (slot 0x0020 even: the
 * setup-author odd guard passes). Reaper effects: SCSP
 * key bytes, voice0+2 = 0, ram[0x2020] = 0, rebalance
 * (every non-0xff age--, voice0 0->0xff), $151e--, then
 * the existing HIGH author to the 0xcb2 door (678 steps).
 * Compared: full-RAM (drain exclusions) + full-SCSP C==S, zero events,
 * packet held. */
static void test_reaper_found(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint8_t *voice = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;
    size_t vi = 0u;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0x00u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0xf5u;
    desc[4] = 0x00u;
    desc[8] = 0x02u;
    desc[9] = 0x01u;
    desc[10] = 0x80u;
    desc[12] = 0x01u;
    desc[13] = 0x00u;
    cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    cram[0x251au] = 0x00u;
    cram[0x251au + 1u] = 0x00u;
    cram[0x251au + 2u] = 0x28u;
    cram[0x251au + 3u] = 0x00u;
    for (vi = 0u; vi < VF2_SOUND_VOICE_COUNT; ++vi) {
        voice = cram + VF2_SOUND_VOICE_TABLE_BASE +
            vi * VF2_SOUND_VOICE_BYTES;
        voice[VF2_SOUND_VOICE_SLOT] = 0x00u;
        voice[VF2_SOUND_VOICE_SLOT + 1u] =
            (uint8_t)(vi * 0x20u);
        voice[VF2_SOUND_VOICE_FLAGS] = 0x40u;
        voice[VF2_SOUND_VOICE_AGE] = 0x40u;
    }
    voice = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice[VF2_SOUND_VOICE_SLOT] = 0x00u;
    voice[VF2_SOUND_VOICE_SLOT + 1u] = 0x20u;
    voice[VF2_SOUND_VOICE_AGE] = 0x00u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch reaper: c-status=%d handler=0x%05x v0=%02x/%02x/%02x/%02x 1e=%02x 2e=%02x cursor=%02x%02x%02x%02x count=%u\n",
            (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[0x2802u], (unsigned)cram[0x2805u],
            (unsigned)cram[0x2809u], (unsigned)cram[0x2803u],
            (unsigned)cram[VF2_SOUND_SHARED_COUNTER_151E],
            (unsigned)cram[VF2_SOUND_SHARED_COUNTER_152E],
            (unsigned)cram[0x251au], (unsigned)cram[0x251au + 1u],
            (unsigned)cram[0x251au + 2u],
            (unsigned)cram[0x251au + 3u],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xcb2));
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
    EXPECT_TRUE(creport.cursor_after == creport.cursor_before);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xcb2), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch reaper: s-status=%d pc=0x%05x steps=%u events=%u v0=%02x/%02x/%02x/%02x 1e=%02x 2e=%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[0x2802u], (unsigned)step_ram[0x2805u],
            (unsigned)step_ram[0x2809u], (unsigned)step_ram[0x2803u],
            (unsigned)step_ram[VF2_SOUND_SHARED_COUNTER_151E],
            (unsigned)step_ram[VF2_SOUND_SHARED_COUNTER_152E]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xcb2));
    EXPECT_TRUE(done == 678u);
    EXPECT_TRUE(bus.event_count == 0u);
    {
        uint32_t addr = 0u;
        for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
            if (addr == 0x2506u ||
                (addr >= 0x2508u && addr < 0x250au) ||
                addr >= 0x7e00u) {
                continue;
            }
            if (cram[addr] != step_ram[addr]) {
                fprintf(stderr, "dispatch reaper: RAM mismatch @0x%05x: C=%02x S=%02x\n",
                        (unsigned)addr, (unsigned)cram[addr],
                        (unsigned)step_ram[addr]);
                EXPECT_TRUE(0);
                break;
            }
        }
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* E1q: drain-delivered LOW author differential (9/10 arc).
 * 90-LOW drain fixture (90-low shape: ring {0x93, 0x05, 1},
 * desc[2] 0x05) but the allocator is starved (all +2 =
 * 0x40, cursor 0x2800 untouched) and the reaper is fed
 * (voice0 +9 == 0, slot 0x0020 even: the odd guard passes).
 * Path: drain -> LOW walk (files $1516 = 0x0090, $1518 =
 * 0x00, $1515 = 0x27, $1511 = 0x07 -- same pins as the
 * 90-low leg) -> 0xf12 exhaust -> 0xf50 voice0 found ->
 * bra 0xa8c -> LOW author (desc[2] 0x05 < 0xf0) -> sample
 * tail -> 0xcb2 (zeroed $5000 gate: fall-through, packet
 * held). This is the first dispatcher-fed (not direct-call)
 * proof of the LOW branch -- the a8c-syn-low leg only
 * covers setup_author() with staged registers. Steps
 * print-first, locked after measurement. Compared:
 * full-RAM (drain exclusions) + full-SCSP C==S, zero
 * events, packet held. */
static void test_low_author_drain(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint8_t *voice = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;
    size_t vi = 0u;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0x05u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0x05u;
    desc[4] = 0x00u;
    desc[6] = 0x07u;
    desc[8] = 0x02u;
    desc[9] = 0x01u;
    desc[10] = 0x80u;
    desc[12] = 0x01u;
    desc[13] = 0x00u;
    cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    cram[0x4223u] = 0x07u;
    cram[0x251au] = 0x00u;
    cram[0x251au + 1u] = 0x00u;
    cram[0x251au + 2u] = 0x28u;
    cram[0x251au + 3u] = 0x00u;
    for (vi = 0u; vi < VF2_SOUND_VOICE_COUNT; ++vi) {
        voice = cram + VF2_SOUND_VOICE_TABLE_BASE +
            vi * VF2_SOUND_VOICE_BYTES;
        voice[VF2_SOUND_VOICE_SLOT] = 0x00u;
        voice[VF2_SOUND_VOICE_SLOT + 1u] =
            (uint8_t)(vi * 0x20u);
        voice[VF2_SOUND_VOICE_FLAGS] = 0x40u;
        voice[VF2_SOUND_VOICE_AGE] = 0x40u;
    }
    voice = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice[VF2_SOUND_VOICE_SLOT] = 0x00u;
    voice[VF2_SOUND_VOICE_SLOT + 1u] = 0x20u;
    voice[VF2_SOUND_VOICE_AGE] = 0x00u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "dispatch lowdrain: c-status=%d handler=0x%05x word=%02x%02x meta=%02x byte=%02x v0=%02x/%02x/%02x/%02x 1e=%02x cursor=%02x%02x%02x%02x count=%u\n",
            (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_LOOKUP_WORD],
            (unsigned)cram[VF2_SOUND_LOOKUP_WORD + 1u],
            (unsigned)cram[VF2_SOUND_LOOKUP_META],
            (unsigned)cram[UINT32_C(0x2515)],
            (unsigned)cram[0x2802u], (unsigned)cram[0x2805u],
            (unsigned)cram[0x2809u], (unsigned)cram[0x2803u],
            (unsigned)cram[VF2_SOUND_SHARED_COUNTER_151E],
            (unsigned)cram[0x251au], (unsigned)cram[0x251au + 1u],
            (unsigned)cram[0x251au + 2u],
            (unsigned)cram[0x251au + 3u],
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xcb2));
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
    EXPECT_TRUE(creport.cursor_after == creport.cursor_before);
    /* Walk files (90-low pins, unchanged by the reaper): */
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_WORD] == 0x00u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_WORD + 1u] == 0x90u);
    EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_META] == 0x00u);
    EXPECT_TRUE(cram[UINT32_C(0x2515)] == 0x27u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xcb2), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch lowdrain: s-status=%d pc=0x%05x steps=%u events=%u word=%02x%02x meta=%02x byte=%02x v0=%02x/%02x/%02x/%02x 1e=%02x\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[VF2_SOUND_LOOKUP_WORD],
            (unsigned)step_ram[VF2_SOUND_LOOKUP_WORD + 1u],
            (unsigned)step_ram[VF2_SOUND_LOOKUP_META],
            (unsigned)step_ram[UINT32_C(0x2515)],
            (unsigned)step_ram[0x2802u], (unsigned)step_ram[0x2805u],
            (unsigned)step_ram[0x2809u], (unsigned)step_ram[0x2803u],
            (unsigned)step_ram[VF2_SOUND_SHARED_COUNTER_151E]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xcb2));
    EXPECT_TRUE(done == 532u);
    EXPECT_TRUE(bus.event_count == 0u);
    {
        uint32_t addr = 0u;
        for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
            if (addr == 0x2506u ||
                (addr >= 0x2508u && addr < 0x250au) ||
                addr >= 0x7e00u) {
                continue;
            }
            if (cram[addr] != step_ram[addr]) {
                fprintf(stderr, "dispatch lowdrain: RAM mismatch @0x%05x: C=%02x S=%02x\n",
                        (unsigned)addr, (unsigned)cram[addr],
                        (unsigned)step_ram[addr]);
                EXPECT_TRUE(0);
                break;
            }
        }
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* E1n: 0xfac d4-as-slot verdict (9/10 arc). The allocator
 * path (E1m) arrives at 0xfac with d4 = the walk's $1515
 * index (low word exact, upper drain residue; all d4 uses
 * are .w-indexed so the upper never matters) -- d4 is NEVER
 * reloaded between the walk and the slot stores (0xafa-0x103e
 * verified: no d4 writer). Core-fill arrivals (S3 future)
 * carry d4 = voice slot (0xa92) and are sane; allocator
 * arrivals carry the walk index: odd (here 0x7f) faults on
 * the first SCSP word store, even would program slot-by-index
 * garbage. Either way unenterable: the 0xfac refusal STANDS
 * for allocator arrivals. No src change (verdict slice).
 * Two-phase S: drain -> 0xfac arrival (67 steps, d4 printed
 * and pinned -- the verdict fact), then continue: the stepper
 * has NO address-error model, so it sails past the odd word
 * store at 0x103e. We now stop BEFORE the LEGAL MOVEM at
 * 0xcb6 (114 steps, pinned), not at a nonexistent illegal-MOVEM
 * fault. Real silicon takes vector 2 -> 0x6004d8 (moveq #7,
 * bra 0x4fe blink): the odd-index path blinks, dead. Even
 * indices would program slot-by-index garbage -- modeled by
 * neither side, refused by design. C-side is the 90-low proof
 * (its leg owns the equality); here C only establishes the
 * pre-state. */
static void test_fac_d4_crash(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint8_t *voice0 = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    cram[VF2_SOUND_COMMAND_RING_BASE] = 0x93u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0x05u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 2u] = 1u;
    cram[VF2_SOUND_COMMAND_COUNT] = 1u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x03u;
    desc[2] = 0x05u;
    desc[4] = 0x00u;
    desc[6] = 0x07u;
    desc[8] = 0x02u;
    desc[9] = 0x01u;
    desc[10] = 0x80u;
    desc[12] = 0x01u;
    desc[13] = 0x00u;
    cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    cram[0x4223u] = 0x07u;
    voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[0] = 0x00u;
    voice0[1] = 0x20u;
    voice0[5] = 0x00u;
    voice0[9] = 0xffu;
    cram[0x251au] = 0x00u;
    cram[0x251au + 1u] = 0x00u;
    cram[0x251au + 2u] = 0x28u;
    cram[0x251au + 3u] = 0x00u;
    voice0[VF2_SOUND_VOICE_FLAGS] = 0x40u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xfac));

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xfac), 0, 20000u,
                          &done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xfac));
    EXPECT_TRUE(done == 67u);
    /* Verdict fact: d4 = the walk's $1515 index (low word
     * exact, upper drain residue -- here 0). Never reloaded
     * before the slot stores. */
    EXPECT_TRUE(regs.d[4] == UINT32_C(0x7f));
    fprintf(stderr, "dispatch fac-crash: arrival d4=%08x d2=%08x d5=%08x d6=%08x a0=%08x a1=%08x a2=%08x\n",
            (unsigned)regs.d[4], (unsigned)regs.d[2],
            (unsigned)regs.d[5], (unsigned)regs.d[6],
            (unsigned)regs.a[0], (unsigned)regs.a[1],
            (unsigned)regs.a[2]);
    /* Phase 2: observe the original copy-engine entry with arrival
     * registers intact. MOVEM.L D0-D7,(A2) at 0xcb6 is LEGAL on 68000
     * (v0880); never invoke the old imagined vector-4 fault. */
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xcb6), 0,
                          20000u, &done);
    fprintf(stderr, "dispatch fac-crash: s-status=%d pc=0x%05x steps=%u events=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xcb6));
    EXPECT_TRUE(done == 114u);
}

/* E1j: a0 max-gate two-round differential (9/10 arc). Zeroed
 * board, desc0 channel-0 match, ring {0xa0, 0x20, byte2}:
 * idx 0x20 rows max 0x7f. Round A (byte2 0x80 > 0x7f): the
 * 0x1eac rts resumes the drain -- C OK + consume, S exits
 * 0x265c (steps print-first) with full-RAM + SCSP equality
 * (the head is register-only, so equality is exact). Round B
 * (byte2 0x7f = max): bgt falls to the 0x1e98 join -- C
 * refuses with the packet held; S runs the residue-wild body
 * from the zeroed state (T2: clean 249-step drain exits;
 * steps re-measured, not assumed) with NO post-state equality
 * (the outcomes differ by design -- the pin is the pair of
 * outcomes). */
static void test_dispatch_a0_maxgate(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    int round = 0;
    (void)audio_rom_size;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x00u;
    for (round = 0; round < 2; ++round) {
        vf2_status cstatus = VF2_OK;
        cram[VF2_SOUND_COMMAND_RING_BASE] = 0xa0u;
        cram[VF2_SOUND_COMMAND_RING_BASE + 1u] = 0x20u;
        cram[VF2_SOUND_COMMAND_RING_BASE + 2u] =
            round == 0 ? 0x80u : 0x7fu;
        cram[VF2_SOUND_COMMAND_RING_BASE + 3u] = 0u;
        cram[VF2_SOUND_COMMAND_COUNT] = 1u;
        cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
        cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
        memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
        memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
        memset(&creport, 0, sizeof(creport));
        cstatus = vf2_sound_board_dispatch_next(&board, &creport);
        bus.ram = step_ram;
        bus.ram_size = sizeof(step_ram);
        bus.rom = audio_rom;
        bus.rom_size = audio_rom_size;
        bus.sample_rom = NULL;
        bus.sample_rom_size = 0u;
        bus.scsp = &step_scsp;
        bus.event_count = 0u;
        vf2_m68k_regs_reset(&regs);
        regs.a[4] = UINT32_C(0x2506);
        regs.a[5] = UINT32_C(0x100000);
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x7f00);
        regs.pc = UINT32_C(0x7a6);
        status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c),
                              0, 20000u, &done);
        fprintf(stderr, "dispatch a0-max: round %d C=%d handler=0x%05x handled=%d count=%u S=%d pc=0x%05x steps=%u events=%u\n",
                round, (int)cstatus,
                (unsigned)creport.handler_address,
                creport.handled,
                (unsigned)cram[VF2_SOUND_COMMAND_COUNT],
                (int)status, (unsigned)regs.pc,
                (unsigned)done, (unsigned)bus.event_count);
        if (round == 0) {
            uint32_t addr = 0u;
            EXPECT_TRUE(cstatus == VF2_OK);
            EXPECT_TRUE(creport.handled == 1);
            EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 0u);
            EXPECT_TRUE(status == VF2_OK);
            EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
            EXPECT_TRUE(done == 130u);
            for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
                if (addr == 0x2506u ||
                    (addr >= 0x2508u && addr < 0x250au) ||
                    addr >= 0x7e00u) {
                    continue;
                }
                if (cram[addr] != step_ram[addr]) {
                    fprintf(stderr, "dispatch a0-max: round 0 RAM mismatch @0x%05x: C=%02x S=%02x\n",
                            (unsigned)addr, (unsigned)cram[addr],
                            (unsigned)step_ram[addr]);
                    EXPECT_TRUE(0);
                    break;
                }
            }
            EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
        } else {
            EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
            EXPECT_TRUE(creport.handler_address ==
                        UINT32_C(0x1e98));
            EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
            EXPECT_TRUE(creport.cursor_after ==
                        creport.cursor_before);
            EXPECT_TRUE(status == VF2_OK);
            EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
            EXPECT_TRUE(done == 210u);
        }
    }
}

/* S3c: synthetic-entry 0xa8c differentials (drain-unreachable
 * bodies). Entry: pc 0xa8c, a0 = descriptor, a1 = voice, a5/a6/a7
 * standard; every d-reg and a2/a3 seeded with garbage
 * (residue-independence: each is written before read on both
 * branches -- a leak fails C==S). Run to the 0xc68 door. C:
 * vf2_sound_board_setup_author(board, 0, 0). Compared: full
 * voice0 record, $151e/$152e, SCSP equal + lanes, lookup_word vs
 * live d2, zero events.
 * - "low": desc[2] = 0x03, $150e ch 3. +3/8/4/9/10 filed (no +2
 *   bsets, no +5/$152e traffic); volume corner voice8 = 0xff x
 *   desc10 = 0xff -> 0x00 absolute; 0x1062 picker; 0x12 word
 *   with BOTH channel rows (0x4220<<5&e0 | 0x4240&7); 0x15 =
 *   desc[12]; 0x16 = ($1511&0x1f)|0x60.
 * - "high-f": HIGH desc, $150e ch 0xf (the 0xaf0 path): +9 =
 *   $151e ($151e++), +2 = 0x50 (no 0x08 bset), +5 preserved. */
static void test_a8c_synth(
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    int low,
    const char *tag
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint8_t *voice0 = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;
    size_t i = 0u;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);

    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = low ? 0x03u : 0x0fu;
    desc[2] = low ? 0x03u : 0xf5u;
    desc[4] = 0x00u;
    desc[5] = 0x04u;
    desc[6] = 0x00u;
    desc[8] = 0x02u;
    desc[9] = 0x01u;
    desc[10] = low ? 0xffu : 0x80u;
    desc[12] = 0x0bu;
    cram[VF2_SOUND_LOOKUP_CURRENT] = 0x00u;
    cram[VF2_SOUND_LOOKUP_MODE] = low ? 0xffu : 0x09u;
    cram[VF2_SOUND_LOOKUP_CONTROL] = 0x05u;
    cram[VF2_SOUND_LOOKUP_WORD_ALT] = 0xe0u;
    cram[VF2_SOUND_LOOKUP_WORD_ALT + 1u] = 0x03u;
    cram[VF2_SOUND_LOOKUP_WORD] = 0x00u;
    cram[VF2_SOUND_LOOKUP_WORD + 1u] = 0x40u;
    cram[VF2_SOUND_LOOKUP_BYTE] = 0x41u;
    cram[VF2_SOUND_LOOKUP_RELEASE] = 0x09u;
    cram[VF2_SOUND_CURRENT_COMMAND] = low ? 0x93u : 0x9fu;
    cram[VF2_SOUND_SHARED_COUNTER_151E] = 0x20u;
    cram[VF2_SOUND_SHARED_COUNTER_152E] = 0x07u;
    cram[0x2518u] = 0x07u;
    cram[0x4206u] = 0x12u;
    cram[0x4207u] = 0x34u;
    cram[0x4223u] = 0x07u;
    cram[0x4243u] = 0x05u;
    voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[0] = 0x00u;
    voice0[1] = 0x20u;
    voice0[5] = 0x77u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_setup_author(&board, 0u, 0u, &creport);
    fprintf(stderr, "dispatch a8c-syn-%s: c-status=%d handler=0x%05x d2=%04x v0=%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x 1e=%02x 2e=%02x\n",
            tag, (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)creport.lookup_word,
            (unsigned)cram[0x2802u], (unsigned)cram[0x2803u],
            (unsigned)cram[0x2804u], (unsigned)cram[0x2805u],
            (unsigned)cram[0x2806u], (unsigned)cram[0x2808u],
            (unsigned)cram[0x2809u], (unsigned)cram[0x280au],
            (unsigned)cram[VF2_SOUND_SHARED_COUNTER_151E],
            (unsigned)cram[VF2_SOUND_SHARED_COUNTER_152E]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xcb2));

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    for (i = 0u; i < 8u; ++i) {
        regs.d[i] = UINT32_C(0xa5000000) + (uint32_t)(i * 0x1111111u);
    }
    regs.a[0] = VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    regs.a[1] = VF2_SOUND_VOICE_TABLE_BASE;
    regs.a[2] = UINT32_C(0xdead0002);
    regs.a[3] = UINT32_C(0xdead0003);
    regs.a[4] = UINT32_C(0xdead0004);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0xa8c);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xcb2), 0, 20000u,
                          &done);
    fprintf(stderr, "dispatch a8c-syn-%s: s-status=%d pc=0x%05x steps=%u events=%u d2=%04x v0=%02x/%02x/%02x/%02x/%02x/%02x/%02x/%02x 1e=%02x 2e=%02x\n",
            tag, (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count, (unsigned)regs.d[2],
            (unsigned)step_ram[0x2802u], (unsigned)step_ram[0x2803u],
            (unsigned)step_ram[0x2804u], (unsigned)step_ram[0x2805u],
            (unsigned)step_ram[0x2806u], (unsigned)step_ram[0x2808u],
            (unsigned)step_ram[0x2809u], (unsigned)step_ram[0x280au],
            (unsigned)step_ram[VF2_SOUND_SHARED_COUNTER_151E],
            (unsigned)step_ram[VF2_SOUND_SHARED_COUNTER_152E]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xcb2));
    EXPECT_TRUE(done == (low ? 112u : 103u));
    EXPECT_TRUE(bus.event_count == 0u);
    for (i = 0u; i < VF2_SOUND_VOICE_BYTES; ++i) {
        if (cram[VF2_SOUND_VOICE_TABLE_BASE + i] !=
            step_ram[VF2_SOUND_VOICE_TABLE_BASE + i]) {
            fprintf(stderr, "dispatch a8c-syn-%s: voice0[%u] differ: C=%02x S=%02x\n",
                    tag, (unsigned)i,
                    (unsigned)cram[VF2_SOUND_VOICE_TABLE_BASE + i],
                    (unsigned)step_ram[VF2_SOUND_VOICE_TABLE_BASE + i]);
            EXPECT_TRUE(0);
        }
    }
    EXPECT_TRUE(cram[VF2_SOUND_SHARED_COUNTER_151E] ==
                step_ram[VF2_SOUND_SHARED_COUNTER_151E]);
    EXPECT_TRUE(cram[VF2_SOUND_SHARED_COUNTER_152E] ==
                step_ram[VF2_SOUND_SHARED_COUNTER_152E]);
    /* S d2 at 0xcb2 is the rate word plus the engine's addi.w
     * #$eba (wrap exact); the report holds the pre-add rate. */
    EXPECT_TRUE(creport.lookup_word ==
                (uint16_t)(regs.d[2] - UINT32_C(0xeba)));
    if (low) {
        /* Volume corner (absolute): 0xff*0xff = 0xfe01 >> 8 =
           0xfe ~ 0x01 >> 1 = 0x00. */
        uint8_t level = 0u;
        EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, 0x2du, &level) ==
                    VF2_OK);
        EXPECT_TRUE(level == 0u);
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S4: production-shape chain differential (ROM-driven control).
 *
 * Board: real init (0x80000 audio ROM + 3MB zero sample window --
 * zeros read fine; timbre pending), real boot descriptors
 * UNTOUCHED, one voice minimally staged for arming (+9 = 0xff;
 * slot/words from init), $152e = 5. Packet: ch = first channel
 * whose lowest matching descriptor is HIGH-capable (byte-match
 * + desc[2] >= 0xf0, scanned from the real images); payload =
 * first 0..31 passing the 0x8a6 gate (trial on memcpy'd boards
 * -- the table is real). Stepper: identical RAM image from the
 * drain 0x7a6 to the 0xcb2 copy loop. Compared: FULL 512KB
 * RAM except the drain-epilogue cursor (2B, C advanced / S
 * hasn't) and the stack residue window [0x7e00,0x8000) (C never
 * touches the stack; S sits mid-call) + full SCSP + zero bus
 * events. The $5000 sample table and $1516/rate bytes are REAL
 * upload/table data here (not zeros) -- this leg proves the
 * composed chain on production bytes. */
static void test_chain_production(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    static uint8_t sample_window[0x300000u];
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *voice0 = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;

    memset(&board, 0, sizeof(board));
    memset(sample_window, 0, sizeof(sample_window));
    memset(step_ram, 0, sizeof(step_ram));
    if (vf2_sound_board_initialize(&board, audio_rom, audio_rom_size,
                                   sample_window,
                                   sizeof(sample_window)) != VF2_OK) {
        fprintf(stderr, "chain: real init failed\n");
        EXPECT_TRUE(0);
        return;
    }
    cram = board.sound_ram;
    /* Pinned production route (one-off survey found 64 door-hits
       across real descs 10,11,13-18; desc10/ch10/payload0 is the
       first): real boot images, real tables, real $5000 upload.
       Packet {0x9a, 0, 1}. */
    {
        uint8_t packet[4];
        packet[0] = 0x9au;
        packet[1] = 0u;
        packet[2] = 1u;
        packet[3] = 0u;
        EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) ==
                    VF2_OK);
    }
    /* Minimal arming stage: voice0 +9 (slot/words from init). */
    voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[9] = 0xffu;
    voice0[5] = 0x00u;
    cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));

    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "chain: c-status=%d handler=0x%05x count=%u\n",
            (int)cstatus, (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xcb2));
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = sample_window;
    bus.sample_rom_size = sizeof(sample_window);
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    step_ram[VF2_SOUND_COMMAND_COUNT] = 1u;
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xcb2), 0, 20000u,
                          &done);
    fprintf(stderr, "chain: s-status=%d pc=0x%05x steps=%u events=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xcb2));
    EXPECT_TRUE(done == 383u);
    EXPECT_TRUE(bus.event_count == 0u);
    {
        /* Full-RAM sweep minus drain-epilogue cursor + stack. */
        uint32_t addr = 0u;
        for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
            /* Excluded by construction: count (C holds the refused
               packet, the drain subq'd at entry), drain-epilogue
               cursor (C advanced, S hasn't run the epilogue), stack
               residue (C is stackless, S sits mid-call). */
            if (addr == 0x2506u ||
                (addr >= 0x2508u && addr < 0x250au) ||
                addr >= 0x7e00u) {
                continue;
            }
            if (cram[addr] != step_ram[addr]) {
                fprintf(stderr, "chain: RAM mismatch @0x%05x: C=%02x S=%02x\n",
                        (unsigned)addr, (unsigned)cram[addr],
                        (unsigned)step_ram[addr]);
                EXPECT_TRUE(0);
                break;
            }
        }
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
}

/* S4: chain render determinism + voice-count (C-only).
 * Same production chain as test_chain_production (real init,
 * packet 0x9a, staged arming): exactly one voice authored
 * (+2 = 0x58 absolute: 0x40|0x10|0x08 HIGH ch!=0xf), $152e
 * back to 5. Render 512 frames twice from snapshot-restored
 * state: identical buffers (determinism gate). The zero sample
 * window renders silence -- expected, not asserted; S4b owns
 * audibility with real banks. */
static void test_chain_render(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static vf2_sound_board board;
    static vf2_sound_board snapshot;
    static uint8_t sample_window[0x300000u];
    static int16_t left_a[512u];
    static int16_t right_a[512u];
    static int16_t left_b[512u];
    static int16_t right_b[512u];
    vf2_sound_dispatch_report report;
    uint8_t *ram = NULL;
    uint8_t *voice0 = NULL;
    size_t i = 0u;
    size_t voiced = 0u;

    memset(&board, 0, sizeof(board));
    memset(sample_window, 0, sizeof(sample_window));
    EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom,
                                           audio_rom_size,
                                           sample_window,
                                           sizeof(sample_window)) ==
                VF2_OK);
    ram = board.sound_ram;
    voice0 = ram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[9] = 0xffu;
    voice0[5] = 0x00u;
    ram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    {
        const uint8_t packet[4] = {0x9au, 0u, 1u, 0u};
        EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) ==
                    VF2_OK);
    }
    ram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    ram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    memset(&report, 0, sizeof(report));
    EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &report) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(report.handler_address == UINT32_C(0xcb2));
    for (i = 0u; i < VF2_SOUND_VOICE_COUNT; ++i) {
        if (ram[VF2_SOUND_VOICE_TABLE_BASE +
                i * VF2_SOUND_VOICE_BYTES + 2u] != 0u) {
            ++voiced;
        }
    }
    fprintf(stderr, "chain-render: voiced=%u v0+2=%02x 2e=%02x\n",
            (unsigned)voiced, (unsigned)voice0[2],
            (unsigned)ram[VF2_SOUND_SHARED_COUNTER_152E]);
    EXPECT_TRUE(voiced == 1u);
    EXPECT_TRUE(voice0[2] == 0x58u);
    EXPECT_TRUE(ram[VF2_SOUND_SHARED_COUNTER_152E] == 5u);
    memcpy(&snapshot, &board, sizeof(snapshot));
    EXPECT_TRUE(vf2_sound_board_render(&board, left_a, right_a, 512u) ==
                VF2_OK);
    memcpy(&board, &snapshot, sizeof(board));
    EXPECT_TRUE(vf2_sound_board_render(&board, left_b, right_b, 512u) ==
                VF2_OK);
    EXPECT_TRUE(memcmp(left_a, left_b, sizeof(left_a)) == 0);
    EXPECT_TRUE(memcmp(right_a, right_b, sizeof(right_b)) == 0);
    fprintf(stderr, "chain-render: determinism holds (frame0 L=%d R=%d)\n",
            (int)left_a[0], (int)right_a[0]);
}

/* S4b: audible chain render (experiment, NOT recovery).
 *
 * Same production chain (real init, packet 0x9a, staged arming)
 * but the sample window is REAL bank bytes (mpr-17570..73,
 * present in this checkout, concatenated in numeric order) and
 * the authored slot is keyed afterwards: OR 0x18 into the slot
 * control byte (ROM 0xefe disassembly, exact) + OR 0x0800
 * KEYONB into the control word (model key gate;
 * sound_board_key_off/V1 and V3c key the same way). The keying
 * MECHANISM stays experiment-owned (E1b programs the slot words
 * natively now, but this leg keeps the manual keying) -- this leg
 * proves the authored CONFIG renders (mixing/envelopes/
 * audibility), not that production keys it. Model gap noted:
 * synchronize_slot zeroes the play address on key-on, so the
 * render starts at window offset 0 regardless of the programmed
 * SAHI/SALO; real-bank order likewise only conditions timbre.
 * Gates: non-silence (peak), determinism (snapshot-restored
 * re-render identical), WAV bonus artifact to out/ (gitignored).
 * Prints: slot/sample address/rate, first sample bytes, slot
 * control word, peak/RMS. */
static void test_chain_audible(
    const char *rom_dir,
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static vf2_sound_board board;
    static vf2_sound_board snapshot;
    static int16_t left[2048u];
    static int16_t right[2048u];
    static int16_t left_b[2048u];
    static int16_t right_b[2048u];
    uint8_t *bank = NULL;
    vf2_sound_dispatch_report report;
    uint8_t *ram = NULL;
    uint8_t *voice0 = NULL;
    uint16_t slot = 0u;
    uint16_t control = 0u;
    uint8_t ctl = 0u;
    size_t i = 0u;
    int32_t peak = 0;
    int64_t sum_sq = 0;

    {
        static const char *names[4] = {
            "mpr-17570.37", "mpr-17571.36",
            "mpr-17572.32", "mpr-17573.31"
        };
        bank = (uint8_t *)malloc(0x800000u);
        EXPECT_TRUE(bank != NULL);
        if (bank == NULL) {
            return;
        }
        memset(bank, 0, 0x800000u);
        for (i = 0u; i < 4u; ++i) {
            char path[512];
            FILE *f = NULL;
            size_t got = 0u;
            snprintf(path, sizeof(path), "%s/%s", rom_dir, names[i]);
            f = fopen(path, "rb");
            if (f == NULL) {
                fprintf(stderr, "chain-audible: missing %s\n", path);
                EXPECT_TRUE(0);
                free(bank);
                return;
            }
            got = fread(bank + i * 0x200000u, 1u, 0x200000u, f);
            fclose(f);
            fprintf(stderr, "chain-audible: %s %u bytes\n", names[i],
                    (unsigned)got);
            EXPECT_TRUE(got == 0x200000u);
        }
    }
    memset(&board, 0, sizeof(board));
    EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom,
                                           audio_rom_size, bank,
                                           0x800000u) == VF2_OK);
    ram = board.sound_ram;
    voice0 = ram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[9] = 0xffu;
    voice0[5] = 0x00u;
    ram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    {
        const uint8_t packet[4] = {0x9au, 0u, 1u, 0u};
        EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) ==
                    VF2_OK);
    }
    ram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    ram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    memset(&report, 0, sizeof(report));
    EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &report) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(report.handler_address == UINT32_C(0xcb2));
    slot = (uint16_t)(((uint16_t)voice0[0] << 8u) | voice0[1]);
    EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, slot, &control) ==
                VF2_OK);
    fprintf(stderr, "chain-audible: slot=0x%04x ctl=%04x rate-voice=%02x bank0=%02x%02x%02x%02x\n",
            (unsigned)slot, (unsigned)control,
            (unsigned)voice0[0x0bu], (unsigned)bank[0],
            (unsigned)bank[1], (unsigned)bank[2], (unsigned)bank[3]);
    /* Engine key bits (experiment): 0xefe OR-0x18, then KEYONB. */
    EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, slot, &ctl) == VF2_OK);
    ctl |= 0x18u;
    EXPECT_TRUE(vf2_scsp_write_u8(&board.scsp, slot, ctl) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, slot, &control) ==
                VF2_OK);
    control |= 0x0800u;
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, slot, control) ==
                VF2_OK);
    fprintf(stderr, "chain-audible: keyed ctl=%04x active=%u\n",
            (unsigned)control,
            (unsigned)board.scsp.slot_active[slot]);
    EXPECT_TRUE(board.scsp.slot_active[slot] == 1u);
    memcpy(&snapshot, &board, sizeof(snapshot));
    EXPECT_TRUE(vf2_sound_board_render(&board, left, right, 2048u) ==
                VF2_OK);
    for (i = 0u; i < 2048u; ++i) {
        int32_t al = left[i] < 0 ? -(int32_t)left[i] : left[i];
        int32_t ar = right[i] < 0 ? -(int32_t)right[i] : right[i];
        if (al > peak) {
            peak = al;
        }
        if (ar > peak) {
            peak = ar;
        }
        sum_sq += (int64_t)left[i] * left[i] + (int64_t)right[i] * right[i];
    }
    fprintf(stderr, "chain-audible: peak=%d meansq=%lld\n", (int)peak,
            (long long)(sum_sq / 4096));
    EXPECT_TRUE(peak > 1000);
    memcpy(&board, &snapshot, sizeof(board));
    EXPECT_TRUE(vf2_sound_board_render(&board, left_b, right_b,
                                       2048u) == VF2_OK);
    EXPECT_TRUE(memcmp(left, left_b, sizeof(left)) == 0);
    EXPECT_TRUE(memcmp(right, right_b, sizeof(right)) == 0);
    {
        /* 44-byte PCM16 stereo WAV, best-effort (out/ gitignored).
           2048 frames x 4B = 8192 data bytes. */
        static const uint8_t hdr[44] = {
            0x52u, 0x49u, 0x46u, 0x46u, 0x24u, 0x20u, 0x00u, 0x00u,
            0x57u, 0x41u, 0x56u, 0x45u, 0x66u, 0x6du, 0x74u, 0x20u,
            0x10u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x02u, 0x00u,
            0x44u, 0xacu, 0x00u, 0x00u, 0x10u, 0xb1u, 0x02u, 0x00u,
            0x04u, 0x00u, 0x10u, 0x00u, 0x64u, 0x61u, 0x74u, 0x61u,
            0x00u, 0x20u, 0x00u, 0x00u
        };
        FILE *wav = fopen("out/chain-voice.wav", "wb");
        if (wav != NULL) {
            uint8_t frame[4];
            size_t f = 0u;
            (void)fwrite(hdr, 1u, sizeof(hdr), wav);
            for (f = 0u; f < 2048u; ++f) {
                frame[0] = (uint8_t)left[f];
                frame[1] = (uint8_t)((uint16_t)left[f] >> 8u);
                frame[2] = (uint8_t)right[f];
                frame[3] = (uint8_t)((uint16_t)right[f] >> 8u);
                (void)fwrite(frame, 1u, sizeof(frame), wav);
            }
            (void)fclose(wav);
            fprintf(stderr, "chain-audible: wrote out/chain-voice.wav\n");
        } else {
            fprintf(stderr, "chain-audible: out/ missing, WAV skipped\n");
        }
    }
    free(bank);
}

/* 8b: session-attach leg (real banks via the session's own path).
 *
 * Builds the real AUDIOCPU + SAMPLES regions (bank order 73/72/
 * 71/70 per rom_manifest, NOT numeric -- S4b's test window had
 * it backwards; only timbre was at stake there) and attaches via
 * game_initialize + game_attach_audio, the exact calls
 * vf2_session_open makes. Gates: attach VF2_OK (kills the stale
 * "boot refuses the real audio ROM" claim), one video frame
 * (735) renders OK twice with bit-identical buffers. Silence is
 * EXPECTED and reported, not gated: no in-session producer keys
 * voices (see the 8a hunt note), so init state renders zeros. */
static void test_session_attach(const char *rom_dir)
{
    static vf2_game game;
    static int16_t left_a[735u];
    static int16_t right_a[735u];
    static int16_t left_b[735u];
    static int16_t right_b[735u];
    uint8_t *audio_rom = NULL;
    uint8_t *sample_rom = NULL;
    size_t audio_size = 0u;
    size_t sample_size = 0u;
    size_t i = 0u;
    int32_t peak = 0;

    EXPECT_TRUE(vf2_romset_build_region(rom_dir, VF2_REGION_AUDIOCPU,
                                       &audio_rom, &audio_size) == VF2_OK);
    EXPECT_TRUE(vf2_romset_build_region(rom_dir, VF2_REGION_SAMPLES,
                                       &sample_rom, &sample_size) == VF2_OK);
    if (audio_rom == NULL || sample_rom == NULL) {
        fprintf(stderr, "session-attach: region build failed\n");
        EXPECT_TRUE(0);
        free(audio_rom);
        free(sample_rom);
        return;
    }
    fprintf(stderr, "session-attach: audio 0x%x samples 0x%x\n",
            (unsigned)audio_size, (unsigned)sample_size);
    EXPECT_TRUE(audio_size == 0x80000u);
    EXPECT_TRUE(sample_size == 0x800000u);
    memset(&game, 0, sizeof(game));
    EXPECT_TRUE(vf2_game_initialize(&game) == VF2_OK);
    EXPECT_TRUE(vf2_game_attach_audio(&game, audio_rom, audio_size,
                                     sample_rom, sample_size) == VF2_OK);
    EXPECT_TRUE(vf2_game_render_audio(&game, left_a, right_a, 735u) ==
                VF2_OK);
    EXPECT_TRUE(vf2_game_render_audio(&game, left_b, right_b, 735u) ==
                VF2_OK);
    EXPECT_TRUE(memcmp(left_a, left_b, sizeof(left_a)) == 0);
    EXPECT_TRUE(memcmp(right_a, right_b, sizeof(right_a)) == 0);
    for (i = 0u; i < 735u; ++i) {
        int32_t al = left_a[i] < 0 ? -(int32_t)left_a[i] : left_a[i];
        int32_t ar = right_a[i] < 0 ? -(int32_t)right_a[i] : right_a[i];
        if (al > peak) {
            peak = al;
        }
        if (ar > peak) {
            peak = ar;
        }
    }
    fprintf(stderr, "session-attach: peak=%d (0 expected, no producer)\n",
            (int)peak);
    vf2_game_shutdown(&game);
    free(audio_rom);
    free(sample_rom);
}

/* 8c-hunt: receiver vector pins (read-only ROM asserts).
 *
 * The comm hunt maps the sound-side receiver to the L3 ISR at
 * 0x526 (UART byte at $405(a5) -> $1500 ring file, 0xff =
 * sound-CPU reset to 0x100) and the stream tick to the L2 ISR
 * at 0x1f16. This leg pins the vector-table words that route
 * there (BE32 in the audio region): reset@0x04, L1@0x64,
 * L2@0x68, L3@0x6c. If a future ROM set moves the receiver,
 * this fails first, before any bridge code trusts it. */
static void test_vectors(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static const uint8_t want_reset[4] = {0x00u, 0x60u, 0x01u, 0x00u};
    static const uint8_t want_l1[4] = {0x00u, 0x60u, 0x05u, 0xfeu};
    static const uint8_t want_l2[4] = {0x00u, 0x60u, 0x1fu, 0x16u};
    static const uint8_t want_l3[4] = {0x00u, 0x60u, 0x05u, 0x26u};

    EXPECT_TRUE(audio_rom_size >= 0x70u);
    if (audio_rom_size < 0x70u) {
        return;
    }
    EXPECT_TRUE(memcmp(audio_rom + 0x04u, want_reset, 4u) == 0);
    EXPECT_TRUE(memcmp(audio_rom + 0x64u, want_l1, 4u) == 0);
    EXPECT_TRUE(memcmp(audio_rom + 0x68u, want_l2, 4u) == 0);
    EXPECT_TRUE(memcmp(audio_rom + 0x6cu, want_l3, 4u) == 0);
    fprintf(stderr, "vectors: reset=060100 L1=0605fe L2=061f16 L3=060526\n");
}

/* 8g L3 receiver legs (byte -> ring file). Staging: file1503 /
 * file1502 (the $1503/$1502 age bytes), dance1526, src1500/src1501
 * (ring-copy sources), cursor (12-bit). The stepper starts past
 * the $0405(a5) hardware read (S3c pattern) with d0 = byte, so
 * the restore pops fresh-stack zeros: post-run d0-d7/a0-a4 are 0,
 * a7 advances to 0x7f34, and no stack residue exists (full-RAM
 * compare, no exclusion). S event count pins the $400001 poke
 * (1 on ring legs, 0 elsewhere -- the poke is logged,
 * stateless). */
static void test_l3_file(
    const uint8_t *audio_rom,
    size_t audio_rom_size,
    uint8_t byte,
    uint8_t file1503,
    uint8_t file1502,
    uint8_t dance1526,
    uint8_t src1500,
    uint8_t src1501,
    uint16_t cursor,
    const char *tag
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    uint8_t *cram = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;
    size_t i = 0u;
    uint32_t stop = (byte == 0xffu) ? UINT32_C(0x5ec) : UINT32_C(0x5ea);

    (void)audio_rom;
    (void)audio_rom_size;
    memset(&board, 0, sizeof(board));
    board.audio_rom = NULL;
    board.audio_rom_size = 0u;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);
    cram[VF2_SOUND_RING_STAGE_BASE] = src1500;
    cram[VF2_SOUND_RING_STAGE_BASE + 1u] = src1501;
    cram[VF2_SOUND_RING_STAGE_BASE + 2u] = file1502;
    cram[VF2_SOUND_RING_STAGE_BASE + 3u] = file1503;
    cram[VF2_SOUND_COMMAND_WRITE_CURSOR] = (uint8_t)(cursor >> 8u);
    cram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u] = (uint8_t)cursor;
    cram[VF2_SOUND_SHARED_COUNTER_1526] = dance1526;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);

    cstatus = vf2_sound_board_file_uart_byte(&board, byte);
    fprintf(stderr, "dispatch l3-%s: c-status=%d f2=%02x f3=%02x c=%04x n=%02x 26=%02x\n",
            tag, (int)cstatus,
            (unsigned)cram[VF2_SOUND_RING_STAGE_BASE + 2u],
            (unsigned)cram[VF2_SOUND_RING_STAGE_BASE + 3u],
            (unsigned)(((uint16_t)cram[VF2_SOUND_COMMAND_WRITE_CURSOR] << 8u) |
                       cram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]),
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)cram[VF2_SOUND_SHARED_COUNTER_1526]);
    if (byte == 0xffu) {
        EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
    } else {
        EXPECT_TRUE(cstatus == VF2_OK);
    }

    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    for (i = 0u; i < 8u; ++i) {
        regs.d[i] = UINT32_C(0xa5000000) + (uint32_t)(i * 0x1111111u);
    }
    regs.d[0] = byte;
    regs.a[0] = UINT32_C(0xdead0000);
    regs.a[1] = UINT32_C(0xdead0001);
    regs.a[2] = UINT32_C(0xdead0002);
    regs.a[3] = UINT32_C(0xdead0003);
    regs.a[4] = UINT32_C(0xdead0004);
    regs.a[5] = UINT32_C(0xdead0005);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x52e);
    status = vf2_m68k_run(&bus, &regs, stop, 0, 20000u, &done);
    fprintf(stderr, "dispatch l3-%s: s-status=%d pc=0x%05x steps=%u events=%u f2=%02x f3=%02x c=%04x n=%02x 26=%02x\n",
            tag, (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_ram[VF2_SOUND_RING_STAGE_BASE + 2u],
            (unsigned)step_ram[VF2_SOUND_RING_STAGE_BASE + 3u],
            (unsigned)(((uint16_t)step_ram[VF2_SOUND_COMMAND_WRITE_CURSOR] << 8u) |
                       step_ram[VF2_SOUND_COMMAND_WRITE_CURSOR + 1u]),
            (unsigned)step_ram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)step_ram[VF2_SOUND_SHARED_COUNTER_1526]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == stop);
    {
        uint32_t addr = 0u;
        for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
            /* jsr $49e return-address spill (ring legs, S only):
             * 4 bytes at a7 - 4. Nothing else touches the stack
             * (the 0x526 save is skipped by the 0x52e entry). */
            if (addr >= 0x7efcu && addr < 0x7f00u) {
                continue;
            }
            if (cram[addr] != step_ram[addr]) {
                fprintf(stderr, "dispatch l3-%s: RAM mismatch @0x%05x: C=%02x S=%02x\n",
                        tag, (unsigned)addr, (unsigned)cram[addr],
                        (unsigned)step_ram[addr]);
                EXPECT_TRUE(0);
                break;
            }
        }
    }
    /* Post-restore zeros (save skipped by the 0x52e entry): d0-d7
     * and a0-a4 read back 0 -- except the 0xff door leg, which
     * stops at 0x5ec before the restore and keeps staged regs. */
    if (byte == 0xffu) {
        EXPECT_TRUE(regs.d[0] == UINT32_C(0xff));
        for (i = 1u; i < 8u; ++i) {
            uint32_t want =
                UINT32_C(0xa5000000) + (uint32_t)(i * 0x1111111u);
            if (regs.d[i] != want) {
                fprintf(stderr, "dispatch l3-%s: d%u differ: S=%08x want=%08x\n",
                        tag, (unsigned)i, (unsigned)regs.d[i],
                        (unsigned)want);
                EXPECT_TRUE(0);
            }
        }
        for (i = 0u; i < 5u; ++i) {
            uint32_t want = UINT32_C(0xdead0000) + (uint32_t)i;
            if (regs.a[i] != want) {
                fprintf(stderr, "dispatch l3-%s: a%u differ: S=%08x want=%08x\n",
                        tag, (unsigned)i, (unsigned)regs.a[i],
                        (unsigned)want);
                EXPECT_TRUE(0);
            }
        }
    } else {
        for (i = 0u; i < 8u; ++i) {
            if (regs.d[i] != 0u) {
                fprintf(stderr, "dispatch l3-%s: d%u nonzero: S=%08x\n",
                        tag, (unsigned)i, (unsigned)regs.d[i]);
                EXPECT_TRUE(0);
            }
        }
        for (i = 0u; i < 5u; ++i) {
            if (regs.a[i] != 0u) {
                fprintf(stderr, "dispatch l3-%s: a%u nonzero: S=%08x\n",
                        tag, (unsigned)i, (unsigned)regs.a[i]);
                EXPECT_TRUE(0);
            }
        }
    }
    EXPECT_TRUE(regs.a[5] == UINT32_C(0xdead0005));
    EXPECT_TRUE(regs.a[6] == UINT32_C(0x1000));
    /* a7 advances 52 through the restore -- except the 0xff door
     * leg, which stops at 0x5ec before any stack traffic. */
    if (byte == 0xffu) {
        EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));
    } else {
        EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f34));
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
    /* The $400001 poke is logged, stateless: exactly one bus event
     * on ring legs (C skips it by design), zero elsewhere. */
    if (cram[VF2_SOUND_COMMAND_COUNT] != 0u) {
        EXPECT_TRUE(bus.event_count == 1u);
        EXPECT_TRUE(bus.events[0].address == UINT32_C(0x400001));
        EXPECT_TRUE(bus.events[0].is_write != 0u);
    } else {
        EXPECT_TRUE(bus.event_count == 0u);
    }
}

/* 8h synthetic chain bridge (queue -> bytes -> ring -> voices -> WAV).
 *
 * Every link is stepper-proven C with its own green differential;
 * this leg proves the COMPOSITION, end to end, with no
 * emit_command shortcut:
 *
 *   filer 0x43888 (v0746, gate-A-open path) files g0 = 0x009a0001
 *     into the i960 queue (count 0 -> 1, ring[0] staged);
 *   drainer 0x43800 (8f) runs 3x (bidx ff -> 2 -> 1 -> 0 -> ff),
 *     emitting staged[2],[1],[0] = 0x9a,0x00,0x01 through the
 *     0x01c80000 UART slot (count 1 -> 0);
 *   L3 file_uart_byte (8g) files the 3 bytes in emission order,
 *     assembling ring {0x9a,0x00,0x01,0x00} -- exactly the S4
 *     production packet (4th byte is untouched zero);
 *   dispatch_next (S4) consumes it to the 0xcb2 copy loop;
 *   keyed render (S4b, real banks) is non-silent + deterministic.
 *
 * Gates: emission order pins {9a,00,01}; ring pins the S4 packet;
 * voice-count == 1 (+2 == 0x58); re-render bit-identical;
 * peak > 1000. WAV bonus artifact to out/chain-uart.wav
 * (gitignored). Prints: queue transitions, bytes, ring, voice,
 * peak/meansq. */
static void test_chain_uart_bridge(
    const char *rom_dir,
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static vf2_sound_board board;
    static vf2_sound_board snapshot;
    static int16_t left[2048u];
    static int16_t right[2048u];
    static int16_t left_b[2048u];
    static int16_t right_b[2048u];
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    vf2_sound_dispatch_report report;
    uint8_t *bank = NULL;
    uint8_t *ram = NULL;
    uint8_t *voice0 = NULL;
    uint8_t emitted[3] = {0u, 0u, 0u};
    uint8_t byte = 0u;
    uint8_t count = 0u;
    uint8_t wcur = 0u;
    uint8_t staged_le[4] = {0u, 0u, 0u, 0u};
    uint32_t filed = 0u;
    uint16_t slot = 0u;
    uint16_t control = 0u;
    uint8_t ctl = 0u;
    size_t i = 0u;
    int call = 0;
    int32_t peak = 0;
    int64_t sum_sq = 0;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    EXPECT_TRUE(vf2_model2a_initialize(&machine) != 0);
    if (machine.work_ram == NULL) {
        fprintf(stderr, "chain-bridge: no work ram\n");
        EXPECT_TRUE(0);
        vf2_model2a_shutdown(&machine);
        return;
    }
    /* Gate A open (0x50002c == 0 from init); queue empty;
     * bidx 0xff parks the drainer on the entry-load path. */
    byte = 0xffu;
    EXPECT_TRUE(vf2_model2a_write(&machine, UINT32_C(0x00504014),
                                 &byte, sizeof(byte)) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00043888));
    cpu.registers[VF2_I960_G0_REGISTER + 0u] = UINT32_C(0x009a0001);
    EXPECT_TRUE(vf2_i960_cpu_enter_procedure(&cpu,
                                            UINT32_C(0x00043888),
                                            UINT32_C(0x0004394c)) ==
                VF2_OK);
    EXPECT_TRUE(vf2_hybrid_player_43888_execute(&machine, &cpu) ==
                VF2_OK);
    EXPECT_TRUE(vf2_model2a_read(&machine, UINT32_C(0x00504001),
                                 &count, sizeof(count)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read(&machine, UINT32_C(0x00504003),
                                 &wcur, sizeof(wcur)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read(&machine, UINT32_C(0x00504020),
                                 staged_le, sizeof(staged_le)) == VF2_OK);
    filed = (uint32_t)staged_le[0] |
            ((uint32_t)staged_le[1] << 8u) |
            ((uint32_t)staged_le[2] << 16u) |
            ((uint32_t)staged_le[3] << 24u);
    fprintf(stderr, "chain-bridge: filed count=%u wcur=%u ring0=0x%08x\n",
            (unsigned)count, (unsigned)wcur, (unsigned)filed);
    EXPECT_TRUE(count == 1u);
    EXPECT_TRUE(wcur == 1u);
    EXPECT_TRUE(filed == UINT32_C(0x009a0001));
    /* Three drainer ticks: ff -> 2 -> 1 -> 0 -> ff. */
    for (call = 0; call < 3; ++call) {
        vf2_i960_cpu_reset(&cpu, 0u, 0u, UINT32_C(0x00043800));
        cpu.registers[6] = UINT32_C(0x00e80004);
        EXPECT_TRUE(vf2_hybrid_sound_43800_execute(&machine, &cpu) ==
                    VF2_OK);
        EXPECT_TRUE(cpu.ip == UINT32_C(0x00043884));
        EXPECT_TRUE(vf2_model2a_read(&machine, UINT32_C(0x01c80000),
                                     &byte, sizeof(byte)) == VF2_OK);
        emitted[call] = byte;
    }
    EXPECT_TRUE(vf2_model2a_read(&machine, UINT32_C(0x00504001),
                                 &count, sizeof(count)) == VF2_OK);
    EXPECT_TRUE(vf2_model2a_read(&machine, UINT32_C(0x00504014),
                                 &byte, sizeof(byte)) == VF2_OK);
    fprintf(stderr, "chain-bridge: bytes %02x %02x %02x count=%u bidx=%02x\n",
            (unsigned)emitted[0], (unsigned)emitted[1],
            (unsigned)emitted[2], (unsigned)count, (unsigned)byte);
    EXPECT_TRUE(emitted[0] == 0x9au);
    EXPECT_TRUE(emitted[1] == 0x00u);
    EXPECT_TRUE(emitted[2] == 0x01u);
    EXPECT_TRUE(count == 0u);
    EXPECT_TRUE(byte == 0xffu);
    vf2_model2a_shutdown(&machine);

    {
        static const char *names[4] = {
            "mpr-17570.37", "mpr-17571.36",
            "mpr-17572.32", "mpr-17573.31"
        };
        bank = (uint8_t *)malloc(0x800000u);
        EXPECT_TRUE(bank != NULL);
        if (bank == NULL) {
            return;
        }
        memset(bank, 0, 0x800000u);
        for (i = 0u; i < 4u; ++i) {
            char path[512];
            FILE *f = NULL;
            size_t got = 0u;
            snprintf(path, sizeof(path), "%s/%s", rom_dir, names[i]);
            f = fopen(path, "rb");
            if (f == NULL) {
                fprintf(stderr, "chain-bridge: missing %s\n", path);
                EXPECT_TRUE(0);
                free(bank);
                return;
            }
            got = fread(bank + i * 0x200000u, 1u, 0x200000u, f);
            fclose(f);
            EXPECT_TRUE(got == 0x200000u);
        }
    }
    memset(&board, 0, sizeof(board));
    EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom,
                                          audio_rom_size, bank,
                                          0x800000u) == VF2_OK);
    ram = board.sound_ram;
    for (call = 0; call < 3; ++call) {
        EXPECT_TRUE(vf2_sound_board_file_uart_byte(&board,
                                                   emitted[call]) ==
                    VF2_OK);
    }
    fprintf(stderr, "chain-bridge: ring %02x %02x %02x %02x count=%u\n",
            (unsigned)ram[VF2_SOUND_COMMAND_RING_BASE],
            (unsigned)ram[VF2_SOUND_COMMAND_RING_BASE + 1u],
            (unsigned)ram[VF2_SOUND_COMMAND_RING_BASE + 2u],
            (unsigned)ram[VF2_SOUND_COMMAND_RING_BASE + 3u],
            (unsigned)ram[VF2_SOUND_COMMAND_COUNT]);
    EXPECT_TRUE(ram[VF2_SOUND_COMMAND_COUNT] == 1u);
    EXPECT_TRUE(ram[VF2_SOUND_COMMAND_RING_BASE] == 0x9au);
    EXPECT_TRUE(ram[VF2_SOUND_COMMAND_RING_BASE + 1u] == 0x00u);
    EXPECT_TRUE(ram[VF2_SOUND_COMMAND_RING_BASE + 2u] == 0x01u);
    EXPECT_TRUE(ram[VF2_SOUND_COMMAND_RING_BASE + 3u] == 0x00u);
    /* S4 production tail: cursor 0, staged arming, dispatch to the
     * 0xcb2 copy loop (UNSUPPORTED by design). */
    ram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    ram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    voice0 = ram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[9] = 0xffu;
    voice0[5] = 0x00u;
    ram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    memset(&report, 0, sizeof(report));
    EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &report) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(report.handler_address == UINT32_C(0xcb2));
    {
        size_t voiced = 0u;
        for (i = 0u; i < VF2_SOUND_VOICE_COUNT; ++i) {
            if (ram[VF2_SOUND_VOICE_TABLE_BASE +
                    i * VF2_SOUND_VOICE_BYTES + 2u] != 0u) {
                ++voiced;
            }
        }
        fprintf(stderr, "chain-bridge: voiced=%u v0+2=%02x\n",
                (unsigned)voiced, (unsigned)voice0[2]);
        EXPECT_TRUE(voiced == 1u);
        EXPECT_TRUE(voice0[2] == 0x58u);
    }
    /* S4b keying (experiment harness, engine-owned mechanism). */
    slot = (uint16_t)(((uint16_t)voice0[0] << 8u) | voice0[1]);
    EXPECT_TRUE(vf2_scsp_read_u8(&board.scsp, slot, &ctl) == VF2_OK);
    ctl |= 0x18u;
    EXPECT_TRUE(vf2_scsp_write_u8(&board.scsp, slot, ctl) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_read_u16(&board.scsp, slot, &control) ==
                VF2_OK);
    control |= 0x0800u;
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, slot, control) ==
                VF2_OK);
    EXPECT_TRUE(board.scsp.slot_active[slot] == 1u);
    memcpy(&snapshot, &board, sizeof(snapshot));
    EXPECT_TRUE(vf2_sound_board_render(&board, left, right, 2048u) ==
                VF2_OK);
    for (i = 0u; i < 2048u; ++i) {
        int32_t al = left[i] < 0 ? -(int32_t)left[i] : left[i];
        int32_t ar = right[i] < 0 ? -(int32_t)right[i] : right[i];
        if (al > peak) {
            peak = al;
        }
        if (ar > peak) {
            peak = ar;
        }
        sum_sq += (int64_t)left[i] * left[i] + (int64_t)right[i] * right[i];
    }
    fprintf(stderr, "chain-bridge: peak=%d meansq=%lld\n", (int)peak,
            (long long)(sum_sq / 4096));
    EXPECT_TRUE(peak > 1000);
    memcpy(&board, &snapshot, sizeof(board));
    EXPECT_TRUE(vf2_sound_board_render(&board, left_b, right_b,
                                      2048u) == VF2_OK);
    EXPECT_TRUE(memcmp(left, left_b, sizeof(left)) == 0);
    EXPECT_TRUE(memcmp(right, right_b, sizeof(right)) == 0);
    {
        static const uint8_t hdr[44] = {
            0x52u, 0x49u, 0x46u, 0x46u, 0x24u, 0x20u, 0x00u, 0x00u,
            0x57u, 0x41u, 0x56u, 0x45u, 0x66u, 0x6du, 0x74u, 0x20u,
            0x10u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x02u, 0x00u,
            0x44u, 0xacu, 0x00u, 0x00u, 0x10u, 0xb1u, 0x02u, 0x00u,
            0x04u, 0x00u, 0x10u, 0x00u, 0x64u, 0x61u, 0x74u, 0x61u,
            0x00u, 0x20u, 0x00u, 0x00u
        };
        FILE *wav = fopen("out/chain-uart.wav", "wb");
        if (wav != NULL) {
            uint8_t frame[4];
            size_t f = 0u;
            (void)fwrite(hdr, 1u, sizeof(hdr), wav);
            for (f = 0u; f < 2048u; ++f) {
                frame[0] = (uint8_t)left[f];
                frame[1] = (uint8_t)((uint16_t)left[f] >> 8u);
                frame[2] = (uint8_t)right[f];
                frame[3] = (uint8_t)((uint16_t)right[f] >> 8u);
                (void)fwrite(frame, 1u, sizeof(frame), wav);
            }
            (void)fclose(wav);
            fprintf(stderr, "chain-bridge: wrote out/chain-uart.wav\n");
        } else {
            fprintf(stderr, "chain-bridge: out/ missing, WAV skipped\n");
        }
    }
    free(bank);
}

/* T2: live-queue chain (queue -> bytes -> ring -> voices -> WAV).
 *
 * The park-sixth-a6c0 snapshot holds a LIVE-filed queue: count 3,
 * rcur 0, wcur 3, ring {0x00ae101f, 0x009e0a7f, 0x00ae101f},
 * staged 0, bidx 0xff (filed by real game code on the
 * boot->TEST-MENU trajectory; the drainer has no live callers
 * so the entries were never drained). Nine reference drainer
 * ticks (probe recipe in audio_t2_live_queue_v0825.md) emit
 * ae 10 1f | 9e 0a 7f | ae 10 1f and leave count 0, bidx 0xff.
 *
 * This leg files those 9 bytes (C file_uart_byte vs stepper L3
 * 0x52e chained x9) and dispatches all 3 ring entries (C
 * dispatch_next vs stepper drain 0x7a6 -> 0xcb2 per entry):
 * full-RAM C==S at every stage (S4 exclusions: count/cursor
 * cells, stack window, jsr-spill), handler/status equality per
 * round, render determinism + WAV to out/ (gitignored). No
 * emit_command shortcut, no voice staging ($152e untouched):
 * the ae/9e commands act on init-state voices. Prints: ring,
 * per-round handler/steps, voice delta, peak. */
static void test_chain_live_queue(
    const char *rom_dir,
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static const uint8_t live_bytes[9] = {
        0xaeu, 0x10u, 0x1fu,
        0x9eu, 0x0au, 0x7fu,
        0xaeu, 0x10u, 0x1fu
    };
    static const uint8_t want_ring[12] = {
        0xaeu, 0x10u, 0x1fu, 0x00u,
        0x9eu, 0x0au, 0x7fu, 0x00u,
        0xaeu, 0x10u, 0x1fu, 0x00u
    };
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    static vf2_sound_board snapshot;
    static vf2_sound_board filed;
    static int16_t left[2048u];
    static int16_t right[2048u];
    static int16_t left_b[2048u];
    static int16_t right_b[2048u];
    uint8_t *bank = NULL;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    size_t i = 0u;
    int round = 0;
    int32_t peak = 0;
    {
        static const char *names[4] = {
            "mpr-17570.37", "mpr-17571.36",
            "mpr-17572.32", "mpr-17573.31"
        };
        bank = (uint8_t *)malloc(0x800000u);
        EXPECT_TRUE(bank != NULL);
        if (bank == NULL) {
            return;
        }
        memset(bank, 0, 0x800000u);
        for (i = 0u; i < 4u; ++i) {
            char path[512];
            FILE *f = NULL;
            size_t got = 0u;
            snprintf(path, sizeof(path), "%s/%s", rom_dir, names[i]);
            f = fopen(path, "rb");
            if (f == NULL) {
                fprintf(stderr, "chain-live: missing %s\n", path);
                EXPECT_TRUE(0);
                free(bank);
                return;
            }
            got = fread(bank + i * 0x200000u, 1u, 0x200000u, f);
            fclose(f);
            EXPECT_TRUE(got == 0x200000u);
        }
    }
    memset(&board, 0, sizeof(board));
    EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom,
                                          audio_rom_size, bank,
                                          0x800000u) == VF2_OK);
    cram = board.sound_ram;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    /* File the 9 live bytes on both sides. */
    for (i = 0u; i < 9u; ++i) {
        EXPECT_TRUE(vf2_sound_board_file_uart_byte(&board,
                                                   live_bytes[i]) ==
                    VF2_OK);
        bus.ram = step_ram;
        bus.ram_size = sizeof(step_ram);
        bus.rom = audio_rom;
        bus.rom_size = audio_rom_size;
        bus.sample_rom = bank;
        bus.sample_rom_size = 0x800000u;
        bus.scsp = &step_scsp;
        bus.event_count = 0u;
        vf2_m68k_regs_reset(&regs);
        regs.d[0] = live_bytes[i];
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x7f00);
        regs.pc = UINT32_C(0x52e);
        status = vf2_m68k_run(&bus, &regs, UINT32_C(0x5ea), 0,
                              20000u, &done);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.pc == UINT32_C(0x5ea));
        EXPECT_TRUE(regs.a[6] == UINT32_C(0x1000));
        if (i == 0u) {
            fprintf(stderr, "chain-live: l3 steps=%u events=%u\n",
                    (unsigned)done, (unsigned)bus.event_count);
        }
    }
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 3u);
    for (i = 0u; i < 12u; ++i) {
        if (cram[VF2_SOUND_COMMAND_RING_BASE + i] != want_ring[i]) {
            fprintf(stderr, "chain-live: ring[%u]: C=%02x want=%02x\n",
                    (unsigned)i,
                    (unsigned)cram[VF2_SOUND_COMMAND_RING_BASE + i],
                    (unsigned)want_ring[i]);
            EXPECT_TRUE(0);
        }
    }
    {
        uint32_t addr = 0u;
        for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
            if (addr >= 0x7efcu && addr < 0x7f00u) {
                continue;
            }
            if (cram[addr] != step_ram[addr]) {
                fprintf(stderr, "chain-live: file RAM mismatch @0x%05x: C=%02x S=%02x\n",
                        (unsigned)addr, (unsigned)cram[addr],
                        (unsigned)step_ram[addr]);
                EXPECT_TRUE(0);
                break;
            }
        }
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
    fprintf(stderr, "chain-live: filed 9 bytes, count=3, ring match, C==S\n");
    /* Independent dispatch rounds: restore the filed snapshot per
     * round (sequential rounds entangle residue -- measured: the
     * a0 body spins when run after the 90-miss zeroes $152e --
     * so rounds share nothing but the filed state). Round 1
     * carries S4-style arming ($152e=5, voice0 +9=0xff) to admit
     * the 90 path; the a0 rounds (0, 2) are designed refusals
     * (d2-residue, sound_board_dispatch_a0) pinned by status +
     * handler + S arrival, without RAM equality. */
    for (round = 0; round < 3; ++round) {
        vf2_status cstatus = VF2_OK;
        uint16_t cursor = (uint16_t)(round * 4);
        uint8_t *voice0 = NULL;
        if (round == 0) {
            memcpy(filed.sound_ram, cram, VF2_SOUND_RAM_BYTES);
            memcpy(&filed.scsp, &board.scsp, sizeof(filed.scsp));
        } else {
            memcpy(cram, filed.sound_ram, VF2_SOUND_RAM_BYTES);
            memcpy(&board.scsp, &filed.scsp, sizeof(board.scsp));
            memcpy(step_ram, filed.sound_ram, VF2_SOUND_RAM_BYTES);
            memcpy(&step_scsp, &filed.scsp, sizeof(step_scsp));
        }
        cram[VF2_SOUND_COMMAND_CURSOR] = (uint8_t)(cursor >> 8u);
        cram[VF2_SOUND_COMMAND_CURSOR + 1u] = (uint8_t)cursor;
        step_ram[VF2_SOUND_COMMAND_CURSOR] = (uint8_t)(cursor >> 8u);
        step_ram[VF2_SOUND_COMMAND_CURSOR + 1u] = (uint8_t)cursor;
        if (round == 1) {
            voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
            voice0[9] = 0xffu;
            voice0[5] = 0x00u;
            cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
            voice0 = step_ram + VF2_SOUND_VOICE_TABLE_BASE;
            voice0[9] = 0xffu;
            voice0[5] = 0x00u;
            step_ram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
        }
        memset(&creport, 0, sizeof(creport));
        cstatus = vf2_sound_board_dispatch_next(&board, &creport);
        bus.ram = step_ram;
        bus.ram_size = sizeof(step_ram);
        bus.rom = audio_rom;
        bus.rom_size = audio_rom_size;
        bus.sample_rom = bank;
        bus.sample_rom_size = 0x800000u;
        bus.scsp = &step_scsp;
        bus.event_count = 0u;
        vf2_m68k_regs_reset(&regs);
        regs.a[4] = UINT32_C(0x2506);
        regs.a[5] = UINT32_C(0x100000);
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x7f00);
        regs.pc = UINT32_C(0x7a6);
        /* Round 1 (90) runs the engine program to the 0xcb2 copy
         * loop; the a0 rounds exit the drain to the main loop
         * (0x265c). */
        status = vf2_m68k_run(&bus, &regs,
                              round == 1 ? UINT32_C(0xcb2)
                                         : UINT32_C(0x265c),
                              0, 20000u, &done);
        fprintf(stderr, "chain-live: round %d c-status=%d s-status=%d handler=0x%05x steps=%u events=%u\n",
                round, (int)cstatus, (int)status,
                (unsigned)creport.handler_address, (unsigned)done,
                (unsigned)bus.event_count);
        EXPECT_TRUE(bus.event_count == 0u);
        /* Step pins (print-first, now locked): a0 drain exit 249,
         * 90 program 407 (= 389 funnel + 18 head/program). */
        if (round == 1) {
            EXPECT_TRUE(done == 407u);
        } else {
            EXPECT_TRUE(done == 249u);
        }
        if (round == 1) {
            /* S4 shape, now with the engine program: C runs the
             * full 90 + setup-author + slot-program path and
             * refuses at the 0xcb2 copy loop with effects
             * applied; S runs to the same point. Full-RAM
             * equality under the S4 exclusions. */
            EXPECT_TRUE(status == VF2_OK);
            EXPECT_TRUE(regs.pc == UINT32_C(0xcb2));
            EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
            EXPECT_TRUE(creport.handler_address == UINT32_C(0xcb2));
            {
                uint32_t addr = 0u;
                for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
                    if (addr == 0x2506u ||
                        (addr >= 0x2508u && addr < 0x250au) ||
                        addr >= 0x7e00u) {
                        continue;
                    }
                    if (cram[addr] != step_ram[addr]) {
                        fprintf(stderr, "chain-live: round %d RAM mismatch @0x%05x: C=%02x S=%02x\n",
                                round, (unsigned)addr,
                                (unsigned)cram[addr],
                                (unsigned)step_ram[addr]);
                        EXPECT_TRUE(0);
                        break;
                    }
                }
            }
            EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
        } else {
            /* Designed a0 refusal: C runs the max-gate head and
             * stops at the 0x1e98 join (sound_board_dispatch_a0;
             * E1j: the live ae10-shape reads idx 0x10 / byte2
             * 0x1f against max 0x1f -- equal, so bgt falls to
             * the join; the old 0x1e74 entry pin moved with the
             * recovered head). The S drain runs the body and
             * returns to the main loop (jmp 0x265c). The 0xc68
             * arrival is main-loop funnel, NOT a per-entry
             * contract (measured: same packet at cursor 8 never
             * funnels there), so stop at the drain exit. Pin
             * the refusal + the clean drain exit. */
            EXPECT_TRUE(cstatus == VF2_ERROR_UNSUPPORTED);
            EXPECT_TRUE(creport.handler_address == UINT32_C(0x1e98));
            EXPECT_TRUE(status == VF2_OK);
            EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
        }
    }
    /* Render from the admitted round-1 post-state: restore filed,
     * re-apply cursor 4 + S4 arming, dispatch C-only (the C==S for
     * this shape was proven in-round). Gates: voice authored by
     * the LIVE {9e} command (+2 == 0x58), render determinism.
     * Audibility is reported, not gated (no engine keying). */
    memcpy(cram, filed.sound_ram, VF2_SOUND_RAM_BYTES);
    memcpy(&board.scsp, &filed.scsp, sizeof(board.scsp));
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 4u;
    {
        uint8_t *voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
        size_t voiced = 0u;
        voice0[9] = 0xffu;
        voice0[5] = 0x00u;
        cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
        memset(&creport, 0, sizeof(creport));
        EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &creport) ==
                    VF2_ERROR_UNSUPPORTED);
        EXPECT_TRUE(creport.handler_address == UINT32_C(0xcb2));
        for (i = 0u; i < VF2_SOUND_VOICE_COUNT; ++i) {
            if (cram[VF2_SOUND_VOICE_TABLE_BASE +
                    i * VF2_SOUND_VOICE_BYTES + 2u] != 0u) {
                ++voiced;
            }
        }
        fprintf(stderr, "chain-live: admitted voiced=%u v0+2=%02x handler=0x%05x\n",
                (unsigned)voiced, (unsigned)voice0[2],
                (unsigned)creport.handler_address);
        EXPECT_TRUE(voiced == 1u);
        EXPECT_TRUE(voice0[2] == 0x58u);
    }
    memcpy(&snapshot, &board, sizeof(snapshot));
    EXPECT_TRUE(vf2_sound_board_render(&board, left, right, 2048u) ==
                VF2_OK);
    for (i = 0u; i < 2048u; ++i) {
        int32_t al = left[i] < 0 ? -(int32_t)left[i] : left[i];
        int32_t ar = right[i] < 0 ? -(int32_t)right[i] : right[i];
        if (al > peak) {
            peak = al;
        }
        if (ar > peak) {
            peak = ar;
        }
    }
    fprintf(stderr, "chain-live: peak=%d\n", (int)peak);
    memcpy(&board, &snapshot, sizeof(board));
    EXPECT_TRUE(vf2_sound_board_render(&board, left_b, right_b,
                                      2048u) == VF2_OK);
    EXPECT_TRUE(memcmp(left, left_b, sizeof(left)) == 0);
    EXPECT_TRUE(memcmp(right, right_b, sizeof(right)) == 0);
    {
        static const uint8_t hdr[44] = {
            0x52u, 0x49u, 0x46u, 0x46u, 0x24u, 0x20u, 0x00u, 0x00u,
            0x57u, 0x41u, 0x56u, 0x45u, 0x66u, 0x6du, 0x74u, 0x20u,
            0x10u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x02u, 0x00u,
            0x44u, 0xacu, 0x00u, 0x00u, 0x10u, 0xb1u, 0x02u, 0x00u,
            0x04u, 0x00u, 0x10u, 0x00u, 0x64u, 0x61u, 0x74u, 0x61u,
            0x00u, 0x20u, 0x00u, 0x00u
        };
        FILE *wav = fopen("out/chain-live.wav", "wb");
        if (wav != NULL) {
            uint8_t frame[4];
            size_t f = 0u;
            (void)fwrite(hdr, 1u, sizeof(hdr), wav);
            for (f = 0u; f < 2048u; ++f) {
                frame[0] = (uint8_t)left[f];
                frame[1] = (uint8_t)((uint16_t)left[f] >> 8u);
                frame[2] = (uint8_t)right[f];
                frame[3] = (uint8_t)((uint16_t)right[f] >> 8u);
                (void)fwrite(frame, 1u, sizeof(frame), wav);
            }
            (void)fclose(wav);
            fprintf(stderr, "chain-live: wrote out/chain-live.wav\n");
        } else {
            fprintf(stderr, "chain-live: out/ missing, WAV skipped\n");
        }
    }
    free(bank);
}

/* E1: engine key-block probe (slot programming + legal copy-loop entry).
 *
 * First-run prints only (no pins): S4-shape board (real init,
 * production packet {9a,0,1,0}, S4 arming) driven two ways --
 * S drain 0x7a6 with progressive stops (0xc82 slot-program
 * entry, 0xcb2 copy-loop head, then 0xcb6 (LEGAL MOVEM entry)),
 * C dispatch_next as today. Prints S regs at each stop, the C
 * refusal and state differential at the proven frontier. The
 * old 0x48d2 vector-4 hypothesis was falsified by the M68000
 * manual and stepper correction v0880. C still refuses at 0xcb2
 * because full copy-loop recovery is a separate task. */
static void test_engine_probe(
    const char *rom_dir,
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static uint8_t pristine_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_scsp pristine_scsp;
    static vf2_sound_board board;
    static uint8_t *sample_window = NULL;
    static size_t sample_window_size = 0u;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *voice0 = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    uint32_t stop = 0u;
    (void)rom_dir;
    memset(&board, 0, sizeof(board));
    memset(step_ram, 0, sizeof(step_ram));
    {
        static const char *names[4] = {
            "mpr-17570.37", "mpr-17571.36",
            "mpr-17572.32", "mpr-17573.31"
        };
        size_t bi = 0u;
        sample_window = (uint8_t *)malloc(0x800000u);
        if (sample_window == NULL) {
            fprintf(stderr, "engine-probe: no bank\n");
            EXPECT_TRUE(0);
            return;
        }
        memset(sample_window, 0, 0x800000u);
        sample_window_size = 0x800000u;
        for (bi = 0u; bi < 4u; ++bi) {
            char path[512];
            FILE *f = NULL;
            size_t got = 0u;
            snprintf(path, sizeof(path), "%s/%s", rom_dir, names[bi]);
            f = fopen(path, "rb");
            if (f == NULL) {
                fprintf(stderr, "engine-probe: missing %s\n", path);
                EXPECT_TRUE(0);
                free(sample_window);
                sample_window = NULL;
                return;
            }
            got = fread(sample_window + bi * 0x200000u, 1u, 0x200000u,
                        f);
            fclose(f);
            EXPECT_TRUE(got == 0x200000u);
        }
    }
    if (vf2_sound_board_initialize(&board, audio_rom, audio_rom_size,
                                   sample_window,
                                   sample_window_size) != VF2_OK) {
        fprintf(stderr, "engine-probe: init failed\n");
        EXPECT_TRUE(0);
        return;
    }
    cram = board.sound_ram;
    {
        uint8_t packet[4];
        packet[0] = 0x9au;
        packet[1] = 0u;
        packet[2] = 1u;
        packet[3] = 0u;
        EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) ==
                    VF2_OK);
    }
    voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[9] = 0xffu;
    voice0[5] = 0x00u;
    cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memcpy(pristine_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&pristine_scsp, &board.scsp, sizeof(pristine_scsp));
    memset(&creport, 0, sizeof(creport));
    EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &creport) ==
                VF2_ERROR_UNSUPPORTED);
    fprintf(stderr, "engine-probe: C handler=0x%05x count=%u\n",
            (unsigned)creport.handler_address,
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT]);
    /* E1b: the C now runs the engine slot program and refuses at
     * the 0xcb2 copy loop. Handler pin moves 0xc68 -> 0xcb2. */
    EXPECT_TRUE(creport.handler_address == UINT32_C(0xcb2));
    /* S runs start from the PRE-dispatch image (S4 pattern). */
    for (stop = 0u; stop < 5u; ++stop) {
        uint32_t stop_ip = stop == 0u ? UINT32_C(0x9ba)
            : stop == 1u ? UINT32_C(0xa8c)
            : stop == 2u ? UINT32_C(0xc82)
            : stop == 3u ? UINT32_C(0xcb2) : UINT32_C(0xcb6);
        memcpy(step_ram, pristine_ram, VF2_SOUND_RAM_BYTES);
        memcpy(&step_scsp, &pristine_scsp, sizeof(step_scsp));
        bus.ram = step_ram;
        bus.ram_size = sizeof(step_ram);
        bus.rom = audio_rom;
        bus.rom_size = audio_rom_size;
        bus.sample_rom = sample_window;
        bus.sample_rom_size = sample_window_size;
        bus.scsp = &step_scsp;
        bus.event_count = 0u;
        vf2_m68k_regs_reset(&regs);
        regs.a[4] = UINT32_C(0x2506);
        regs.a[5] = UINT32_C(0x100000);
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x7f00);
        regs.pc = UINT32_C(0x7a6);
        /* The unreachable 0x9ba probe is bounded at the established
         * 0xcb6 frontier. The other stops target their own PCs. */
        status = vf2_m68k_run(&bus, &regs, stop_ip, 0,
                              stop == 0u ? 384u : 20000u, &done);
        fprintf(stderr, "engine-probe: stop=0x%05x s-status=%d pc=0x%05x steps=%u events=%u\n",
                (unsigned)stop_ip, (int)status, (unsigned)regs.pc,
                (unsigned)done, (unsigned)bus.event_count);
        fprintf(stderr, "engine-probe: d0=%08x d2=%08x d3=%08x d4=%08x d5=%08x d6=%08x a0=%08x a1=%08x a2=%08x a3=%08x a4=%08x a5=%08x\n",
                (unsigned)regs.d[0], (unsigned)regs.d[2],
                (unsigned)regs.d[3], (unsigned)regs.d[4],
                (unsigned)regs.d[5], (unsigned)regs.d[6],
                (unsigned)regs.a[0], (unsigned)regs.a[1],
                (unsigned)regs.a[2], (unsigned)regs.a[3],
                (unsigned)regs.a[4], (unsigned)regs.a[5]);
        if (stop == 2u) {
            size_t k = 0u;
            fprintf(stderr, "engine-probe: desc10=");
            for (k = 0u; k < 16u; ++k) {
                fprintf(stderr, "%02x", (unsigned)cram[0x40a0u + k]);
            }
            fprintf(stderr, "\nengine-probe: voice0=");
            for (k = 0u; k < 16u; ++k) {
                fprintf(stderr, "%02x", (unsigned)cram[VF2_SOUND_VOICE_TABLE_BASE + k]);
            }
            fprintf(stderr, "\nengine-probe: tab7f80=");
            for (k = 0u; k < 8u; ++k) {
                fprintf(stderr, "%02x", (unsigned)cram[0x7f80u + k]);
            }
            fprintf(stderr, "\nengine-probe: shared=");
            for (k = 0u; k < 8u; ++k) {
                fprintf(stderr, "%02x", (unsigned)cram[0x2510u + k]);
            }
            fprintf(stderr, "\n");
            /* Engine entry state (stepper-measured, S4 packet):
             * d2 = rate word, d3.w = $5000 table index, d5/a0 =
             * table pair at 0x5000+d3 ({sample ptr, value}),
             * a1 = voice ptr, a2 = table base. 372 steps. */
            EXPECT_TRUE(status == VF2_OK);
            EXPECT_TRUE(regs.pc == UINT32_C(0xc82));
            EXPECT_TRUE(done == 372u);
            EXPECT_TRUE(regs.d[2] == UINT32_C(0x1004));
            EXPECT_TRUE(regs.d[3] == UINT32_C(0x2f80));
            EXPECT_TRUE(regs.d[5] == UINT32_C(0x4a62));
            EXPECT_TRUE(regs.a[0] == UINT32_C(0xf00000));
            EXPECT_TRUE(regs.a[1] == UINT32_C(0x2800));
            EXPECT_TRUE(regs.a[2] == UINT32_C(0x5000));
        } else if (stop == 3u) {
            /* Post slot-program: d5 -= 0x200, d6 = KEYON word,
             * d2 += 0xeba (rate pair), a2/a3 re-based. 383 steps.
             * E1b: the C applies the same program, so full-RAM +
             * SCSP equality under the S4 exclusions (the S
             * movem-load has no memory effects). */
            EXPECT_TRUE(status == VF2_OK);
            EXPECT_TRUE(regs.pc == UINT32_C(0xcb2));
            EXPECT_TRUE(done == 383u);
            EXPECT_TRUE(regs.d[2] == UINT32_C(0x1ebe));
            EXPECT_TRUE(regs.d[5] == UINT32_C(0x4862));
            EXPECT_TRUE(regs.d[6] == UINT32_C(0x10310000));
            EXPECT_TRUE(regs.a[2] == UINT32_C(0x10000));
            EXPECT_TRUE(regs.a[3] == UINT32_C(0x2000));
            {
                uint32_t addr = 0u;
                for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
                    if (addr == 0x2506u ||
                        (addr >= 0x2508u && addr < 0x250au) ||
                        addr >= 0x7e00u) {
                        continue;
                    }
                    if (cram[addr] != step_ram[addr]) {
                        fprintf(stderr, "engine-probe: RAM mismatch @0x%05x: C=%02x S=%02x\n",
                                (unsigned)addr, (unsigned)cram[addr],
                                (unsigned)step_ram[addr]);
                        EXPECT_TRUE(0);
                        break;
                    }
                }
            }
            EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
            if (!scsp_equal(&board.scsp, &step_scsp)) {
                size_t q = 0u;
                for (q = 0u; q < 0x20u; ++q) {
                    if (board.scsp.registers[q] != step_scsp.registers[q]) {
                        fprintf(stderr, "engine-probe: reg[%02x]: C=%02x S=%02x\n",
                                (unsigned)q,
                                (unsigned)board.scsp.registers[q],
                                (unsigned)step_scsp.registers[q]);
                    }
                }
                for (q = 0u; q < 2u; ++q) {
                    fprintf(stderr, "engine-probe: slot%u addr C=%08x S=%08x active C=%u S=%u\n",
                            (unsigned)q,
                            (unsigned)board.scsp.slot_address[q],
                            (unsigned)step_scsp.slot_address[q],
                            (unsigned)board.scsp.slot_active[q],
                            (unsigned)step_scsp.slot_active[q]);
                }
                EXPECT_TRUE(0);
            }
        } else if (stop == 4u) {
            /* Stop BEFORE the now-legal MOVEM at 0xcb6; do not
             * assert an illegal instruction or invent a later exit. */
            EXPECT_TRUE(status == VF2_OK);
            EXPECT_TRUE(regs.pc == UINT32_C(0xcb6));
            EXPECT_TRUE(done == 384u);
        } else {
            /* Setup waypoints: 0x9ba is NOT entered in this S4
             * scenario before the bounded 0xcb6 frontier; 0xa8c
             * is independently reached after 277 instructions. */
            if (stop_ip == UINT32_C(0xa8c)) {
                EXPECT_TRUE(status == VF2_OK);
                EXPECT_TRUE(regs.pc == UINT32_C(0xa8c));
                EXPECT_TRUE(done == 277u);
            } else {
                EXPECT_TRUE(status == VF2_ERROR_UNSUPPORTED);
                EXPECT_TRUE(regs.pc == UINT32_C(0xcb6));
                EXPECT_TRUE(done == 384u);
            }
        }
    }
    free(sample_window);
    sample_window = NULL;
}

/* E1c key-on leg (v0825): the 0xed8 arm as a full native
 * dispatch. Same S4 fixture as test_engine_probe, but the gate
 * bit is staged pre-dispatch (table+0 |= 0x80000000 at 0x7f80;
 * init-staged, survives to the gate -- synthetic scenario,
 * ROM-exact logic). C: full dispatch to OK (handled 1, count 0,
 * cursor +4, all 20 descriptors scanned). S: 0xf10 in 381 steps,
 * rts resume at 0x7f6 in 1, drain exit 0x265c in 62. Compared:
 * full RAM under the S4 exclusions + full SCSP incl.
 * slot_active[0]==1 both sides; render non-silence (peak>0)
 * + determinism (no S-render oracle, S4b precedent). */
static void test_engine_ed8_keyon(
    const char *rom_dir,
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static uint8_t pristine_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    static uint8_t *sample_window = NULL;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *voice0 = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    uint16_t w = 0u;
    {
        static const char *names[4] = {
            "mpr-17570.37", "mpr-17571.36",
            "mpr-17572.32", "mpr-17573.31"
        };
        size_t bi = 0u;
        sample_window = (uint8_t *)malloc(0x800000u);
        if (sample_window == NULL) {
            fprintf(stderr, "ed8-scout: no bank\n");
            EXPECT_TRUE(0);
            return;
        }
        memset(sample_window, 0, 0x800000u);
        for (bi = 0u; bi < 4u; ++bi) {
            char path[512];
            FILE *f = NULL;
            size_t got = 0u;
            snprintf(path, sizeof(path), "%s/%s", rom_dir, names[bi]);
            f = fopen(path, "rb");
            if (f == NULL) {
                fprintf(stderr, "ed8-scout: missing %s\n", path);
                EXPECT_TRUE(0);
                free(sample_window);
                sample_window = NULL;
                return;
            }
            got = fread(sample_window + bi * 0x200000u, 1u, 0x200000u,
                        f);
            fclose(f);
            EXPECT_TRUE(got == 0x200000u);
        }
    }
    if (vf2_sound_board_initialize(&board, audio_rom, audio_rom_size,
                                   sample_window,
                                   0x800000u) != VF2_OK) {
        fprintf(stderr, "ed8-scout: init failed\n");
        EXPECT_TRUE(0);
        return;
    }
    cram = board.sound_ram;
    {
        uint8_t packet[4];
        packet[0] = 0x9au;
        packet[1] = 0u;
        packet[2] = 1u;
        packet[3] = 0u;
        EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) ==
                    VF2_OK);
    }
    voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    voice0[9] = 0xffu;
    voice0[5] = 0x00u;
    cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    /* Stage the gate bit in BOTH images (pre-dispatch): the table
     * is init-staged, not rewritten mid-drain (scout proved the
     * patch survives: post == pre with bit31 intact). Synthetic
     * scenario, ROM-exact logic (a8c-syn precedent). */
    cram[0x7f80u] |= 0x80u;
    memcpy(pristine_ram, cram, VF2_SOUND_RAM_BYTES);
    fprintf(stderr, "ed8-scout: table7f80=");
    {
        size_t k = 0u;
        for (k = 0u; k < 16u; ++k) {
            fprintf(stderr, "%02x", (unsigned)cram[0x7f80u + k]);
        }
    }
    fprintf(stderr, "\n");
    memset(&creport, 0, sizeof(creport));
    {
        vf2_status cstatus =
            vf2_sound_board_dispatch_next(&board, &creport);
        fprintf(stderr, "ed8-scout: C status=%d handler=0x%05x handled=%d count=%u cursor=%04x->%04x scan=%u match=%u sword=%04x\n",
                (int)cstatus, (unsigned)creport.handler_address,
                creport.handled,
                (unsigned)cram[VF2_SOUND_COMMAND_COUNT],
                (unsigned)creport.cursor_before,
                (unsigned)creport.cursor_after,
                (unsigned)creport.descriptors_scanned,
                (unsigned)creport.descriptors_matched,
                (unsigned)creport.lookup_word);
        EXPECT_TRUE(cstatus == VF2_OK);
    }
    /* E1c: the key-on arm completes the dispatch -- the drain scan
     * resumes past the rts (S: 0x7f6 in 1 step, exit 0x265c) and
     * the packet is consumed (count 0, cursor +4, handled 1). */
    EXPECT_TRUE(creport.handled == 1);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 0u);
    EXPECT_TRUE(creport.cursor_after ==
                (uint16_t)((creport.cursor_before + 4u) &
                           UINT16_C(0x0fff)));
    /* S run with the gate bit staged: table+0 |= 0x80000000. */
    memcpy(step_ram, pristine_ram, VF2_SOUND_RAM_BYTES);
    step_ram[0x7f80u] |= 0x80u;
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = sample_window;
    bus.sample_rom_size = 0x800000u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0xf10), 0, 20000u,
                          &done);
    fprintf(stderr, "ed8-scout: S s-status=%d pc=0x%05x steps=%u events=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count);
    fprintf(stderr, "ed8-scout: S d2=%08x d4=%08x d5=%08x d6=%08x a0=%08x a2=%08x a3=%08x\n",
            (unsigned)regs.d[2], (unsigned)regs.d[4],
            (unsigned)regs.d[5], (unsigned)regs.d[6],
            (unsigned)regs.a[0], (unsigned)regs.a[2],
            (unsigned)regs.a[3]);
    /* Stepper-pinned arm shape (measured 2026-10-15): 381 steps
     * to the 0xf10 rts; d2 untouched rate, d4 = slot 0,
     * d5 = post-RMW word (0x4a61 low with 0x18 low byte). */
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0xf10));
    EXPECT_TRUE(done == 381u);
    EXPECT_TRUE(regs.d[2] == UINT32_C(0x1004));
    EXPECT_TRUE(regs.d[4] == UINT32_C(0));
    EXPECT_TRUE(regs.d[5] == UINT32_C(0x4a18));
    EXPECT_TRUE(step_ram[0x2000u] == 0x40u);
    EXPECT_TRUE(step_scsp.slot_active[0] == 1u);
    if (vf2_scsp_read_u16(&step_scsp, 0u, &w) == VF2_OK) {
        uint16_t w2 = 0u, w4 = 0u, w6 = 0u;
        vf2_scsp_read_u16(&step_scsp, 4u, &w4);
        vf2_scsp_read_u16(&step_scsp, 6u, &w6);
        if (vf2_scsp_read_u16(&step_scsp, 2u, &w2) != VF2_OK) {
            w2 = 0u;
        }
        fprintf(stderr, "ed8-scout: S slot0=%04x+%04x +4=%04x +6=%04x\n",
                (unsigned)w, (unsigned)w2, (unsigned)w4,
                (unsigned)w6);
        EXPECT_TRUE(w == UINT16_C(0x1830));
        EXPECT_TRUE(w2 == UINT16_C(0));
        EXPECT_TRUE(w4 == UINT16_C(0));
        EXPECT_TRUE(w6 == UINT16_C(0x4a61));
    } else {
        EXPECT_TRUE(0);
    }
    if (status == VF2_OK && regs.pc == UINT32_C(0xf10)) {
        /* Step the rts: expect resume at 0x7f6 (drain loop). */
        vf2_status st2 = vf2_m68k_run(&bus, &regs, UINT32_C(0x7f6),
                                      0, 20000u, &done);
        fprintf(stderr, "ed8-scout: S resume s-status=%d pc=0x%05x steps=%u\n",
                (int)st2, (unsigned)regs.pc, (unsigned)done);
        EXPECT_TRUE(st2 == VF2_OK);
        EXPECT_TRUE(regs.pc == UINT32_C(0x7f6));
        EXPECT_TRUE(done == 1u);
        /* Full drain to main-loop exit: the C consumed the packet
         * (count 0, handled 1); S must show the same poststate. */
        st2 = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c), 0,
                           20000u, &done);
        fprintf(stderr, "ed8-scout: S drain-exit s-status=%d pc=0x%05x steps=%u\n",
                (int)st2, (unsigned)regs.pc, (unsigned)done);
        EXPECT_TRUE(st2 == VF2_OK);
        EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
        EXPECT_TRUE(done == 62u);
        {
            uint32_t addr = 0u;
            for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
                if (addr == 0x2506u ||
                    (addr >= 0x2508u && addr < 0x250au) ||
                    addr >= 0x7e00u) {
                    continue;
                }
                if (cram[addr] != step_ram[addr]) {
                    fprintf(stderr, "ed8-scout: RAM mismatch @0x%05x: C=%02x S=%02x\n",
                            (unsigned)addr, (unsigned)cram[addr],
                            (unsigned)step_ram[addr]);
                    EXPECT_TRUE(0);
                    break;
                }
            }
        }
        EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
        EXPECT_TRUE(board.scsp.slot_active[0] == 1u);
    }
    {
        /* First live-keyed render from a native dispatch: model
         * gate is non-silence + determinism (no S-render oracle,
         * S4b precedent). */
        static int16_t left[2048];
        static int16_t right[2048];
        static int16_t left2[2048];
        static int16_t right2[2048];
        vf2_sound_board snapshot;
        int32_t peak = 0;
        size_t i = 0u;
        memcpy(&snapshot, &board, sizeof(snapshot));
        EXPECT_TRUE(vf2_sound_board_render(&board, left, right,
                                           2048u) == VF2_OK);
        for (i = 0u; i < 2048u; ++i) {
            int32_t al = left[i] < 0 ? -(int32_t)left[i]
                                     : (int32_t)left[i];
            int32_t ar = right[i] < 0 ? -(int32_t)right[i]
                                      : (int32_t)right[i];
            if (al > peak) {
                peak = al;
            }
            if (ar > peak) {
                peak = ar;
            }
        }
        fprintf(stderr, "ed8-scout: render peak=%d\n", (int)peak);
        /* First live-keyed audible render from a native dispatch:
         * non-silence proves the key-on took effect in the model. */
        EXPECT_TRUE(peak > 0);
        memcpy(&board, &snapshot, sizeof(board));
        EXPECT_TRUE(vf2_sound_board_render(&board, left2, right2,
                                           2048u) == VF2_OK);
        EXPECT_TRUE(memcmp(left, left2, sizeof(left)) == 0);
        EXPECT_TRUE(memcmp(right, right2, sizeof(right)) == 0);
    }
    free(sample_window);
    sample_window = NULL;
}

/* E1f-b: the 0x80 voice scan over the E1f-a row fixture
 * ($1516=0x00ff, $1515=0x90, channel 0). Round A stages the
 * andi witness first (voice0: +2=0x80/full match -> andi clears
 * to 0x00, beq skips) then the jsr match (voice1:
 * +2=0x40/full match -> jsr taken): C refuses at the 0x11c0 jsr
 * door, S arrives there in 72 steps (E1f-c will recover the
 * subroutine). Order matters: both sides stop at the FIRST
 * jsr, so witnesses must precede the match in scan order.
 * Round B unmatches voice1 and adds the early-miss witness
 * (voice2 +2=0xc0/+3=0x00: skipped before the andi, bit7
 * kept): miss-all exhausts to the 0x11ce rts, the drain
 * resumes and exits (0x265c), the packet consumes, full-RAM +
 * SCSP equality under the drain exclusions. Round-B S steps
 * print-first, locked after measurement. */
static void test_dispatch_80_high(
    const char *rom_dir,
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static uint8_t pristine_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_scsp pristine_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    (void)rom_dir;
    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);
    {
        uint8_t packet[4];
        packet[0] = 0x80u;
        packet[1] = 0x05u;
        packet[2] = 0u;
        packet[3] = 0u;
        EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) ==
                    VF2_OK);
    }
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x00u;
    desc[2] = 0xf3u;
    desc[4] = 0x07u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memcpy(pristine_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&pristine_scsp, &board.scsp, sizeof(pristine_scsp));
    /* E1f-b/c: five rounds over the E1f-a row fixture
     * ($1516=0x00ff, $1515=0x90, channel 0). voice0 is the andi
     * witness in every round (+2=0x80/full match -> 0x00, beq
     * skips); voice2 is the early-miss witness (+2=0xc0/+3=0x00,
     * bit7 kept). voice1 + chantable select the 0x11d0 arm:
     * round 0 teardown-normal (+0xb=0, bit3 clear), round 1
     * key+L1-arm (+0xb=5), round 2 defer (chantable bit7),
     * round 3 teardown-release (+0xb=0, +2=0x48), round 4
     * miss-all (voice1 +3=0x00). Every round completes the
     * dispatch (C OK + consume); S drains to 0x265c with
     * full-RAM + SCSP equality. New-round S steps print-first,
     * locked after measurement. */
    {
        int round = 0;
        for (round = 0; round < 5; ++round) {
            uint8_t *v0 = NULL;
            uint8_t *v1 = NULL;
            uint8_t *v2 = NULL;
            uint8_t *sv0 = NULL;
            uint8_t *sv1 = NULL;
            uint8_t *sv2 = NULL;
            vf2_status cstatus = VF2_OK;
            memcpy(cram, pristine_ram, VF2_SOUND_RAM_BYTES);
            memcpy(&board.scsp, &pristine_scsp, sizeof(board.scsp));
            memcpy(step_ram, pristine_ram, VF2_SOUND_RAM_BYTES);
            memcpy(&step_scsp, &pristine_scsp, sizeof(step_scsp));
            v0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
            v1 = cram + VF2_SOUND_VOICE_TABLE_BASE + 0x10u;
            v2 = cram + VF2_SOUND_VOICE_TABLE_BASE + 0x20u;
            sv0 = step_ram + VF2_SOUND_VOICE_TABLE_BASE;
            sv1 = step_ram + VF2_SOUND_VOICE_TABLE_BASE + 0x10u;
            sv2 = step_ram + VF2_SOUND_VOICE_TABLE_BASE + 0x20u;
            v0[VF2_SOUND_VOICE_FLAGS] = 0x80u;
            v0[VF2_SOUND_VOICE_BYTE] = 0x90u;
            v0[VF2_SOUND_VOICE_CHANNEL] = 0x00u;
            v0[VF2_SOUND_VOICE_SELECT] = 0x00u;
            v0[VF2_SOUND_VOICE_SELECT + 1u] = 0xffu;
            v1[VF2_SOUND_VOICE_FLAGS] =
                round == 3 ? 0x48u : 0x40u;
            v1[VF2_SOUND_VOICE_BYTE] = round == 4 ? 0x00u : 0x90u;
            v1[VF2_SOUND_VOICE_CHANNEL] = 0x00u;
            v1[VF2_SOUND_VOICE_SELECT] = 0x00u;
            v1[VF2_SOUND_VOICE_SELECT + 1u] = 0xffu;
            v1[VF2_SOUND_VOICE_RATE] = round == 1 || round == 2
                ? 0x05u : 0x00u;
            v2[VF2_SOUND_VOICE_FLAGS] = 0xc0u;
            v2[VF2_SOUND_VOICE_BYTE] = 0x00u;
            cram[VF2_SOUND_CHANNEL_CONTROL_BASE] =
                round == 2 ? 0x80u : 0x00u;
            sv0[VF2_SOUND_VOICE_FLAGS] = 0x80u;
            sv0[VF2_SOUND_VOICE_BYTE] = 0x90u;
            sv0[VF2_SOUND_VOICE_CHANNEL] = 0x00u;
            sv0[VF2_SOUND_VOICE_SELECT] = 0x00u;
            sv0[VF2_SOUND_VOICE_SELECT + 1u] = 0xffu;
            sv1[VF2_SOUND_VOICE_FLAGS] =
                round == 3 ? 0x48u : 0x40u;
            sv1[VF2_SOUND_VOICE_BYTE] = round == 4 ? 0x00u : 0x90u;
            sv1[VF2_SOUND_VOICE_CHANNEL] = 0x00u;
            sv1[VF2_SOUND_VOICE_SELECT] = 0x00u;
            sv1[VF2_SOUND_VOICE_SELECT + 1u] = 0xffu;
            sv1[VF2_SOUND_VOICE_RATE] = round == 1 || round == 2
                ? 0x05u : 0x00u;
            sv2[VF2_SOUND_VOICE_FLAGS] = 0xc0u;
            sv2[VF2_SOUND_VOICE_BYTE] = 0x00u;
            step_ram[VF2_SOUND_CHANNEL_CONTROL_BASE] =
                round == 2 ? 0x80u : 0x00u;
            memset(&creport, 0, sizeof(creport));
            cstatus =
                vf2_sound_board_dispatch_next(&board, &creport);
            bus.ram = step_ram;
            bus.ram_size = sizeof(step_ram);
            bus.rom = audio_rom;
            bus.rom_size = audio_rom_size;
            bus.sample_rom = NULL;
            bus.sample_rom_size = 0u;
            bus.scsp = &step_scsp;
            bus.event_count = 0u;
            vf2_m68k_regs_reset(&regs);
            regs.a[4] = UINT32_C(0x2506);
            regs.a[5] = UINT32_C(0x100000);
            regs.a[6] = UINT32_C(0x1000);
            regs.a[7] = UINT32_C(0x7f00);
            regs.pc = UINT32_C(0x7a6);
            status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c),
                                  0, 20000u, &done);
            fprintf(stderr, "dispatch 80-sub: round %d C=%d handled=%d count=%u v1+2=%02x scsp0=%02x/%02x e151e=%02x e152e=%02x S=%d pc=0x%05x steps=%u events=%u\n",
                    round, (int)cstatus, creport.handled,
                    (unsigned)cram[VF2_SOUND_COMMAND_COUNT],
                    (unsigned)v1[VF2_SOUND_VOICE_FLAGS],
                    (unsigned)board.scsp.registers[0x00u],
                    (unsigned)board.scsp.registers[0x0bu],
                    (unsigned)cram[VF2_SOUND_SHARED_COUNTER_151E],
                    (unsigned)cram[VF2_SOUND_SHARED_COUNTER_152E],
                    (int)status, (unsigned)regs.pc,
                    (unsigned)done, (unsigned)bus.event_count);
            /* Every round completes the dispatch (the sub always
             * rts's): consume + drain exit + full post-state. */
            EXPECT_TRUE(cstatus == VF2_OK);
            EXPECT_TRUE(creport.handled == 1);
            EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 0u);
            EXPECT_TRUE(v0[VF2_SOUND_VOICE_FLAGS] == 0x00u);
            EXPECT_TRUE(v2[VF2_SOUND_VOICE_FLAGS] == 0xc0u);
            EXPECT_TRUE(status == VF2_OK);
            EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
            EXPECT_TRUE(done == (round == 0 ? 534u : round == 1
                ? 330u : round == 2 ? 326u : round == 3 ? 470u
                : 309u));
            if (round == 0) {
                /* Teardown-normal: key-off pair, +2=0, +9 snap,
                 * $151e-- (0 -> 0xff). */
                EXPECT_TRUE(v1[VF2_SOUND_VOICE_FLAGS] == 0x00u);
                EXPECT_TRUE(board.scsp.registers[0x00u] == 0x10u);
                EXPECT_TRUE(board.scsp.registers[0x0bu] == 0x1fu);
                EXPECT_TRUE(cram[VF2_SOUND_SHARED_COUNTER_151E] ==
                            0xffu);
            } else if (round == 1) {
                /* Key+L1-arm: slot key, bset #0, counters live. */
                EXPECT_TRUE(v1[VF2_SOUND_VOICE_FLAGS] == 0x41u);
                EXPECT_TRUE(board.scsp.registers[0x00u] == 0x10u);
                EXPECT_TRUE(board.scsp.registers[0x0bu] == 0x00u);
                EXPECT_TRUE(cram[VF2_SOUND_SHARED_COUNTER_151E] ==
                            0x00u);
            } else if (round == 2) {
                /* Defer: bset #1 only, no SCSP, no counters. */
                EXPECT_TRUE(v1[VF2_SOUND_VOICE_FLAGS] == 0x42u);
                EXPECT_TRUE(board.scsp.registers[0x00u] == 0x00u);
                EXPECT_TRUE(board.scsp.registers[0x0bu] == 0x00u);
            } else if (round == 3) {
                /* Teardown-release: key-off pair, +2/+9/+5=0,
                 * $152e-- (0 -> 0xff). */
                EXPECT_TRUE(v1[VF2_SOUND_VOICE_FLAGS] == 0x00u);
                EXPECT_TRUE(v1[VF2_SOUND_VOICE_AGE] == 0x00u);
                EXPECT_TRUE(v1[VF2_SOUND_VOICE_AGE_RELEASE] ==
                            0x00u);
                EXPECT_TRUE(board.scsp.registers[0x00u] == 0x10u);
                EXPECT_TRUE(board.scsp.registers[0x0bu] == 0x1fu);
                EXPECT_TRUE(cram[VF2_SOUND_SHARED_COUNTER_152E] ==
                            0xffu);
            } else {
                EXPECT_TRUE(done == 309u);
            }
            {
                uint32_t addr = 0u;
                for (addr = 0u; addr < VF2_SOUND_RAM_BYTES;
                     ++addr) {
                    if (addr == 0x2506u ||
                        (addr >= 0x2508u && addr < 0x250au) ||
                        addr >= 0x7e00u) {
                        continue;
                    }
                    if (cram[addr] != step_ram[addr]) {
                        fprintf(stderr, "dispatch 80-sub: round %d RAM mismatch @0x%05x: C=%02x S=%02x\n",
                                round, (unsigned)addr,
                                (unsigned)cram[addr],
                                (unsigned)step_ram[addr]);
                        EXPECT_TRUE(0);
                        break;
                    }
                }
            }
            EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
        }
    }
}

/* E1g: 0x80 LOW row three-round differential. Same shape as the
 * E1f-c HIGH rounds but desc[2] = 0x03 (LOW sel 3 -> table
 * 0x6055e6, walk e0: 0x0c <= 0x52, $1516 = 0x008a,
 * $1515 = byte(0x60c3e6+0x1f) = 0xa7). voice0 is the andi
 * witness (+2=0x80/full match -> 0x00); voice2 the early-miss
 * witness (+2=0xc0/+3=0x00, bit7 kept). voice1 selects the
 * 0x11d0 arm: round 0 teardown-normal, round 1 key+L1-arm
 * (+0xb=5), round 2 miss-all (voice1 +3=0x00). Defer/release
 * need no LOW re-proof (chantable/+2-bit3 logic is
 * row-independent, proven via HIGH). Every round completes
 * (C OK + consume); S drains to 0x265c with full-RAM + SCSP
 * equality. S steps print-first, locked after measurement. */
static void test_dispatch_80_low(
    const char *rom_dir,
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static uint8_t pristine_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_scsp pristine_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    (void)rom_dir;
    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    memset(step_ram, 0, sizeof(step_ram));
    vf2_scsp_initialize(&step_scsp);
    {
        uint8_t packet[4];
        packet[0] = 0x80u;
        packet[1] = 0x05u;
        packet[2] = 0u;
        packet[3] = 0u;
        EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) ==
                    VF2_OK);
    }
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x00u;
    desc[2] = 0x03u;
    desc[4] = 0x07u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memcpy(pristine_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&pristine_scsp, &board.scsp, sizeof(pristine_scsp));
    {
        int round = 0;
        for (round = 0; round < 3; ++round) {
            uint8_t *v0 = NULL;
            uint8_t *v1 = NULL;
            uint8_t *v2 = NULL;
            uint8_t *sv0 = NULL;
            uint8_t *sv1 = NULL;
            uint8_t *sv2 = NULL;
            vf2_status cstatus = VF2_OK;
            memcpy(cram, pristine_ram, VF2_SOUND_RAM_BYTES);
            memcpy(&board.scsp, &pristine_scsp, sizeof(board.scsp));
            memcpy(step_ram, pristine_ram, VF2_SOUND_RAM_BYTES);
            memcpy(&step_scsp, &pristine_scsp, sizeof(step_scsp));
            v0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
            v1 = cram + VF2_SOUND_VOICE_TABLE_BASE + 0x10u;
            v2 = cram + VF2_SOUND_VOICE_TABLE_BASE + 0x20u;
            sv0 = step_ram + VF2_SOUND_VOICE_TABLE_BASE;
            sv1 = step_ram + VF2_SOUND_VOICE_TABLE_BASE + 0x10u;
            sv2 = step_ram + VF2_SOUND_VOICE_TABLE_BASE + 0x20u;
            v0[VF2_SOUND_VOICE_FLAGS] = 0x80u;
            v0[VF2_SOUND_VOICE_BYTE] = 0xa7u;
            v0[VF2_SOUND_VOICE_CHANNEL] = 0x00u;
            v0[VF2_SOUND_VOICE_SELECT] = 0x00u;
            v0[VF2_SOUND_VOICE_SELECT + 1u] = 0x8au;
            v1[VF2_SOUND_VOICE_FLAGS] = 0x40u;
            v1[VF2_SOUND_VOICE_BYTE] = round == 2 ? 0x00u : 0xa7u;
            v1[VF2_SOUND_VOICE_CHANNEL] = 0x00u;
            v1[VF2_SOUND_VOICE_SELECT] = 0x00u;
            v1[VF2_SOUND_VOICE_SELECT + 1u] = 0x8au;
            v1[VF2_SOUND_VOICE_RATE] = round == 1 ? 0x05u : 0x00u;
            v2[VF2_SOUND_VOICE_FLAGS] = 0xc0u;
            v2[VF2_SOUND_VOICE_BYTE] = 0x00u;
            sv0[VF2_SOUND_VOICE_FLAGS] = 0x80u;
            sv0[VF2_SOUND_VOICE_BYTE] = 0xa7u;
            sv0[VF2_SOUND_VOICE_CHANNEL] = 0x00u;
            sv0[VF2_SOUND_VOICE_SELECT] = 0x00u;
            sv0[VF2_SOUND_VOICE_SELECT + 1u] = 0x8au;
            sv1[VF2_SOUND_VOICE_FLAGS] = 0x40u;
            sv1[VF2_SOUND_VOICE_BYTE] = round == 2 ? 0x00u : 0xa7u;
            sv1[VF2_SOUND_VOICE_CHANNEL] = 0x00u;
            sv1[VF2_SOUND_VOICE_SELECT] = 0x00u;
            sv1[VF2_SOUND_VOICE_SELECT + 1u] = 0x8au;
            sv1[VF2_SOUND_VOICE_RATE] = round == 1 ? 0x05u : 0x00u;
            sv2[VF2_SOUND_VOICE_FLAGS] = 0xc0u;
            sv2[VF2_SOUND_VOICE_BYTE] = 0x00u;
            memset(&creport, 0, sizeof(creport));
            cstatus =
                vf2_sound_board_dispatch_next(&board, &creport);
            bus.ram = step_ram;
            bus.ram_size = sizeof(step_ram);
            bus.rom = audio_rom;
            bus.rom_size = audio_rom_size;
            bus.sample_rom = NULL;
            bus.sample_rom_size = 0u;
            bus.scsp = &step_scsp;
            bus.event_count = 0u;
            vf2_m68k_regs_reset(&regs);
            regs.a[4] = UINT32_C(0x2506);
            regs.a[5] = UINT32_C(0x100000);
            regs.a[6] = UINT32_C(0x1000);
            regs.a[7] = UINT32_C(0x7f00);
            regs.pc = UINT32_C(0x7a6);
            status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c),
                                  0, 20000u, &done);
            fprintf(stderr, "dispatch 80-low: round %d C=%d handled=%d count=%u v1+2=%02x word=%04x byte=%02x scsp0=%02x/%02x S=%d pc=0x%05x steps=%u events=%u\n",
                    round, (int)cstatus, creport.handled,
                    (unsigned)cram[VF2_SOUND_COMMAND_COUNT],
                    (unsigned)v1[VF2_SOUND_VOICE_FLAGS],
                    (unsigned)((unsigned)cram[VF2_SOUND_LOOKUP_WORD] << 8u |
                               (unsigned)cram[VF2_SOUND_LOOKUP_WORD + 1u]),
                    (unsigned)cram[UINT32_C(0x2515)],
                    (unsigned)board.scsp.registers[0x00u],
                    (unsigned)board.scsp.registers[0x0bu],
                    (int)status, (unsigned)regs.pc,
                    (unsigned)done, (unsigned)bus.event_count);
            EXPECT_TRUE(cstatus == VF2_OK);
            EXPECT_TRUE(creport.handled == 1);
            EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 0u);
            EXPECT_TRUE(v0[VF2_SOUND_VOICE_FLAGS] == 0x00u);
            EXPECT_TRUE(v2[VF2_SOUND_VOICE_FLAGS] == 0xc0u);
            EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_WORD] == 0x00u);
            EXPECT_TRUE(cram[VF2_SOUND_LOOKUP_WORD + 1u] == 0x8au);
            EXPECT_TRUE(cram[UINT32_C(0x2515)] == 0xa7u);
            EXPECT_TRUE(status == VF2_OK);
            EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
            EXPECT_TRUE(done == (round == 0 ? 531u : round == 1
                ? 327u : 306u));
            if (round == 0) {
                EXPECT_TRUE(v1[VF2_SOUND_VOICE_FLAGS] == 0x00u);
                EXPECT_TRUE(board.scsp.registers[0x00u] == 0x10u);
                EXPECT_TRUE(board.scsp.registers[0x0bu] == 0x1fu);
                EXPECT_TRUE(cram[VF2_SOUND_SHARED_COUNTER_151E] ==
                            0xffu);
            } else if (round == 1) {
                EXPECT_TRUE(v1[VF2_SOUND_VOICE_FLAGS] == 0x41u);
                EXPECT_TRUE(board.scsp.registers[0x00u] == 0x10u);
                EXPECT_TRUE(board.scsp.registers[0x0bu] == 0x00u);
            }
            {
                uint32_t addr = 0u;
                for (addr = 0u; addr < VF2_SOUND_RAM_BYTES;
                     ++addr) {
                    if (addr == 0x2506u ||
                        (addr >= 0x2508u && addr < 0x250au) ||
                        addr >= 0x7e00u) {
                        continue;
                    }
                    if (cram[addr] != step_ram[addr]) {
                        fprintf(stderr, "dispatch 80-low: round %d RAM mismatch @0x%05x: C=%02x S=%02x\n",
                                round, (unsigned)addr,
                                (unsigned)cram[addr],
                                (unsigned)step_ram[addr]);
                        EXPECT_TRUE(0);
                        break;
                    }
                }
            }
            EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
        }
    }
}

/* E1h: audible note on->off composition (9/10 arc).
 *
 * Two native dispatches share one board/slot: (b) the E1c 0x90
 * ed8 shape keys slot 0 (KEYONB, slot_active 1) and renders
 * peak P1 > 0; (c) a staged 0x80 LOW packet (desc 0x80/0x00/
 * 0x03/0x07, sel-3 walk to $1516 = 0x008a / $1515 = 0xa7)
 * matches a staged voice1 (slot 0, +2 = 0x40, +0xb = 0) and
 * tears it down (key-off pair, +2 = 0, active 0), rendering
 * release peaks P2/P3. No src change (both dispatches exist):
 * the proof is the composition, E1f-d precedent. S proves (c)
 * from the post-(b) snapshot (drain 0x7a6 -> 0x265c, steps
 * print-first); (b) is E1c-owned, re-asserted lightly (OK +
 * active). Descriptors 1..19 are zero-gated so desc0 matches
 * alone (both images see it -- fixture, E1f precedent).
 * slot_active/peak pins print-first, locked after measurement. */
static void test_note_on_off(
    const char *rom_dir,
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    static int16_t left[2048];
    static int16_t right[2048];
    static int16_t left2[2048];
    static int16_t right2[2048];
    uint8_t *sample_window = NULL;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    vf2_sound_board snapshot;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint8_t *v1 = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    vf2_status cstatus = VF2_OK;
    int32_t p1 = 0;
    int32_t p2 = 0;
    int32_t last2 = 0;
    int32_t p3 = 0;
    size_t i = 0u;
    size_t k = 0u;
    {
        static const char *names[4] = {
            "mpr-17570.37", "mpr-17571.36",
            "mpr-17572.32", "mpr-17573.31"
        };
        size_t bi = 0u;
        sample_window = (uint8_t *)malloc(0x800000u);
        if (sample_window == NULL) {
            fprintf(stderr, "note-on-off: no bank\n");
            EXPECT_TRUE(0);
            return;
        }
        memset(sample_window, 0, 0x800000u);
        for (bi = 0u; bi < 4u; ++bi) {
            char path[512];
            FILE *f = NULL;
            size_t got = 0u;
            snprintf(path, sizeof(path), "%s/%s", rom_dir, names[bi]);
            f = fopen(path, "rb");
            if (f == NULL) {
                fprintf(stderr, "note-on-off: missing %s\n", path);
                EXPECT_TRUE(0);
                free(sample_window);
                return;
            }
            got = fread(sample_window + bi * 0x200000u, 1u, 0x200000u,
                        f);
            fclose(f);
            EXPECT_TRUE(got == 0x200000u);
        }
    }
    if (vf2_sound_board_initialize(&board, audio_rom, audio_rom_size,
                                   sample_window,
                                   0x800000u) != VF2_OK) {
        fprintf(stderr, "note-on-off: init failed\n");
        EXPECT_TRUE(0);
        free(sample_window);
        return;
    }
    cram = board.sound_ram;
    /* ON: E1c shape (0x9a packet, S4 voice/table fixture, gate
     * bit staged pre-dispatch). */
    {
        uint8_t packet[4];
        packet[0] = 0x9au;
        packet[1] = 0u;
        packet[2] = 1u;
        packet[3] = 0u;
        EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) ==
                    VF2_OK);
    }
    {
        uint8_t *voice0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
        voice0[9] = 0xffu;
        voice0[5] = 0x00u;
    }
    cram[VF2_SOUND_SHARED_COUNTER_152E] = 5u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    cram[0x7f80u] |= 0x80u;
    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    EXPECT_TRUE(cstatus == VF2_OK);
    EXPECT_TRUE(board.scsp.slot_active[0] == 1u);
    memcpy(&snapshot, &board, sizeof(snapshot));
    EXPECT_TRUE(vf2_sound_board_render(&board, left, right,
                                       2048u) == VF2_OK);
    for (i = 0u; i < 2048u; ++i) {
        int32_t al = left[i] < 0 ? -(int32_t)left[i]
                                 : (int32_t)left[i];
        int32_t ar = right[i] < 0 ? -(int32_t)right[i]
                                  : (int32_t)right[i];
        if (al > p1) {
            p1 = al;
        }
        if (ar > p1) {
            p1 = ar;
        }
    }
    memcpy(&board, &snapshot, sizeof(board));
    EXPECT_TRUE(vf2_sound_board_render(&board, left2, right2,
                                       2048u) == VF2_OK);
    EXPECT_TRUE(memcmp(left, left2, sizeof(left)) == 0);
    EXPECT_TRUE(memcmp(right, right2, sizeof(right)) == 0);
    fprintf(stderr, "note-on-off: ON ok active=%u peak=%d\n",
            (unsigned)board.scsp.slot_active[0], (int)p1);
    EXPECT_TRUE(p1 > 0);
    /* OFF staging: fresh 0x80 packet, desc0 LOW, descs 1..19
     * gate-shut, voice1 = the slot-0 teardown match. */
    {
        uint8_t packet[4];
        packet[0] = 0x80u;
        packet[1] = 0x05u;
        packet[2] = 0u;
        packet[3] = 0u;
        EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) ==
                    VF2_OK);
    }
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x00u;
    desc[2] = 0x03u;
    desc[3] = 0x00u;
    desc[4] = 0x07u;
    for (k = 1u; k < 20u; ++k) {
        uint8_t *other = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE +
            k * VF2_SOUND_COMMAND_DESCRIPTOR_BYTES;
        other[0] = 0x00u;
    }
    v1 = cram + VF2_SOUND_VOICE_TABLE_BASE + 0x10u;
    /* Slot word 0: the ed8-keyed slot. Init authored index*0x20
     * (voice1 -> 0x20); without this the teardown keys off an
     * idle slot and slot 0 drones (measured: regs 0x18 stuck,
     * active 1, release peaks climbing). */
    v1[VF2_SOUND_VOICE_SLOT] = 0x00u;
    v1[VF2_SOUND_VOICE_SLOT + 1u] = 0x00u;
    v1[VF2_SOUND_VOICE_FLAGS] = 0x40u;
    v1[VF2_SOUND_VOICE_BYTE] = 0xa7u;
    v1[VF2_SOUND_VOICE_CHANNEL] = 0x00u;
    v1[VF2_SOUND_VOICE_SELECT] = 0x00u;
    v1[VF2_SOUND_VOICE_SELECT + 1u] = 0x8au;
    v1[VF2_SOUND_VOICE_RATE] = 0x00u;
    v1[VF2_SOUND_VOICE_AGE] = 0x00u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&creport, 0, sizeof(creport));
    cstatus = vf2_sound_board_dispatch_next(&board, &creport);
    fprintf(stderr, "note-on-off: OFF C=%d handled=%d count=%u cursor=%04x->%04x v1+2=%02x scsp0=%02x/%02x active=%u\n",
            (int)cstatus, creport.handled,
            (unsigned)cram[VF2_SOUND_COMMAND_COUNT],
            (unsigned)creport.cursor_before,
            (unsigned)creport.cursor_after,
            (unsigned)v1[VF2_SOUND_VOICE_FLAGS],
            (unsigned)board.scsp.registers[0x00u],
            (unsigned)board.scsp.registers[0x0bu],
            (unsigned)board.scsp.slot_active[0]);
    EXPECT_TRUE(cstatus == VF2_OK);
    EXPECT_TRUE(creport.handled == 1);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 0u);
    EXPECT_TRUE(creport.cursor_before == 4u);
    EXPECT_TRUE(creport.cursor_after == 8u);
    EXPECT_TRUE(v1[VF2_SOUND_VOICE_FLAGS] == 0x00u);
    EXPECT_TRUE(board.scsp.registers[0x00u] == 0x10u);
    EXPECT_TRUE(board.scsp.registers[0x0bu] == 0x1fu);
    /* Key-off moves the slot to RELEASE, it does not drone and
     * does not edge active off: active stays 1 until the
     * envelope bottoms out (synchronize_slot model; S agrees
     * via SCSP equality). Silence is proven by the renders. */
    EXPECT_TRUE(board.scsp.slot_active[0] == 1u);
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;
    vf2_m68k_regs_reset(&regs);
    regs.a[4] = UINT32_C(0x2506);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x7a6);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x265c),
                          0, 20000u, &done);
    fprintf(stderr, "note-on-off: OFF S=%d pc=0x%05x steps=%u events=%u active=%u\n",
            (int)status, (unsigned)regs.pc, (unsigned)done,
            (unsigned)bus.event_count,
            (unsigned)step_scsp.slot_active[0]);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(regs.pc == UINT32_C(0x265c));
    EXPECT_TRUE(done == 518u);
    EXPECT_TRUE(step_scsp.slot_active[0] == 1u);
    {
        uint32_t addr = 0u;
        for (addr = 0u; addr < VF2_SOUND_RAM_BYTES; ++addr) {
            if (addr == 0x2506u ||
                (addr >= 0x2508u && addr < 0x250au) ||
                addr >= 0x7e00u) {
                continue;
            }
            if (cram[addr] != step_ram[addr]) {
                fprintf(stderr, "note-on-off: RAM mismatch @0x%05x: C=%02x S=%02x\n",
                        (unsigned)addr, (unsigned)cram[addr],
                        (unsigned)step_ram[addr]);
                EXPECT_TRUE(0);
                break;
            }
        }
    }
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
    /* Release renders: first block starts loud and decays, the
     * second must be silent (pins print-first). Determinism on
     * the release block (E1c pattern). */
    memcpy(&snapshot, &board, sizeof(snapshot));
    EXPECT_TRUE(vf2_sound_board_render(&board, left, right,
                                       2048u) == VF2_OK);
    for (i = 0u; i < 2048u; ++i) {
        int32_t al = left[i] < 0 ? -(int32_t)left[i]
                                 : (int32_t)left[i];
        int32_t ar = right[i] < 0 ? -(int32_t)right[i]
                                  : (int32_t)right[i];
        if (al > p2) {
            p2 = al;
        }
        if (ar > p2) {
            p2 = ar;
        }
    }
    {
        int32_t tail = left[2047] < 0 ? -(int32_t)left[2047]
                                      : (int32_t)left[2047];
        int32_t tailr = right[2047] < 0 ? -(int32_t)right[2047]
                                        : (int32_t)right[2047];
        last2 = tail > tailr ? tail : tailr;
    }
    memcpy(&board, &snapshot, sizeof(board));
    EXPECT_TRUE(vf2_sound_board_render(&board, left2, right2,
                                       2048u) == VF2_OK);
    EXPECT_TRUE(memcmp(left, left2, sizeof(left)) == 0);
    EXPECT_TRUE(memcmp(right, right2, sizeof(right)) == 0);
    EXPECT_TRUE(vf2_sound_board_render(&board, left, right,
                                       2048u) == VF2_OK);
    for (i = 0u; i < 2048u; ++i) {
        int32_t al = left[i] < 0 ? -(int32_t)left[i]
                                 : (int32_t)left[i];
        int32_t ar = right[i] < 0 ? -(int32_t)right[i]
                                  : (int32_t)right[i];
        if (al > p3) {
            p3 = al;
        }
        if (ar > p3) {
            p3 = ar;
        }
    }
    fprintf(stderr, "note-on-off: release p2=%d last=%d p3=%d\n",
            (int)p2, (int)last2, (int)p3);
    /* Release tail: the first post-off block starts loud and
     * decays (p2 above 0 but below the keyed peak p1), ends at
     * zero; the second block is fully silent. */
    EXPECT_TRUE(p2 > 0);
    EXPECT_TRUE(p2 < p1);
    EXPECT_TRUE(last2 == 0);
    EXPECT_TRUE(p3 == 0);
    free(sample_window);
}

/* E1f-d: the L1 lifecycle on the E1f-c round-1 post-state.
 * Fixture replicates round 1 (0x80/0x05 packet, HIGH desc,
 * voice0 andi-witness, voice1 +2=0x40/+3=0x90/+4=0/+6=0x00ff/
 * +0xb=5, voice2 early-miss, chantable clear): dispatch keys
 * the slot and arms L1 (+2=0x41). Then five maintain_voices
 * ticks: countdown 5->1 with no expiry, fifth tick expires
 * (normal arm: key-off pair, +2=0, $151e--). Each tick is a
 * C-vs-stepper differential on the evolving state (triangle
 * L1 convention: pc=0x5fe entry, rte stop, a7 balanced, RAM
 * compare below the stack zone + above it, SCSP equal). S
 * step counts print-first, locked after measurement. */
static void test_voice_lifecycle_l1(
    const char *rom_dir,
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_dispatch_report creport;
    vf2_sound_voice_maintenance_report mreport;
    uint8_t *cram = NULL;
    uint8_t *desc = NULL;
    uint8_t *v0 = NULL;
    uint8_t *v1 = NULL;
    uint8_t *v2 = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;
    int tick = 0;
    (void)rom_dir;
    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    {
        uint8_t packet[4];
        packet[0] = 0x80u;
        packet[1] = 0x05u;
        packet[2] = 0u;
        packet[3] = 0u;
        EXPECT_TRUE(vf2_sound_board_emit_command(&board, packet) ==
                    VF2_OK);
    }
    desc = cram + VF2_SOUND_COMMAND_DESCRIPTOR_BASE;
    desc[0] = 0x80u;
    desc[1] = 0x00u;
    desc[2] = 0xf3u;
    desc[4] = 0x07u;
    cram[VF2_SOUND_COMMAND_CURSOR] = 0u;
    cram[VF2_SOUND_COMMAND_CURSOR + 1u] = 0u;
    v0 = cram + VF2_SOUND_VOICE_TABLE_BASE;
    v1 = cram + VF2_SOUND_VOICE_TABLE_BASE + 0x10u;
    v2 = cram + VF2_SOUND_VOICE_TABLE_BASE + 0x20u;
    v0[VF2_SOUND_VOICE_FLAGS] = 0x80u;
    v0[VF2_SOUND_VOICE_BYTE] = 0x90u;
    v0[VF2_SOUND_VOICE_CHANNEL] = 0x00u;
    v0[VF2_SOUND_VOICE_SELECT] = 0x00u;
    v0[VF2_SOUND_VOICE_SELECT + 1u] = 0xffu;
    v1[VF2_SOUND_VOICE_FLAGS] = 0x40u;
    v1[VF2_SOUND_VOICE_BYTE] = 0x90u;
    v1[VF2_SOUND_VOICE_CHANNEL] = 0x00u;
    v1[VF2_SOUND_VOICE_SELECT] = 0x00u;
    v1[VF2_SOUND_VOICE_SELECT + 1u] = 0xffu;
    v1[VF2_SOUND_VOICE_RATE] = 0x05u;
    v2[VF2_SOUND_VOICE_FLAGS] = 0xc0u;
    v2[VF2_SOUND_VOICE_BYTE] = 0x00u;
    memset(&creport, 0, sizeof(creport));
    EXPECT_TRUE(vf2_sound_board_dispatch_next(&board, &creport) ==
                VF2_OK);
    EXPECT_TRUE(v1[VF2_SOUND_VOICE_FLAGS] == 0x41u);
    for (tick = 1; tick <= 5; ++tick) {
        /* Snapshot the pre-tick state for the S run first; the C
         * tick advances cram, then S replays from the snapshot. */
        memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
        memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
        memset(&mreport, 0, sizeof(mreport));
        EXPECT_TRUE(vf2_sound_board_maintain_voices(&board,
                                                   &mreport) ==
                    VF2_OK);
        EXPECT_TRUE(mreport.voices_scanned == 32u);
        bus.ram = step_ram;
        bus.ram_size = sizeof(step_ram);
        bus.rom = audio_rom;
        bus.rom_size = audio_rom_size;
        bus.sample_rom = NULL;
        bus.sample_rom_size = 0u;
        bus.scsp = &step_scsp;
        bus.event_count = 0u;
        vf2_m68k_regs_reset(&regs);
        regs.a[5] = UINT32_C(0x100000);
        regs.a[6] = UINT32_C(0x1000);
        regs.a[7] = UINT32_C(0x7f00);
        regs.pc = UINT32_C(0x5fe);
        status = vf2_m68k_run(&bus, &regs, UINT32_C(0xffffff), 1,
                              20000u, &done);
        fprintf(stderr, "lifecycle: tick %d C timer=%02x expired=%u/%u scsp0=%02x/%02x S steps=%u events=%u a7=%05x\n",
                tick, (unsigned)v1[VF2_SOUND_VOICE_RATE],
                (unsigned)mreport.voices_expired,
                (unsigned)mreport.normal_expirations,
                (unsigned)board.scsp.registers[0x00u],
                (unsigned)board.scsp.registers[0x0bu],
                (unsigned)done, (unsigned)bus.event_count,
                (unsigned)regs.a[7]);
        EXPECT_TRUE(status == VF2_OK);
        EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));
        EXPECT_TRUE(bus.event_count == 0u);
        EXPECT_TRUE(done == (tick < 5 ? 138u : 343u));
        if (tick < 5) {
            EXPECT_TRUE(v1[VF2_SOUND_VOICE_RATE] ==
                        (uint8_t)(5 - tick));
            EXPECT_TRUE(v1[VF2_SOUND_VOICE_FLAGS] == 0x41u);
            EXPECT_TRUE(mreport.voices_expired == 0u);
        } else {
            EXPECT_TRUE(v1[VF2_SOUND_VOICE_RATE] == 0x00u);
            EXPECT_TRUE(v1[VF2_SOUND_VOICE_FLAGS] == 0x00u);
            EXPECT_TRUE(mreport.voices_expired == 1u);
            EXPECT_TRUE(mreport.normal_expirations == 1u);
            EXPECT_TRUE(board.scsp.registers[0x0bu] == 0x1fu);
        }
        /* Triangle compare convention: below the ISR stack zone
         * + above it (C has no register file); then SCSP. */
        EXPECT_TRUE(memcmp(cram, step_ram, 0x7eccu) == 0);
        EXPECT_TRUE(memcmp(cram + 0x7f00u, step_ram + 0x7f00u,
                           VF2_SOUND_RAM_BYTES - 0x7f00u) == 0);
        EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));
    }
}

/* v0826: the 0x700-0xBFF window is the SCSP DSP surface, not sample
 * wave data (audio_dsp_program_measured_v0826.md). This leg pins the
 * static facts the recovery and the C boot mirror rely on: the
 * program table at 0x60493e (count word + two long pointers); the
 * 0x500-byte blob geometry (COEF 0x000-0x07f, MADRS 0x080-0x0bf, a
 * zero pad 0x0c0-0x0ff aligning MPRO at 0x100); the MAME Step()
 * invariants the programs satisfy (IRA <= 0x31, MRD/MWT only on odd
 * steps, MPRO last nonzero step 81/43 -> LastStep 82/44 after the
 * 0xBF0 start write); the EWT steps (blob0 -> EFREG[0..3], blob1 ->
 * EFREG[0..1]); the effect-return table at 0x605368 with blob0
 * arming slots 0-3 only; and the C boot mirror (ROM 0x1b0 jsr
 * 0x28e + 0x1c4 jsr 0x402) uploading blob0 verbatim with $402 =
 * 0x0138. A future ROM set that moves any of these fails here. */

static uint16_t dsp_word(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8u) | bytes[1]);
}

static uint32_t dsp_long(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24u) | ((uint32_t)bytes[1] << 16u) |
           ((uint32_t)bytes[2] << 8u) | (uint32_t)bytes[3];
}

static void test_scsp_dsp_program(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    struct dsp_bank {
        uint32_t pointer;
        unsigned laststep;
        unsigned nonzero_steps;
        unsigned madrs_nonzero;
        unsigned ewt_count;
        unsigned ewt_step[4];
        unsigned ewt_efreg[4];
    };
    static const struct dsp_bank banks[2] = {
        {UINT32_C(0x604948), 82u, 73u, 12u, 4u,
         {45u, 47u, 79u, 81u}, {0u, 1u, 2u, 3u}},
        {UINT32_C(0x604e58), 44u, 38u, 9u, 2u,
         {41u, 43u, 0u, 0u}, {0u, 1u, 0u, 0u}}
    };
    /* The 0xBF0 start write is MPRO step 126's first word
       (window byte 0x100 + 126*8 = 0x4f0). */
    _Static_assert(0x100u + 126u * 8u == 0x4f0u,
                   "0xbf0 start write must land on MPRO step 126");
    static vf2_sound_board board;
    static uint8_t sample_window[0x300000u];
    unsigned bank = 0u;
    unsigned step = 0u;
    unsigned index = 0u;

    EXPECT_TRUE(audio_rom_size >= (size_t)UINT32_C(0x40000));
    EXPECT_TRUE(dsp_word(audio_rom + 0x493eu) == 1u);
    EXPECT_TRUE(dsp_long(audio_rom + 0x4940u) == UINT32_C(0x604948));
    EXPECT_TRUE(dsp_long(audio_rom + 0x4944u) == UINT32_C(0x604e58));
    for (bank = 0u; bank < 2u; ++bank) {
        const uint8_t *blob =
            audio_rom + (banks[bank].pointer - UINT32_C(0x600000));
        unsigned nonzero = 0u;
        unsigned madrs_nonzero = 0u;
        unsigned last_nonzero = 0u;
        unsigned ewt_index = 0u;
        unsigned probe = 0u;

        /* COEF head: both programs open with the same Q15 words. */
        EXPECT_TRUE(dsp_word(blob + 0x00u) == UINT16_C(0x7ff8));
        EXPECT_TRUE(dsp_word(blob + 0x02u) == UINT16_C(0x4000));
        EXPECT_TRUE(dsp_word(blob + 0x04u) == 0u);
        EXPECT_TRUE(dsp_word(blob + 0x06u) == UINT16_C(0xc000));
        EXPECT_TRUE(dsp_word(blob + 0x08u) == UINT16_C(0x8008));
        EXPECT_TRUE(dsp_word(blob + 0x0au) == UINT16_C(0x7ff8));
        /* MADRS: both programs open with the same ring taps. */
        EXPECT_TRUE(dsp_word(blob + 0x80u) == UINT16_C(0x1f00));
        EXPECT_TRUE(dsp_word(blob + 0x82u) == 0u);
        EXPECT_TRUE(dsp_word(blob + 0x84u) == UINT16_C(0x101c));
        for (probe = 0u; probe < 32u; ++probe) {
            if (dsp_word(blob + 0x80u + probe * 2u) != 0u) {
                ++madrs_nonzero;
            }
        }
        EXPECT_TRUE(madrs_nonzero == banks[bank].madrs_nonzero);
        /* Zero pad 0x0c0-0x0ff aligns MPRO at 0x100. */
        for (probe = 0u; probe < 32u; ++probe) {
            EXPECT_TRUE(dsp_word(blob + 0xc0u + probe * 2u) == 0u);
        }
        for (step = 0u; step < 128u; ++step) {
            const uint8_t *ip = blob + 0x100u + step * 8u;
            uint16_t w0 = dsp_word(ip);
            uint16_t w1 = dsp_word(ip + 2u);
            uint16_t w2 = dsp_word(ip + 4u);
            uint16_t w3 = dsp_word(ip + 6u);
            unsigned ira = ((unsigned)w1 >> 6) & 0x3fu;
            unsigned ewt = ((unsigned)w2 >> 12) & 1u;
            unsigned ewa = ((unsigned)w2 >> 8) & 0x0fu;
            unsigned mem = ((unsigned)w2 >> 13) & 0x03u;

            if ((w0 | w1 | w2 | w3) == 0u) {
                continue;
            }
            ++nonzero;
            last_nonzero = step;
            /* Step() aborts the whole program if IRA > 0x31. */
            EXPECT_TRUE(ira <= 0x31u);
            /* MAME only performs MRD/MWT on odd steps. */
            if (mem != 0u) {
                EXPECT_TRUE((step & 1u) != 0u);
            }
            if (ewt != 0u) {
                EXPECT_TRUE(ewt_index < banks[bank].ewt_count);
                EXPECT_TRUE(step == banks[bank].ewt_step[ewt_index]);
                EXPECT_TRUE(ewa == banks[bank].ewt_efreg[ewt_index]);
                ++ewt_index;
            }
        }
        EXPECT_TRUE(nonzero == banks[bank].nonzero_steps);
        EXPECT_TRUE(last_nonzero + 1u == banks[bank].laststep);
        EXPECT_TRUE(ewt_index == banks[bank].ewt_count);
        /* The 0xBF0 start write is MPRO step 126's first word
         * (window byte 0x100 + 126*8 = 0x4f0, asserted above);
         * steps 126-127 are zero, so the mid-burst Start() scan
         * equals the scan after the full copy. */
        for (probe = 0u; probe < 16u; ++probe) {
            EXPECT_TRUE(blob[0x4f0u + probe] == 0u);
        }
        EXPECT_TRUE((size_t)(banks[bank].pointer - UINT32_C(0x600000)) +
                        0x500u <=
                    audio_rom_size);
    }
    /* Effect-return table: count 3, four pointers (entry 3 repeats
     * entry 1), blob0 arms slots 0-3 (nonzero EFSDL), 4-15 zero. */
    EXPECT_TRUE(dsp_word(audio_rom + 0x5368u) == 3u);
    EXPECT_TRUE(dsp_long(audio_rom + 0x536au) == UINT32_C(0x60537a));
    EXPECT_TRUE(dsp_long(audio_rom + 0x536eu) == UINT32_C(0x60538a));
    EXPECT_TRUE(dsp_long(audio_rom + 0x5372u) == UINT32_C(0x60539a));
    EXPECT_TRUE(dsp_long(audio_rom + 0x5376u) == UINT32_C(0x60538a));
    EXPECT_TRUE(audio_rom[0x537au + 0u] == 0xdfu);
    EXPECT_TRUE(audio_rom[0x537au + 1u] == 0xcfu);
    EXPECT_TRUE(audio_rom[0x537au + 2u] == 0xffu);
    EXPECT_TRUE(audio_rom[0x537au + 3u] == 0xefu);
    for (index = 0u; index < 16u; ++index) {
        const uint8_t level = audio_rom[0x537au + index];
        if (index < 4u) {
            EXPECT_TRUE(((level >> 5u) & 7u) != 0u);
        } else {
            EXPECT_TRUE(level == 0u);
        }
    }
    /* C boot mirror: uploads blob0 and writes the delay-RAM config
     * (RBP 0x38, RBL 2 -> 32K-word ring at words 0x38000-0x3ffff). */
    memset(&board, 0, sizeof(board));
    memset(sample_window, 0, sizeof(sample_window));
    EXPECT_TRUE(vf2_sound_board_initialize(&board, audio_rom,
                                           audio_rom_size, sample_window,
                                           sizeof(sample_window)) == VF2_OK);
    EXPECT_TRUE(board.scsp.registers[0x402u] == 0x01u);
    EXPECT_TRUE(board.scsp.registers[0x403u] == 0x38u);
    for (index = 0u; index < 0x500u; ++index) {
        EXPECT_TRUE(board.scsp.registers[0x700u + index] ==
                    audio_rom[0x4948u + index]);
    }
    for (index = 0u; index < 16u; ++index) {
        EXPECT_TRUE(board.scsp.registers[0x17u + index * 0x20u] ==
                    audio_rom[0x537au + index]);
    }
}

/* v0852: the L2 stream tick (ROM 0x1f16..0x1f7a, vector 26) as a
 * C-vs-stepper differential. The tick runs: movem spill, two
 * unconditional SCSP pokes (0x0418 = 0x00ce, 0x0422 = 0x0040), the
 * ring-count gate at ram[0x2506], then the body -- prescaler jsr
 * 0x21ba plus eight worker jsr 0x1f7c calls over the 0x3000 records
 * (stride 0x20; record 0 is both the prescaler's and worker 0's).
 * Fixtures:
 * 1. boot-idle with the poke targets poisoned (the head is the only
 *    state change; the 51-step F10 verdict);
 * 2. count 0xff skips the body even for an armed record, pokes still
 *    apply;
 * 3. live countdowns decay: prescaler byte 5 -> 4, worker word 5 -> 4
 *    and word 0 -> 0xffff (16-bit wrap exact);
 * 4. refusal: an armed record whose word is 1 hands off to the
 *    unmeasured applier -- S stops at the 0x1fba class jump with the
 *    word already decremented; C refuses clean (no mutation);
 * 5. refusal: the prescaler beat emit -- S files be 10 04 at the
 *    ring + cursor/count and reloads; C refuses clean.
 * Compare convention (triangle L1, widened): RAM below the ISR stack
 * zone [0x7ec8,0x7f00) + at/above it (C is stackless; the zone holds
 * the movem frame plus the deepest jsr push word), full SCSP, a7
 * balanced, zero bus events. Refusal legs pin the C's clean state
 * and document the S behavior (no equality claim: a refused tick is
 * unmodeled, not silently wrong). */
static void test_l2_stream_tick(
    const uint8_t *audio_rom,
    size_t audio_rom_size
)
{
    static uint8_t step_ram[VF2_SOUND_RAM_BYTES];
    static vf2_scsp step_scsp;
    static vf2_sound_board board;
    vf2_m68k_bus bus;
    vf2_m68k_regs regs;
    vf2_sound_stream_maintenance_report report;
    uint8_t *cram = NULL;
    uint8_t *record = NULL;
    uint32_t done = 0u;
    vf2_status status = VF2_OK;

    memset(&board, 0, sizeof(board));
    board.audio_rom = audio_rom;
    board.audio_rom_size = audio_rom_size;
    vf2_scsp_initialize(&board.scsp);
    cram = board.sound_ram;
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    bus.ram = step_ram;
    bus.ram_size = sizeof(step_ram);
    bus.rom = audio_rom;
    bus.rom_size = audio_rom_size;
    bus.sample_rom = NULL;
    bus.sample_rom_size = 0u;
    bus.scsp = &step_scsp;
    bus.event_count = 0u;

    /* Fixture 1: boot-idle, poke targets poisoned first. */
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, 0x0418u, 0x1122u) == VF2_OK);
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, 0x0422u, 0x3344u) == VF2_OK);
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&report, 0, sizeof(report));
    EXPECT_TRUE(vf2_sound_board_maintain_streams(&board, &report) == VF2_OK);
    EXPECT_TRUE(report.descriptors_scanned == 8u);
    EXPECT_TRUE(report.timers_expired == 0u);
    EXPECT_TRUE(report.packets_emitted == 0u);
    EXPECT_TRUE(report.refused_stage == 0u);
    EXPECT_TRUE(report.refused_record == 0xffu);
    EXPECT_TRUE(board.scsp.registers[0x418u] == 0x00u &&
                board.scsp.registers[0x419u] == 0xceu);
    EXPECT_TRUE(board.scsp.registers[0x422u] == 0x00u &&
                board.scsp.registers[0x423u] == 0x40u);
    vf2_m68k_regs_reset(&regs);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x1f16);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1f7a), 0, 20000u, &done);
    fprintf(stderr,
            "l2-tick idle: S status=%d steps=%u a7=%05x events=%u\n",
            (int)status, (unsigned)done, (unsigned)regs.a[7],
            (unsigned)bus.event_count);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(done == 51u);
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(memcmp(cram, step_ram, 0x7ec8u) == 0);
    EXPECT_TRUE(memcmp(cram + 0x7f00u, step_ram + 0x7f00u,
                       VF2_SOUND_RAM_BYTES - 0x7f00u) == 0);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));

    /* Fixture 2: ring-count gate 0xff skips the body. */
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    vf2_scsp_initialize(&board.scsp);
    cram[VF2_SOUND_COMMAND_COUNT] = 0xffu;
    record = cram + VF2_SOUND_STREAM_ALTERNATE_BASE;
    record[0] = 0x80u;
    record[2] = 0u;
    record[3] = 5u;
    EXPECT_TRUE(vf2_scsp_write_u16(&board.scsp, 0x0418u, 0x1122u) == VF2_OK);
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&report, 0, sizeof(report));
    EXPECT_TRUE(vf2_sound_board_maintain_streams(&board, &report) == VF2_OK);
    EXPECT_TRUE(report.descriptors_scanned == 0u);
    EXPECT_TRUE(report.refused_stage == 0u);
    EXPECT_TRUE(report.refused_record == 0xffu);
    EXPECT_TRUE(board.scsp.registers[0x418u] == 0x00u &&
                board.scsp.registers[0x419u] == 0xceu);
    vf2_m68k_regs_reset(&regs);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x1f16);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1f7a), 0, 20000u, &done);
    fprintf(stderr, "l2-tick gate: S status=%d steps=%u\n",
            (int)status, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(done == 7u);
    EXPECT_TRUE(memcmp(cram, step_ram, 0x7ec8u) == 0);
    EXPECT_TRUE(memcmp(cram + 0x7f00u, step_ram + 0x7f00u,
                       VF2_SOUND_RAM_BYTES - 0x7f00u) == 0);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));

    /* Fixture 3: live countdowns decay. */
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    vf2_scsp_initialize(&board.scsp);
    record = cram + VF2_SOUND_DESCRIPTOR_BASE;
    record[0x0cu] = 8u;
    record[0x0du] = 5u;
    record = cram + VF2_SOUND_STREAM_ALTERNATE_BASE;
    record[0] = 0x80u;
    record[2] = 0u;
    record[3] = 5u;
    record = cram + VF2_SOUND_DESCRIPTOR_BASE + 0x40u;
    record[0] = 0x80u;
    record[2] = 0u;
    record[3] = 0u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&report, 0, sizeof(report));
    EXPECT_TRUE(vf2_sound_board_maintain_streams(&board, &report) == VF2_OK);
    EXPECT_TRUE(report.descriptors_scanned == 8u);
    EXPECT_TRUE(report.timers_expired == 0u);
    EXPECT_TRUE(cram[VF2_SOUND_DESCRIPTOR_BASE + 0x0du] == 4u);
    EXPECT_TRUE(cram[VF2_SOUND_STREAM_ALTERNATE_BASE + 2u] == 0u &&
                cram[VF2_SOUND_STREAM_ALTERNATE_BASE + 3u] == 4u);
    EXPECT_TRUE(cram[VF2_SOUND_DESCRIPTOR_BASE + 0x40u + 2u] == 0xffu &&
                cram[VF2_SOUND_DESCRIPTOR_BASE + 0x40u + 3u] == 0xffu);
    vf2_m68k_regs_reset(&regs);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x1f16);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1f7a), 0, 20000u, &done);
    fprintf(stderr, "l2-tick decay: S status=%d steps=%u\n",
            (int)status, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(done == 57u);
    EXPECT_TRUE(memcmp(cram, step_ram, 0x7ec8u) == 0);
    EXPECT_TRUE(memcmp(cram + 0x7f00u, step_ram + 0x7f00u,
                       VF2_SOUND_RAM_BYTES - 0x7f00u) == 0);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));

    /* Fixture 4: armed record word 1 -> applier handoff, C refuses. */
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    vf2_scsp_initialize(&board.scsp);
    record = cram + VF2_SOUND_STREAM_ALTERNATE_BASE;
    record[0] = 0x80u;
    record[2] = 0u;
    record[3] = 1u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&report, 0, sizeof(report));
    EXPECT_TRUE(vf2_sound_board_maintain_streams(&board, &report) ==
                VF2_ERROR_UNSUPPORTED);
    EXPECT_TRUE(report.refused_stage == 2u);
    EXPECT_TRUE(report.refused_record == 1u);
    EXPECT_TRUE(report.timers_expired == 1u);
    EXPECT_TRUE(report.packets_emitted == 0u);
    EXPECT_TRUE(cram[VF2_SOUND_STREAM_ALTERNATE_BASE] == 0x80u);
    EXPECT_TRUE(cram[VF2_SOUND_STREAM_ALTERNATE_BASE + 2u] == 0u &&
                cram[VF2_SOUND_STREAM_ALTERNATE_BASE + 3u] == 1u);
    vf2_m68k_regs_reset(&regs);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x1f16);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1fba), 0, 20000u, &done);
    fprintf(stderr, "l2-tick worker-refuse: S status=%d steps=%u\n",
            (int)status, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(done == 32u);
    EXPECT_TRUE(step_ram[VF2_SOUND_STREAM_ALTERNATE_BASE + 2u] == 0u &&
                step_ram[VF2_SOUND_STREAM_ALTERNATE_BASE + 3u] == 0u);

    /* Fixture 5: the previously refused original prescaler pulse
     * is now executed by bounded C, with full-RAM and SCSP parity. */
    memset(cram, 0, VF2_SOUND_RAM_BYTES);
    vf2_scsp_initialize(&board.scsp);
    record = cram + VF2_SOUND_DESCRIPTOR_BASE;
    record[0x0cu] = 8u;
    record[0x0du] = 1u;
    record[0x0eu] = 8u;
    cram[VF2_SOUND_COMMAND_RING_BASE + 3u] = 0xa5u;
    memcpy(step_ram, cram, VF2_SOUND_RAM_BYTES);
    memcpy(&step_scsp, &board.scsp, sizeof(step_scsp));
    memset(&report, 0, sizeof(report));
    EXPECT_TRUE(vf2_sound_board_maintain_streams(&board, &report) ==
                VF2_OK);
    EXPECT_TRUE(report.refused_stage == 0u);
    EXPECT_TRUE(report.packets_emitted == 1u);
    EXPECT_TRUE(report.timers_expired == 1u);
    EXPECT_TRUE(cram[VF2_SOUND_DESCRIPTOR_BASE + 0x0du] == 8u);
    EXPECT_TRUE(cram[VF2_SOUND_DESCRIPTOR_BASE + 0x0eu] == 4u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_RING_BASE] == 0xbeu);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_RING_BASE + 1u] == 0x10u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_RING_BASE + 2u] == 0x04u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_RING_BASE + 3u] == 0xa5u);
    EXPECT_TRUE(cram[VF2_SOUND_COMMAND_COUNT] == 1u);
    vf2_m68k_regs_reset(&regs);
    regs.a[5] = UINT32_C(0x100000);
    regs.a[6] = UINT32_C(0x1000);
    regs.a[7] = UINT32_C(0x7f00);
    regs.pc = UINT32_C(0x1f16);
    status = vf2_m68k_run(&bus, &regs, UINT32_C(0x1f7a), 0, 20000u, &done);
    fprintf(stderr, "l2-tick original BE beat: S status=%d steps=%u\n",
            (int)status, (unsigned)done);
    EXPECT_TRUE(status == VF2_OK);
    EXPECT_TRUE(done == 67u);
    EXPECT_TRUE(regs.a[7] == UINT32_C(0x7f00));
    EXPECT_TRUE(bus.event_count == 0u);
    EXPECT_TRUE(step_ram[VF2_SOUND_COMMAND_RING_BASE + 3u] == 0xa5u);
    EXPECT_TRUE(memcmp(cram, step_ram, 0x7ec8u) == 0);
    EXPECT_TRUE(memcmp(cram + 0x7f00u, step_ram + 0x7f00u,
                       VF2_SOUND_RAM_BYTES - 0x7f00u) == 0);
    EXPECT_TRUE(scsp_equal(&board.scsp, &step_scsp));

}

int main(int argc, char **argv)
{
    uint8_t *audio_rom = NULL;
    size_t audio_rom_size = 0u;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <rom-dir>\n", argv[0]);
        return 2;
    }
    if (vf2_romset_build_region(argv[1], VF2_REGION_AUDIOCPU, &audio_rom,
                                &audio_rom_size) != VF2_OK ||
        audio_rom == NULL) {
        fprintf(stderr, "FAIL: cannot build audio region from %s\n", argv[1]);
        return 2;
    }
    fprintf(stderr, "audio region size: 0x%x\n", (unsigned)audio_rom_size);
    if (argc == 3 && strcmp(argv[2], "--probe-b0-181c") == 0) {
        probe_b0_181c_original(audio_rom, audio_rom_size);
        free(audio_rom);
        return 0;
    }
    if (argc != 2) {
        fprintf(stderr, "usage: %s <rom-dir> [--probe-b0-181c]\n", argv[0]);
        free(audio_rom);
        return 2;
    }
    test_switch_routing(audio_rom, audio_rom_size);
    test_e0_head_nomatch(audio_rom, audio_rom_size);
    test_e0_gate_rts(audio_rom, audio_rom_size);
    test_a0_overmax(audio_rom, audio_rom_size);
    test_e0_differential(audio_rom, audio_rom_size);
    test_drain_e0_entry(audio_rom, audio_rom_size);
    test_drain_90_head(audio_rom, audio_rom_size, 0xf5u,
                       UINT32_C(0xf12), "high");
    test_drain_90_head(audio_rom, audio_rom_size, 0x03u,
                       UINT32_C(0xa3e), "low");
    test_drain_90_trap(audio_rom, audio_rom_size);
    test_drain_80_head(audio_rom, audio_rom_size);
    test_drain_90_high(audio_rom, audio_rom_size, 0x01u, 0x00u,
                       "keep");
    test_drain_90_high(audio_rom, audio_rom_size, 0x08u, 0x00u,
                       "table5");
    test_drain_90_high(audio_rom, audio_rom_size, 0x01u, 0x07u,
                       "clear");
    test_drain_90_gate(audio_rom, audio_rom_size);
    /* S2d Loop-B doors ("setup" takes the 0x9b6 bra to 0xf12:
       $1515 = 0x80 is neither 0xb6 nor 0xb8; "setup9ba" doors to
       0x9ba with payload 0x2a -> base 0x2a -> $1515 0xb6, found by
       the one-off 9ba sweep, hard-pinned). */
    test_drain_loopb(audio_rom, audio_rom_size, 0x93u, 0u, 7u, 0x00u,
                     0x00u, UINT32_C(0xfac), UINT32_C(0xfac), 213u,
                     "miss");
    /* S2d Loop-B arm door rolled forward (S3b): the armed voice is
       now authored through the HIGH arm to the 0xcb2 loop. Same
       fixture as the S3b leg but with zero volume fixtures. */
    test_drain_loopb(audio_rom, audio_rom_size, 0x93u, 0u, 5u, 0xffu,
                     0x00u, UINT32_C(0xcb2), UINT32_C(0xcb2), 322u,
                     "arm");
    test_drain_loopb(audio_rom, audio_rom_size, 0x9fu, 0u, 0u, 0x00u,
                     0x00u, UINT32_C(0xfac), UINT32_C(0xfac), 83u,
                     "setup");
    test_drain_loopb(audio_rom, audio_rom_size, 0x9fu, 0x2au, 0u,
                     0x00u, 0x00u, UINT32_C(0xfac), UINT32_C(0xfac),
                     211u, "setup9ba");
    /* S3a 0x9ba scan: two teardowns + skip-shape. Allocator
     * tails: "two" finds voice0 (0x2810), "skip" finds voice1
     * (0x2820, voice0 stays 0x40). Steps re-measured through
     * the allocator. */
    test_9ba_scan(audio_rom, audio_rom_size,
                  0x40u, 0xbau, 0x0fu, 3u,
                  0x01u, 0xbau, 0x0fu, 7u, 626u, UINT32_C(0x2810),
                  "two");
    test_9ba_scan(audio_rom, audio_rom_size,
                  0x40u, 0xbbu, 0x0fu, 3u,
                  0x00u, 0xbau, 0x0eu, 7u, 219u, UINT32_C(0x2820),
                  "skip");
    test_a8c_high(audio_rom, audio_rom_size);
    test_engine_d92(audio_rom, audio_rom_size);
    test_dispatch_90_low(audio_rom, audio_rom_size);
    test_allocator_exhausted(audio_rom, audio_rom_size);
    test_reaper_found(audio_rom, audio_rom_size);
    test_low_author_drain(audio_rom, audio_rom_size);
    test_fac_d4_crash(audio_rom, audio_rom_size);
    test_dispatch_a0_maxgate(audio_rom, audio_rom_size);
    /* S3c synthetic entries (drain-unreachable bodies). */
    test_a8c_synth(audio_rom, audio_rom_size, 1, "low");
    test_a8c_synth(audio_rom, audio_rom_size, 0, "high-f");
    /* 8g L3 receiver legs (byte -> ring file). */
    test_l3_file(audio_rom, audio_rom_size, 0x85u, 0u, 0u, 0u, 0u, 0u,
                 0u, "hi-word");
    test_l3_file(audio_rom, audio_rom_size, 0xc5u, 0u, 0u, 0u, 0u, 0u,
                 0u, "hi-c0");
    test_l3_file(audio_rom, audio_rom_size, 0x03u, 5u, 9u, 0xaau, 0u, 0u,
                 0u, "lo-file");
    test_l3_file(audio_rom, audio_rom_size, 0x03u, 1u, 9u, 0xaau, 0xaau,
                 0xbbu, 0u, "lo-ringA");
    test_l3_file(audio_rom, audio_rom_size, 0x03u, 0u, 1u, 0xaau, 0xccu,
                 0xddu, 0u, "lo-ringB");
    test_l3_file(audio_rom, audio_rom_size, 0xf5u, 0u, 0u, 0u, 0u, 0u,
                 0u, "f0");
    test_l3_file(audio_rom, audio_rom_size, 0xffu, 0u, 0u, 0u, 0u, 0u,
                 0u, "ff");
    test_l3_file(audio_rom, audio_rom_size, 0x03u, 1u, 9u, 0xfeu, 0xaau,
                 0xbbu, 0u, "dance-guard");
    test_l3_file(audio_rom, audio_rom_size, 0x03u, 1u, 9u, 0xaau, 0xaau,
                 0xbbu, 0xffeu, "wrap");
    test_chain_production(audio_rom, audio_rom_size);
    test_chain_render(audio_rom, audio_rom_size);
    test_chain_audible(argv[1], audio_rom, audio_rom_size);
    test_chain_uart_bridge(argv[1], audio_rom, audio_rom_size);
    test_chain_live_queue(argv[1], audio_rom, audio_rom_size);
    test_engine_probe(argv[1], audio_rom, audio_rom_size);
    test_engine_ed8_keyon(argv[1], audio_rom, audio_rom_size);
    test_dispatch_80_high(argv[1], audio_rom, audio_rom_size);
    test_dispatch_80_low(argv[1], audio_rom, audio_rom_size);
    test_note_on_off(argv[1], audio_rom, audio_rom_size);
    test_voice_lifecycle_l1(argv[1], audio_rom, audio_rom_size);
    test_l2_stream_tick(audio_rom, audio_rom_size);
    test_b0_prescaler_setup_original(audio_rom, audio_rom_size);
    test_b0_181c_native_original(audio_rom, audio_rom_size);
    test_akira_first_worker_return_original(audio_rom, audio_rom_size);
    test_akira_fourteen_initial_packets_original(audio_rom, audio_rom_size);
    test_akira_twelve_initial_packets_original(audio_rom, audio_rom_size);
    test_akira_ten_initial_packets_original(audio_rom, audio_rom_size);
    test_akira_nine_initial_packets_original(audio_rom, audio_rom_size);
    test_akira_six_initial_packets_original(audio_rom, audio_rom_size);
    test_akira_first_9f_original(audio_rom, audio_rom_size);
    test_akira_l2_three_b_original(audio_rom, audio_rom_size);
    test_akira_l2_first_b_original(audio_rom, audio_rom_size);
    test_a0_music_low_header_original(audio_rom, audio_rom_size);
    test_b0_50_7d_7f_original(audio_rom, audio_rom_size);
    test_b0_50_7e_original(audio_rom, audio_rom_size);
    test_session_attach(argv[1]);
    test_scsp_dsp_program(audio_rom, audio_rom_size);
    test_vectors(audio_rom, audio_rom_size);
    free(audio_rom);
    if (failures != 0) {
        fprintf(stderr, "cdispatch: %d failures\n", failures);
    }
    return failures == 0 ? 0 : 1;
}
