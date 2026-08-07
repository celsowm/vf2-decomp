#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/native_runtime.h"

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(                                                \
                stderr,                                             \
                "FAILED %s:%d: %s\n",                              \
                __FILE__,                                           \
                __LINE__,                                           \
                #expression                                         \
            );                                                      \
            ++failures;                                             \
        }                                                           \
    } while (0)

static const char *state_path(void)
{
    return "vf2-native-runtime-state-test.bin";
}

static vf2_native_runtime_state sample_state(void)
{
    vf2_native_runtime_state state;

    memset(&state, 0, sizeof(state));
    state.frame_wait.visits = 1u;
    state.frame_wait.visits_before_interrupt = 4u;
    state.frame_wait.interrupts_injected = 7u;
    state.blocks_executed = 101u;
    state.task_bodies_executed = 43u;
    state.frame_wait_phases = 8u;
    state.scheduler_entries = 5u;
    state.scheduler_transitions = 13u;
    state.scheduler_finishes = 4u;
    state.recovered_instruction_count = UINT64_C(123456789);
    state.recovered_procedure_calls = UINT64_C(9876);
    state.recovered_procedure_returns = UINT64_C(8765);
    return state;
}

static void check_equal(
    const vf2_native_runtime_state *expected,
    const vf2_native_runtime_state *actual
)
{
    CHECK(actual->frame_wait.visits == expected->frame_wait.visits);
    CHECK(actual->frame_wait.visits_before_interrupt ==
          expected->frame_wait.visits_before_interrupt);
    CHECK(actual->frame_wait.interrupts_injected ==
          expected->frame_wait.interrupts_injected);
    CHECK(actual->blocks_executed == expected->blocks_executed);
    CHECK(actual->task_bodies_executed == expected->task_bodies_executed);
    CHECK(actual->frame_wait_phases == expected->frame_wait_phases);
    CHECK(actual->scheduler_entries == expected->scheduler_entries);
    CHECK(actual->scheduler_transitions == expected->scheduler_transitions);
    CHECK(actual->scheduler_finishes == expected->scheduler_finishes);
    CHECK(actual->recovered_instruction_count ==
          expected->recovered_instruction_count);
    CHECK(actual->recovered_procedure_calls ==
          expected->recovered_procedure_calls);
    CHECK(actual->recovered_procedure_returns ==
          expected->recovered_procedure_returns);
}

static void test_invalid_arguments(void)
{
    vf2_native_runtime_state state = sample_state();

    CHECK(
        vf2_native_runtime_state_write_file(NULL, state_path()) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_native_runtime_state_write_file(&state, NULL) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_native_runtime_state_read_file(NULL, state_path()) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
    CHECK(
        vf2_native_runtime_state_read_file(&state, NULL) ==
        VF2_ERROR_INVALID_ARGUMENT
    );

    state.frame_wait.visits = state.frame_wait.visits_before_interrupt;
    CHECK(
        vf2_native_runtime_state_write_file(&state, state_path()) ==
        VF2_ERROR_INVALID_ARGUMENT
    );
}

static void test_round_trip(void)
{
    const vf2_native_runtime_state expected = sample_state();
    vf2_native_runtime_state actual;

    memset(&actual, 0, sizeof(actual));
    CHECK(
        vf2_native_runtime_state_write_file(&expected, state_path()) == VF2_OK
    );
    CHECK(
        vf2_native_runtime_state_read_file(&actual, state_path()) == VF2_OK
    );
    check_equal(&expected, &actual);
    CHECK(remove(state_path()) == 0);
}

static void test_crc_rejection(void)
{
    const vf2_native_runtime_state expected = sample_state();
    vf2_native_runtime_state actual;
    FILE *file = NULL;
    int value = 0;

    CHECK(
        vf2_native_runtime_state_write_file(&expected, state_path()) == VF2_OK
    );
    file = fopen(state_path(), "r+b");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fseek(file, 24L, SEEK_SET) == 0);
        value = fgetc(file);
        CHECK(value != EOF);
        CHECK(fseek(file, 24L, SEEK_SET) == 0);
        CHECK(fputc(value ^ 1, file) != EOF);
        CHECK(fclose(file) == 0);
    }

    memset(&actual, 0, sizeof(actual));
    CHECK(
        vf2_native_runtime_state_read_file(&actual, state_path()) ==
        VF2_ERROR_BAD_CRC32
    );
    CHECK(remove(state_path()) == 0);
}

static void test_size_rejection(void)
{
    const vf2_native_runtime_state expected = sample_state();
    vf2_native_runtime_state actual;
    FILE *file = NULL;

    CHECK(
        vf2_native_runtime_state_write_file(&expected, state_path()) == VF2_OK
    );
    file = fopen(state_path(), "ab");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fputc(0, file) != EOF);
        CHECK(fclose(file) == 0);
    }
    CHECK(
        vf2_native_runtime_state_read_file(&actual, state_path()) ==
        VF2_ERROR_BAD_SIZE
    );
    CHECK(remove(state_path()) == 0);

    file = fopen(state_path(), "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fwrite("VF2", 1u, 3u, file) == 3u);
        CHECK(fclose(file) == 0);
    }
    CHECK(
        vf2_native_runtime_state_read_file(&actual, state_path()) ==
        VF2_ERROR_BAD_SIZE
    );
    CHECK(remove(state_path()) == 0);
}

int main(void)
{
    test_invalid_arguments();
    test_round_trip();
    test_crc_rejection();
    test_size_rejection();

    if (failures != 0) {
        fprintf(stderr, "%d native-runtime-state test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("native-runtime-state tests passed");
    return EXIT_SUCCESS;
}
