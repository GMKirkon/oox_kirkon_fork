# SPDX-License-Identifier: Apache-2.0
"""Independent fixtures for historical command plans and metric ingestion."""
import unittest

from baselines import parse_metrics
from paper_graphs import parameters


class HistoricalToolsTest(unittest.TestCase):
    def test_pasl_published_harness_parameters(self):
        # Expected values evaluated independently from graph.ml's load table.
        expected = {
            ("square-grid", "small"): ("width", 707),
            ("square-grid", "large"): ("width", 7071),
            ("cube-grid", "small"): ("nb_on_side", 69),
            ("cube-grid", "large"): ("nb_on_side", 321),
            ("par-chains-100", "large"): ("nb_edges_per_path", 500000),
            ("phases-10-d-2", "large"): ("nb_vertices_per_phase", 3333333),
            ("phases-50-d-5", "large"): ("nb_vertices_per_phase", 800000),
            ("trees-524k", "small"): ("nb_phases", 3),
            ("trees-524k", "medium"): ("nb_phases", 38),
            ("trees-524k", "large"): ("nb_phases", 381),
            ("rand-arity-100", "large"): ("num_rows", 1000000),
        }
        for (kind, size), (name, value) in expected.items():
            with self.subTest(kind=kind, size=size):
                self.assertEqual(parameters(kind, size)[name], value)
        self.assertEqual(parameters("rand-arity-100", "large")["bits"], 32)
        self.assertEqual(parameters("trees-524k", "large")["bits"], 64)

    def test_heartbeat_example_keeps_repeated_measurements(self):
        result = parse_metrics("""diagnostic text
exectime 0.076
nb_promotions 6144
nb_steals 1220
utilization 0.8211
exectime 0.078
""")
        self.assertEqual(result["exectime"], [0.076, 0.078])
        self.assertEqual(result["nb_steals"], [1220])
        self.assertNotIn("nb_stacklet_allocations", result)

    def test_invalid_or_missing_timing_is_rejected(self):
        for output in ("nb_steals 0", "exectime nan", "exectime -1", "exectime inf"):
            with self.subTest(output=output), self.assertRaises(ValueError):
                parse_metrics(output)


if __name__ == "__main__":
    unittest.main()
