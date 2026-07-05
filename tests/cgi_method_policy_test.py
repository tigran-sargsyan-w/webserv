#!/usr/bin/env python3
"""
Small regression guard for CGI method policy.

Run from the repository root after starting the server with default config:

    make
    ./webserv configs/default.conf
    python3 tests/cgi_method_policy_test.py

The /cgi-bin route in configs/default.conf allows only GET and POST, so
DELETE /cgi-bin/env.py must return 405 and must not execute the CGI script.
"""

import argparse
import socket
import sys


def receive_all(sock):
    chunks = []
    while True:
        data = sock.recv(65536)
        if not data:
            break
        chunks.append(data)
    return b"".join(chunks)


def parse_response(raw):
    if b"\r\n\r\n" not in raw:
        raise RuntimeError("response has no complete HTTP header block")
    header_block, body = raw.split(b"\r\n\r\n", 1)
    lines = header_block.split(b"\r\n")
    status_parts = lines[0].decode("latin-1", errors="replace").split(" ", 2)
    if len(status_parts) < 2:
        raise RuntimeError("invalid HTTP status line")
    status = int(status_parts[1])
    headers = {}
    for line in lines[1:]:
        if not line:
            continue
        if b":" not in line:
            raise RuntimeError("invalid response header: %r" % (line,))
        name, value = line.split(b":", 1)
        headers[name.decode("latin-1").strip().lower()] = value.decode("latin-1").strip()
    return status, headers, body.decode("latin-1", errors="replace")


def request(host, port, target):
    payload = (
        "DELETE %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n" % (target, host)
    ).encode("ascii")
    with socket.create_connection((host, port), timeout=5.0) as sock:
        sock.sendall(payload)
        raw = receive_all(sock)
    return parse_response(raw)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", default=8080, type=int)
    parser.add_argument("--target", default="/cgi-bin/env.py")
    args = parser.parse_args()

    status, headers, body = request(args.host, args.port, args.target)

    if status != 405:
        raise AssertionError("expected 405 Method Not Allowed, got %d\n%s" % (status, body))

    allow = headers.get("allow", "")
    if "GET" not in allow or "POST" not in allow:
        raise AssertionError("expected Allow header to contain GET and POST, got %r" % allow)

    forbidden_fragments = (
        "REQUEST_METHOD=DELETE",
        "SCRIPT_NAME=/cgi-bin/env.py",
        "SERVER_PROTOCOL=HTTP/1.1",
        "BODY:",
    )
    for fragment in forbidden_fragments:
        if fragment in body:
            raise AssertionError("CGI appears to have executed; found %r in body" % fragment)

    print("OK: DELETE on CGI route returned 405 and did not execute CGI")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print("FAIL: %s" % exc, file=sys.stderr)
        sys.exit(1)
