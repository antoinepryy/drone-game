"""Turns the sketch's serial output into structured events.

The GUI reads exactly what the firmware prints, so what Jérôme sees on screen is
what the board would print on its serial monitor:

    --- TARGETS ---
    Target → X=1.80 m | Y=3.70 m
    Target → X=0.60 m | Y=3.50 m
    Y min = 3.50 m | Y max = 3.70 m
    [RADAR] invalid length dropped: 2
    [RADAR] bad footer, frame dropped
"""
import re

TARGET_RE = re.compile(r"^Target \S+ X=(-?\d+(?:\.\d+)?) m \| Y=(-?\d+(?:\.\d+)?) m$")
YRANGE_RE = re.compile(r"^Y min = (-?\d+(?:\.\d+)?) m \| Y max = (-?\d+(?:\.\d+)?) m$")
INVALID_LEN_RE = re.compile(r"^\[RADAR\] invalid length dropped: (\d+)$")
BAD_FOOTER = "[RADAR] bad footer, frame dropped"
FRAME_START = "--- TARGETS ---"
NO_TARGET = "No target"


class SerialParser:
    def __init__(self):
        self._targets = None  # list while inside a frame

    def feed(self, line):
        """Feed one line; return an event dict when one is complete, else None."""
        line = line.strip()
        if not line:
            return None

        if line == FRAME_START:
            self._targets = []
            return None

        m = INVALID_LEN_RE.match(line)
        if m:
            self._targets = None
            return {"type": "rejected", "reason": "invalid_length", "detail": int(m.group(1))}
        if line == BAD_FOOTER:
            self._targets = None
            return {"type": "rejected", "reason": "bad_footer"}

        if self._targets is None:
            return None  # banner or anything outside a frame

        m = TARGET_RE.match(line)
        if m:
            self._targets.append([float(m.group(1)), float(m.group(2))])
            return None

        if line == NO_TARGET:
            self._targets = None
            return {"type": "frame", "targets": [], "ymin": None, "ymax": None}

        m = YRANGE_RE.match(line)
        if m:
            targets, self._targets = self._targets, None
            return {"type": "frame", "targets": targets,
                    "ymin": float(m.group(1)), "ymax": float(m.group(2))}

        # Unexpected line inside a frame: drop the partial frame rather than guess.
        self._targets = None
        return {"type": "rejected", "reason": "unparsed_line", "detail": line[:80]}
