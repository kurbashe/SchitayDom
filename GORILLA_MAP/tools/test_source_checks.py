#!/usr/bin/env python3
"""Regressions from observed DayZ compiler failures; not an Enforce compiler."""
import unittest

from verify import unsupported_index_increments


class EnforceSourceChecks(unittest.TestCase):
    def test_rejects_reported_array_increment_and_related_forms(self):
        source = "counts[marker.Kind]++;\ncounts[index]--;\n++counts[index];\n--obj.Counts[index];"
        self.assertEqual(unsupported_index_increments(source), [1, 2, 3, 4])

    def test_allows_assignment_and_scalar_increments(self):
        source = "counts[marker.Kind] = counts[marker.Kind] + 1;\ni++;\nm_Store.NextId++;"
        self.assertEqual(unsupported_index_increments(source), [])

    def test_ignores_comments_and_strings(self):
        source = '// counts[index]++;\nPrint("counts[index]++;");\n/* ++counts[index]; */\ncounts[index]++;'
        self.assertEqual(unsupported_index_increments(source), [4])


if __name__ == "__main__":
    unittest.main(verbosity=2)
