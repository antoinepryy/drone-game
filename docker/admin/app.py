"""Admin dashboard: lists the active replicas of the "web" service.

Replicas are discovered through Docker's embedded DNS (one A record per
replica), named via reverse DNS and probed over HTTP. No Docker socket needed.
"""
import json
import socket
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

SERVICE = "web"
PORT = 8000
INDEX = (Path(__file__).parent / "index.html").read_bytes()


def probe(ip):
    try:
        name = socket.gethostbyaddr(ip)[0].split(".")[0]
    except OSError:
        name = ip
    start = time.perf_counter()
    try:
        with urllib.request.urlopen(f"http://{ip}/", timeout=2) as resp:
            status = resp.status
    except Exception:
        status = None
    latency = round((time.perf_counter() - start) * 1000, 1)
    return {"name": name, "ip": ip, "status": status, "healthy": status == 200, "latency_ms": latency}


def replicas():
    try:
        ips = sorted({info[4][0] for info in socket.getaddrinfo(SERVICE, 80, socket.AF_INET, socket.SOCK_STREAM)})
    except socket.gaierror:
        ips = []
    with ThreadPoolExecutor() as pool:
        return sorted(pool.map(probe, ips), key=lambda r: r["name"])


class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.rstrip("/") in ("", "/"):
            self._send(200, "text/html; charset=utf-8", INDEX)
        elif self.path == "/api/replicas":
            body = json.dumps({"service": SERVICE, "replicas": replicas(), "checked_at": time.time()})
            self._send(200, "application/json", body.encode())
        else:
            self._send(404, "text/plain", b"not found")

    def _send(self, code, ctype, body):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
