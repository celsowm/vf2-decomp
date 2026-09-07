#define vf2_native_runtime_step_condition_impl vf2_native_runtime_step_counter_base
#include "native_runtime_condition_impl_leaf.c"
#undef vf2_native_runtime_step_condition_impl
#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_hud_base

#include "game_disp_counter_glyph.inc.c"
#include "game_disp_counter_step.inc.c"

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_clear_base
#include "game_disp_hud_gate.inc.c"

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_event_base
#include "game_disp_clear_branch.inc.c"

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_event_queue_base
#include "game_disp_event_queue_gate.inc.c"

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_resource_base
#include "game_disp_event_flag13.inc.c"

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_hud_body_base
#include "game_disp_fighter_resource_pair.inc.c"

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_hud_body_stride_base
#include "game_disp_hud_body.inc.c"

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_number_base
#include "game_disp_hud_body_stride.inc.c"

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_main_base
#include "game_disp_number_zero.inc.c"

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_cont_base
#include "game_disp_main_compose.inc.c"

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_condition_flag13_base
#include "game_disp_continuation.inc.c"

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_condition_impl
#include "game_disp_continuation_flag13.inc.c"
