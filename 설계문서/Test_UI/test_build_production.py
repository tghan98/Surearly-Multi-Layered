"""Checks for the production HTML generator."""

import unittest

from build_production import BLOCKS, SOURCE, generate, marker


class ProductionBuildTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")

    def test_removes_dedicated_resets_but_keeps_terminal(self):
        output = generate(self.source)
        for removed in ("btnReset", "btnQcReset", "'Q'", "'R'", "DEV-ONLY:"):
            self.assertNotIn(removed, output)
        for kept in ("sendInput", "hexMode", "lineEnding", "btnSend", "btnRawDump", "btnSaveCsv", "btnExportPdf"):
            self.assertIn(kept, output)

    def test_missing_marker_is_rejected(self):
        start = marker("qc-handler", BLOCKS["qc-handler"], "START")
        with self.assertRaises(ValueError):
            generate(self.source.replace(start, "", 1))

    def test_unmarked_reset_command_is_rejected(self):
        with self.assertRaises(ValueError):
            generate(self.source + "\nconst hidden = 'Q';\n")

    def test_generation_is_deterministic(self):
        self.assertEqual(generate(self.source), generate(self.source))


if __name__ == "__main__":
    unittest.main()
