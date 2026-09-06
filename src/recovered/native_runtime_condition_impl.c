#define vf2_native_runtime_step_condition_impl vf2_native_runtime_step_counter_base
#include "native_runtime_condition_impl_leaf.c"
#undef vf2_native_runtime_step_condition_impl
#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_hud_base

#include "game_disp_counter_glyph.inc.c"
#include "game_disp_counter_step.inc.c"

#undef vf2_native_runtime_step
#define vf2_native_runtime_step vf2_native_runtime_step_condition_impl
#include "game_disp_hud_gate.inc.c"
