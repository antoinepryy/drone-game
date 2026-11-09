import unittest

from serial_parser import SerialParser


def run(lines):
    parser = SerialParser()
    return [e for e in (parser.feed(l) for l in lines) if e is not None]


class SerialParserTest(unittest.TestCase):
    def test_frame_with_targets(self):
        events = run([
            "",
            "=== LD2460 X / Y CALIBRATION ===",
            "[RADAR] UART started",
            "",
            "--- TARGETS ---",
            "Target → X=1.80 m | Y=3.70 m",
            "Target → X=-0.60 m | Y=3.50 m",
            "Y min = 3.50 m | Y max = 3.70 m",
        ])
        self.assertEqual(events, [{
            "type": "frame",
            "targets": [[1.8, 3.7], [-0.6, 3.5]],
            "ymin": 3.5, "ymax": 3.7,
        }])

    def test_empty_frame(self):
        events = run(["--- TARGETS ---", "No target"])
        self.assertEqual(events, [{"type": "frame", "targets": [], "ymin": None, "ymax": None}])

    def test_rejected_frames(self):
        events = run(["[RADAR] invalid length dropped: 32767", "[RADAR] bad footer, frame dropped"])
        self.assertEqual(events, [
            {"type": "rejected", "reason": "invalid_length", "detail": 32767},
            {"type": "rejected", "reason": "bad_footer"},
        ])

    def test_new_rejection_reasons(self):
        events = run(["[RADAR] invalid data size dropped: 6", "[RADAR] frame timeout, dropped after 12 bytes"])
        self.assertEqual(events, [
            {"type": "rejected", "reason": "invalid_data_size", "detail": 6},
            {"type": "rejected", "reason": "timeout", "detail": 12},
        ])

    def test_negative_values_between_zero_and_minus_one(self):
        events = run(["--- TARGETS ---", "Target → X=-0.10 m | Y=2.00 m", "Y min = 2.00 m | Y max = 2.00 m"])
        self.assertEqual(events[0]["targets"], [[-0.1, 2.0]])

    def test_unexpected_line_drops_partial_frame(self):
        events = run(["--- TARGETS ---", "Target → X=1.00 m | Y=2.00 m", "garbage", "Y min = 2.00 m | Y max = 2.00 m"])
        self.assertEqual(events, [{"type": "rejected", "reason": "unparsed_line", "detail": "garbage"}])

    def test_rejection_inside_frame_resets_it(self):
        events = run([
            "--- TARGETS ---",
            "Target → X=1.00 m | Y=2.00 m",
            "[RADAR] bad footer, frame dropped",
            "--- TARGETS ---",
            "Target → X=0.50 m | Y=4.00 m",
            "Y min = 4.00 m | Y max = 4.00 m",
        ])
        self.assertEqual([e["type"] for e in events], ["rejected", "frame"])
        self.assertEqual(events[1]["targets"], [[0.5, 4.0]])


if __name__ == "__main__":
    unittest.main()
