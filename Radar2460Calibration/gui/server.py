"""Real-time radar viewer.

Runs the sketch (native build, simulated LD2460) as a subprocess, parses its serial
output and streams it to the browser with Server-Sent Events. Standard library only.

    GET  /             viewer page
    GET  /events       SSE stream: frame, rejected, injected_fault, config events
    GET  /api/config   current simulator settings
    POST /api/config   {"targets": 1-8, "faults": bool, "speed": 0.25-4} restarts the simulator
"""
import json
import os
import queue
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from serial_parser import SerialParser

PORT = int(os.environ.get("GUI_PORT", "8080"))
RADAR_BIN = os.environ.get("RADAR_BIN", "radar-calibration")
PERIOD_MS = int(os.environ.get("SIM_PERIOD_MS", "100"))
INDEX = (Path(__file__).parent / "index.html").read_bytes()


class Hub:
    """Fan-out of events to every connected browser."""

    def __init__(self):
        self._clients = set()
        self._lock = threading.Lock()

    def subscribe(self):
        q = queue.Queue(maxsize=500)
        with self._lock:
            self._clients.add(q)
        return q

    def unsubscribe(self, q):
        with self._lock:
            self._clients.discard(q)

    def publish(self, event):
        data = json.dumps(event)
        with self._lock:
            clients = list(self._clients)
        for q in clients:
            try:
                q.put_nowait(data)
            except queue.Full:
                pass  # slow browser: drop rather than block the radar reader


class Radar:
    """Owns the simulator process; restarting it applies new settings."""

    def __init__(self, hub):
        self.hub = hub
        self.config = {"targets": 5, "faults": False, "speed": 1.0}
        self._proc = None
        self._lock = threading.Lock()

    def start(self, **changes):
        with self._lock:
            self.config.update(changes)
            if self._proc:
                self._proc.kill()
                self._proc.wait()
            env = dict(os.environ,
                       SIM_FRAMES="0",
                       SIM_SCENARIO="walk",
                       SIM_PERIOD_MS=str(PERIOD_MS),
                       SIM_TARGETS=str(self.config["targets"]),
                       SIM_FAULTS="1" if self.config["faults"] else "0",
                       SIM_SPEED=str(self.config["speed"]))
            proc = subprocess.Popen([RADAR_BIN], env=env, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, text=True, bufsize=1)
            self._proc = proc
        threading.Thread(target=self._read_stdout, args=(proc,), daemon=True).start()
        threading.Thread(target=self._read_stderr, args=(proc,), daemon=True).start()
        self.hub.publish({"type": "config", **self.config, "period_ms": PERIOD_MS})

    def _read_stdout(self, proc):
        parser = SerialParser()
        for line in proc.stdout:
            event = parser.feed(line)
            if event:
                event["t"] = time.time()
                self.hub.publish(event)

    def _read_stderr(self, proc):
        # The simulator announces each malformed frame it sends on stderr.
        for line in proc.stderr:
            if line.startswith("[SIM] fault:"):
                self.hub.publish({"type": "injected_fault", "detail": line[12:].strip(), "t": time.time()})


hub = Hub()
radar = Radar(hub)


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/":
            self._send(200, "text/html; charset=utf-8", INDEX)
        elif self.path == "/api/config":
            self._send(200, "application/json", json.dumps({**radar.config, "period_ms": PERIOD_MS}).encode())
        elif self.path == "/events":
            self._stream()
        else:
            self._send(404, "text/plain", b"not found")

    def do_POST(self):
        if self.path != "/api/config":
            return self._send(404, "text/plain", b"not found")
        try:
            body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
            changes = {}
            if "targets" in body:
                targets = int(body["targets"])
                if not 1 <= targets <= 8:
                    raise ValueError("targets must be between 1 and 8")
                changes["targets"] = targets
            if "speed" in body:
                speed = float(body["speed"])
                if not 0.25 <= speed <= 4:
                    raise ValueError("speed must be between 0.25 and 4")
                changes["speed"] = speed
            if "faults" in body:
                changes["faults"] = bool(body["faults"])
        except (ValueError, TypeError) as exc:
            return self._send(400, "application/json", json.dumps({"error": str(exc)}).encode())
        radar.start(**changes)
        self._send(200, "application/json", json.dumps(radar.config).encode())

    def _stream(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        q = hub.subscribe()
        try:
            self._write(json.dumps({"type": "config", **radar.config, "period_ms": PERIOD_MS}))
            while True:
                try:
                    self._write(q.get(timeout=15))
                except queue.Empty:
                    self.wfile.write(b": keepalive\n\n")
                    self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            hub.unsubscribe(q)

    def _write(self, data):
        self.wfile.write(f"data: {data}\n\n".encode())
        self.wfile.flush()

    def _send(self, code, ctype, body):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    radar.start()
    print(f"Radar viewer: http://localhost:{PORT}", flush=True)
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
