#!/usr/bin/env python3
"""ROM-free and clock-exact conservative SCSP/M68000 scheduling tests."""
from __future__ import annotations

import pathlib
import sys
import unittest
from fractions import Fraction

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from audio_m68k_scheduler_budget import (
    audit_tick, budget_cycles, lower_bound_cycles,
    fifo_prefix_completion_ceiling
)


class BudgetTests(unittest.TestCase):
    def test_model2a_clock_and_timer_a(self):
        self.assertEqual(budget_cycles(), Fraction(12544))
        self.assertEqual(budget_cycles(frames_per_timer=50), Fraction(12800))

    def test_owner_first_tick_is_physically_impossible(self):
        result = audit_tick(1073, 12176, 588)
        self.assertEqual(result["total_instructions"], 13837)
        self.assertEqual(result["minimum_cycles"], 55348)
        self.assertEqual(result["capacity_cycles_floor"], 12544)
        self.assertTrue(result["overload_certain"])
        self.assertFalse(result["accurate_irq_scheduling_claim"])

    def test_optimal_minimum_does_not_prove_actual_feasibility(self):
        self.assertFalse(audit_tick(0, 0, 3136)["overload_certain"])
        self.assertTrue(audit_tick(0, 0, 3137)["overload_certain"])
        self.assertEqual(lower_bound_cycles(0, 0, 588), 2352)

    def test_first_tick_ordered_fifo_ceiling(self):
        # From the original-ROM tick-0 trace: 32 queue entries,
        # L2 1073 instructions, 588 instructions to scan 32
        # descriptors, and the first 3 commands = 336/629/1941.
        self.assertEqual(
            fifo_prefix_completion_ceiling(
                1073, 588, [336, 629, 1941] + [310] * 29),
            2)
        self.assertEqual(fifo_prefix_completion_ceiling(
            3136, 0, [5]), 0)
        self.assertEqual(fifo_prefix_completion_ceiling(
            0, 0, [1] * 64), 64)
        with self.assertRaises(ValueError):
            fifo_prefix_completion_ceiling(0, 0, [0])

    def test_invalid_inputs_refused(self):
        for kw in [{"cpu_hz": 0}, {"sample_rate": 0},
                   {"frames_per_timer": 0}]:
            with self.assertRaises(ValueError):
                budget_cycles(**kw)
        with self.assertRaises(ValueError):
            audit_tick(0, -1, 0)


if __name__ == "__main__":
    unittest.main()
