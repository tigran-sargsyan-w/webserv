#!/usr/bin/env python3
"""Client request timeout tests. Start webserv with configs/test-client-timeout.conf first."""

import argparse
import socket
import sys
import time


def request_bytes(host: str, path: str = "/") -> bytes:
    return (
        f"GET {path} HTTP/1.1\r\n"
        f"Host: {host}\r\n"
        f"Connection: close\r\n"
        f"\r\n"
    ).encode()


def get_response(host: str, port: int, timeout: float = 3.0) -> tuple[int, bool]:
  """Returns (status_code, success)."""
    payload = request_bytes(host)
    with socket.create_connection((host, port), timeout=timeout) as sock:
        sock.sendall(payload)
        sock.settimeout(timeout)
        data = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
    if b"HTTP/" not in data:
        return (0, False)
    status_line = data.split(b"\r\n", 1)[0]
    status = int(status_line.split()[1])
    return (status, True)


def test_stalled_client_is_closed(host: str, port: int, client_timeout: int) -> bool:
    """Send incomplete request, wait past timeout, connection should be closed."""
    sock = socket.create_connection((host, port), timeout=5.0)
    try:
        sock.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
        time.sleep(client_timeout + 2)
        sock.settimeout(2.0)
        data = sock.recv(4096)
        return data == b""
    finally:
        sock.close()


def test_server_still_works(host: str, port: int) -> bool:
    status, ok = get_response(host, port)
    return ok and status == 200


def test_stalled_client_does_not_block_others(host: str, port: int) -> bool:
    """Normal request completes while another client is stalled."""
    slow = socket.create_connection((host, port), timeout=5.0)
    try:
        slow.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
        started = time.monotonic()
        status, ok = get_response(host, port)
        elapsed = time.monotonic() - started
        return ok and status == 200 and elapsed < 2.0
    finally:
        slow.close()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8098)
    parser.add_argument("--client-timeout", type=int, default=5)
    args = parser.parse_args()

    tests = [
        ("stalled client closed", lambda: test_stalled_client_is_closed(
            args.host, args.port, args.client_timeout)),
        ("server still works", lambda: test_server_still_works(args.host, args.port)),
        ("stalled client does not block others", lambda: test_stalled_client_does_not_block_others(
            args.host, args.port)),
    ]

    failed = 0
    for name, fn in tests:
        try:
            passed = fn()
        except Exception as exc:
            print(f"FAIL {name}: {exc}")
            failed += 1
            continue
        if passed:
            print(f"PASS {name}")
        else:
            print(f"FAIL {name}")
            failed += 1

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())