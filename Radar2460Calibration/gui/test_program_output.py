"""Contract between the sketch and the GUI: the parser must read the real program output.

The reference outputs in host/tests/expected/ are produced by the native build of the
sketch (see host/tests/program_tests.sh). If the sketch changes its serial format, these
tests fail here instead of the GUI silently showing nothing.
"""
import unittest
from collections import Counter
from pathlib import Path

from serial_parser import SerialParser

EXPECTED = Path(__file__).resolve().parent.parent / "host" / "tests" / "expected"


def parse(name):
    parser = SerialParser()
    with open(EXPECTED / f"{name}.txt", encoding="utf-8") as f:
        return [e for e in (parser.feed(line) for line in f) if e is not None]


def kinds(events):
    return Counter(e["type"] if e["type"] == "frame" else e["reason"] for e in events)


class ProgramOutputTest(unittest.TestCase):
    def test_default_run(self):
        events = parse("default")
        self.assertEqual(kinds(events), {"frame": 10})
        self.assertEqual(sum(len(e["targets"]) for e in events), 20)
        for e in events:
            ys = [y for _, y in e["targets"]]
            self.assertEqual((e["ymin"], e["ymax"]), (min(ys), max(ys)))

    def test_faulty_run(self):
        events = parse("faults")
        self.assertEqual(kinds(events), {"frame": 8, "invalid_length": 3, "bad_footer": 1})
        self.assertEqual(sum(1 for e in events if e["type"] == "frame" and not e["targets"]), 1)

    def test_walk_scenario(self):
        events = parse("walk")
        self.assertEqual(kinds(events), {"frame": 600})
        self.assertEqual(sum(len(e["targets"]) for e in events), 2220)
        self.assertEqual(max(len(e["targets"]) for e in events), 6)
        # From 44 s on (frame 440), the X > 0 side stays clear.
        self.assertFalse(any(x > 0 for e in events[440:] for x, _ in e["targets"]))


if __name__ == "__main__":
    unittest.main()
