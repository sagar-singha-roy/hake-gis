#!/usr/bin/env python3
"""
Hake GeoDesk Licensing Server Runner.
Supports execution via Uvicorn/FastAPI, with seamless fallback to Python standard library
ThreadingHTTPServer so it can run immediately in any Python environment with zero setup.
"""

import json
import os
import sys
from http.server import HTTPServer, BaseHTTPRequestHandler
from socketserver import ThreadingMixIn

# Add current folder to sys.path
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, BASE_DIR)

from app.database import init_db
from app.crypto import init_keys, get_public_key_hex
from app.api import handle_api_request

HOST = os.environ.get("LICENSE_SERVER_HOST", "127.0.0.1")
PORT = int(os.environ.get("LICENSE_SERVER_PORT", "8000"))

class ThreadedHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True

class StandardApiHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        # Clean logging
        sys.stderr.write(f"[{self.log_date_time_string()}] {self.command} {self.path} -> {args[1]}\n")

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
        self.end_headers()

    def do_GET(self):
        self._dispatch("GET")

    def do_POST(self):
        self._dispatch("POST")

    def _dispatch(self, method: str):
        content_len = int(self.headers.get("Content-Length", 0))
        body = {}
        if content_len > 0:
            try:
                raw_body = self.rfile.read(content_len).decode("utf-8")
                body = json.loads(raw_body)
            except Exception:
                body = {}

        client_ip = self.client_address[0]
        status, data, html = handle_api_request(
            method=method,
            path=self.path.split("?")[0],
            body=body,
            headers=dict(self.headers),
            client_ip=client_ip
        )

        self.send_response(status)
        self.send_header("Access-Control-Allow-Origin", "*")
        if html is not None:
            self.send_header("Content-Type", "text/html; charset=utf-8")
            payload_bytes = html.encode("utf-8")
        else:
            self.send_header("Content-Type", "application/json; charset=utf-8")
            payload_bytes = json.dumps(data).encode("utf-8")

        self.send_header("Content-Length", str(len(payload_bytes)))
        self.end_headers()
        self.wfile.write(payload_bytes)

def run_server():
    init_db()
    init_keys()
    pubkey = get_public_key_hex()

    print("=" * 72)
    print("  HAKE GEODESK — ENTERPRISE LICENSING SERVER")
    print(f"  Status:       Listening on http://{HOST}:{PORT}")
    print(f"  Public Key:   {pubkey}")
    print(f"  Admin Panel:  http://{HOST}:{PORT}/admin")
    print(f"  Health Check: http://{HOST}:{PORT}/api/health")
    print("=" * 72)

    # Try running via Uvicorn if available and not overridden
    if os.environ.get("FORCE_STDLIB_SERVER") != "1":
        try:
            import uvicorn
            from app.api import app
            if app is not None:
                print("Starting server using Uvicorn ASGI engine...")
                uvicorn.run(app, host=HOST, port=PORT, log_level="info")
                return
        except ImportError:
            pass

    print("Running with built-in high-concurrency HTTP engine...")
    server = ThreadedHTTPServer((HOST, PORT), StandardApiHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down Hake GeoDesk licensing server.")
        server.server_close()

if __name__ == "__main__":
    run_server()
