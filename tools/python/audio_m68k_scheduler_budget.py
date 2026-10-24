#!/usr/bin/env python3
"""Conservative M68000 instruction-floor budget for a modeled SCSP Timer A.

Does NOT implement 68000 instruction cycle timings or IRQ scheduling.
It only detects traces that are physically impossible EVEN IF every
executed instruction were to take the theoretical minimum of 4 cycles.
"""
from __future__ import annotations

import argparse
import json
from fractions import Fraction

MODEL2A_M68K_HZ = 45_158_400 // 4
SCSP_SAMPLE_HZ = 44_100
VF2_TIMER_A_FRAMES = 49
M68000_MIN_CYCLES_PER_INSTRUCTION = 4


def budget_cycles(
    *, cpu_hz: int = MODEL2A_M68K_HZ,
    sample_rate: int = SCSP_SAMPLE_HZ,
    frames_per_timer: int = VF2_TIMER_A_FRAMES,
) -> Fraction:
    if cpu_hz <= 0 or sample_rate <= 0 or frames_per_timer <= 0:
        raise ValueError("clock, sample rate and Timer-A period must be positive")
    return Fraction(cpu_hz * frames_per_timer, sample_rate)


def lower_bound_cycles(*instruction_counts: int) -> int:
    if any(n < 0 for n in instruction_counts):
        raise ValueError("instruction counts must be nonnegative")
    return sum(instruction_counts) * M68000_MIN_CYCLES_PER_INSTRUCTION


def audit_tick(
    l2_instructions: int, dispatch_instructions: int,
    walker_instructions: int, *, cpu_hz: int = MODEL2A_M68K_HZ,
    sample_rate: int = SCSP_SAMPLE_HZ,
    frames_per_timer: int = VF2_TIMER_A_FRAMES
) -> dict[str, int | bool]:
    capacity = budget_cycles(
        cpu_hz=cpu_hz, sample_rate=sample_rate,
        frames_per_timer=frames_per_timer,
    )
    instruction_count = l2_instructions + dispatch_instructions + walker_instructions
    floor = lower_bound_cycles(
        l2_instructions, dispatch_instructions, walker_instructions
    )
    # The CPU may have LESS available time due to IRQ entry,
    # contention, wait states and non-modeled work. It cannot
    # execute a trace if even its optimistic minimum exceeds this.
    return {
        "cpu_clock_hz": cpu_hz,
        "sample_frames_per_timer": frames_per_timer,
        "capacity_cycles_floor": capacity.numerator // capacity.denominator,
        "total_instructions": instruction_count,
        "minimum_cycles": floor,
        "overload_certain": floor > capacity,
        "cycle_bound_ratio_permille": (floor * 1000 * capacity.denominator)
        // capacity.numerator,
        "accurate_irq_scheduling_claim": False,
    }


def fifo_prefix_completion_ceiling(
    l2_instructions: int, walker_instructions: int,
    command_instructions: list[int],
    *, cpu_hz: int = MODEL2A_M68K_HZ,
    sample_rate: int = SCSP_SAMPLE_HZ,
    frames_per_timer: int = VF2_TIMER_A_FRAMES
) -> int:
    """Optimistic FIFO completions in one timer interval in original order.

    Assumes an entire L2 ISR and walker also execute inside the interval.
    Does NOT count partial progress into the next FIFO command;
    that would require preserving and preempting MC68000 registers.
    """
    cap = budget_cycles(cpu_hz=cpu_hz, sample_rate=sample_rate,
                        frames_per_timer=frames_per_timer)
    lower_bound_cycles(l2_instructions, walker_instructions,
                       *command_instructions)
    if any(n < 1 for n in command_instructions):
        raise ValueError("FIFO command needs at least one instruction")
    remaining = (cap.numerator // cap.denominator) // 4
    remaining -= l2_instructions + walker_instructions
    completed = 0
    for steps in command_instructions:
        if steps > remaining:
            break
        remaining -= steps
        completed += 1
    return completed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--l2-instructions", required=True, type=int)
    parser.add_argument("--drain-instructions", required=True, type=int)
    parser.add_argument("--walker-instructions", required=True, type=int)
    parser.add_argument("--cpu-clock-hz", type=int, default=MODEL2A_M68K_HZ)
    parser.add_argument("--frames-per-timer", type=int, default=VF2_TIMER_A_FRAMES)
    args = parser.parse_args()
    try:
        result = audit_tick(
            args.l2_instructions, args.drain_instructions,
            args.walker_instructions, cpu_hz=args.cpu_clock_hz,
            frames_per_timer=args.frames_per_timer
        )
    except ValueError as exc:
        parser.error(str(exc))
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
