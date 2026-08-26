#!/usr/bin/env python3
"""
Launch the Clicker ESP32 Web Flasher locally and open it in your default web browser.
Usage:
  python tools/serve_flasher.py
  (or double-click run_flasher.bat / run_flasher.ps1)
"""

import http.server
import socketserver
import webbrowser
import os
import sys
from pathlib import Path

PORT = 8080
ROOT = Path(__file__).resolve().parents[2]

class FlasherHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def log_message(self, format, *args):
        # Clean logging format
        sys.stderr.write(f"[{self.log_date_time_string()}] {format % args}\n")

def main():
    os.chdir(str(ROOT))
    
    # Try port 8080 or fallback to next available port if 8080 is busy
    port = PORT
    server = None
    for p in range(PORT, PORT + 20):
        try:
            server = socketserver.TCPServer(("127.0.0.1", p), FlasherHandler)
            port = p
            break
        except OSError:
            continue

    if server is None:
        print("Error: Could not find an available local port.")
        sys.exit(1)

    url = f"http://localhost:{port}/web_flasher/index.html"
    print("=" * 60)
    print("  CLICKER ESP32 WEB FLASHER SERVER")
    print("=" * 60)
    print(f"  Local URL: {url}")
    print("  Serving root: " + str(ROOT))
    print("  Press Ctrl+C to stop the server.")
    print("=" * 60)

    # Automatically open in browser
    webbrowser.open(url)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server...")
        server.shutdown()
        server.server_close()
        print("Server stopped.")

if __name__ == "__main__":
    main()
