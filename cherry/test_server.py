#!/usr/bin/env python3
"""
ESP32-S3 box-demo 网络测试服务器

用法: python test_server.py [--port HTTP_PORT] [--tcp-port TCP_PORT]

功能:
  - HTTP GET 测试端点 (默认 :8080)
  - TCP Echo 服务器 (默认 :8081)
"""

import socket
import threading
import argparse
from http.server import HTTPServer, BaseHTTPRequestHandler
from datetime import datetime


# ============================================================
# HTTP Server
# ============================================================

class TestHandler(BaseHTTPRequestHandler):
    """简单的 HTTP 测试服务器"""

    def log_message(self, format, *args):
        print(f"[HTTP] {datetime.now().strftime('%H:%M:%S')} {args[0]}")

    def do_GET(self):
        body = (
            f"box-demo Test Server\n"
            f"Time: {datetime.now().isoformat()}\n"
            f"Path: {self.path}\n"
            f"Client: {self.client_address[0]}\n"
        ).encode()

        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        data = self.rfile.read(content_length) if content_length else b""

        body = (
            f"POST received\n"
            f"Time: {datetime.now().isoformat()}\n"
            f"Path: {self.path}\n"
            f"Body ({len(data)} bytes): {data[:100].decode('utf-8', errors='replace')}\n"
        ).encode()

        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def start_http_server(port: int):
    server = HTTPServer(("0.0.0.0", port), TestHandler)
    print(f"[HTTP] Listening on 0.0.0.0:{port}")
    server.serve_forever()


# ============================================================
# TCP Echo Server
# ============================================================

def handle_tcp_client(conn: socket.socket, addr: tuple):
    print(f"[TCP]  Client connected: {addr}")
    try:
        conn.settimeout(10)
        data = conn.recv(4096)
        if data:
            text = data.decode("utf-8", errors="replace")
            print(f"[TCP]  Received ({len(data)} bytes): {text.strip()}")
            reply = f"[Echo] {text}".encode()
            conn.sendall(reply)
            print(f"[TCP]  Sent echo reply")
    except socket.timeout:
        print(f"[TCP]  Timeout: {addr}")
    except Exception as e:
        print(f"[TCP]  Error: {e}")
    finally:
        conn.close()
        print(f"[TCP]  Disconnected: {addr}")


def start_tcp_server(port: int):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", port))
    sock.listen(5)
    print(f"[TCP]  Listening on 0.0.0.0:{port}")

    while True:
        conn, addr = sock.accept()
        t = threading.Thread(target=handle_tcp_client, args=(conn, addr), daemon=True)
        t.start()


# ============================================================
# Main
# ============================================================

def main():
    parser = argparse.ArgumentParser(description="box-demo network test server")
    parser.add_argument("--port", type=int, default=8080,
                        help="HTTP server port (default: 8080)")
    parser.add_argument("--tcp-port", type=int, default=8081,
                        help="TCP echo server port (default: 8081)")
    args = parser.parse_args()

    print("=" * 50)
    print("  box-demo Test Server")
    print("=" * 50)
    print(f"  HTTP GET : http://<your-ip>:{args.port}/")
    print(f"  TCP Echo : <your-ip>:{args.tcp_port}")
    print(f"  Press Ctrl+C to stop")
    print("=" * 50)

    # Get local IP
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        print(f"  Your IP  : {local_ip}")
        print("=" * 50)
    except:
        pass

    # Start servers in background threads
    http_thread = threading.Thread(target=start_http_server, args=(args.port,), daemon=True)
    tcp_thread = threading.Thread(target=start_tcp_server, args=(args.tcp_port,), daemon=True)

    http_thread.start()
    tcp_thread.start()

    try:
        http_thread.join()
    except KeyboardInterrupt:
        print("\n[MAIN] Shutting down...")


if __name__ == "__main__":
    main()
