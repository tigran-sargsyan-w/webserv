#!/usr/bin/env python3
"""
Webserv functional + regression tester.

Usage from the repository root:

    python3 tests/regression_tester.py record --build \
        --baseline tests/baselines/before_refactor.json

    python3 tests/regression_tester.py compare --build \
        --baseline tests/baselines/before_refactor.json

    python3 tests/regression_tester.py check --build

Usage from the repository root:

    python3 tests/regression_tester.py record \
    --build \
    --check-relink \
    --baseline tests/baselines/before_refactor.json

    python3 tests/regression_tester.py compare \
        --build \
        --check-relink \
        --baseline tests/baselines/before_refactor.json

    python3 tests/regression_tester.py compare \
        --build \
        --stress \
        --check-relink \
        --baseline tests/baselines/before_refactor.json

The tester creates an isolated runtime directory, generates its own config and
website fixtures, starts ./webserv, performs deterministic tests, and stops it.
Only Python's standard library is used.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
import os
import shutil
import socket
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Callable, Dict, Iterable, List, Optional, Tuple, Union


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 18080
DEFAULT_SECOND_PORT = 18081
DEFAULT_SOCKET_TIMEOUT = 6.0
RUNTIME_DIRECTORY = ".webserv_regression_runtime"

SNAPSHOT_HEADERS = (
    "allow",
    "connection",
    "content-length",
    "content-type",
    "location",
)


@dataclass
class HttpResponse:
    raw: bytes
    status: int
    reason: str
    headers: Dict[str, str]
    body: bytes

    def text(self) -> str:
        return self.body.decode("utf-8", errors="replace")

    def snapshot(self) -> Dict[str, object]:
        selected_headers = {
            name: self.headers[name]
            for name in SNAPSHOT_HEADERS
            if name in self.headers
        }
        body_text = self.text()
        if len(body_text) > 500:
            body_preview = body_text[:500] + "...<truncated>"
        else:
            body_preview = body_text
        return {
            "status": self.status,
            "reason": self.reason,
            "headers": selected_headers,
            "body_sha256": hashlib.sha256(self.body).hexdigest(),
            "body_size": len(self.body),
            "body_preview": body_preview,
        }


@dataclass
class TestResult:
    name: str
    category: str
    passed: bool
    expected: str
    actual: str
    snapshot: Dict[str, object]
    duration_ms: int


class TestFailure(Exception):
    pass


def colour(text: str, code: str, enabled: bool) -> str:
    if not enabled:
        return text
    return "\033[" + code + "m" + text + "\033[0m"


def status_line(result: TestResult, use_colour: bool) -> str:
    marker = colour("PASS", "32", use_colour) if result.passed else colour("FAIL", "31", use_colour)
    return (
        f"[{marker}] {result.category:<12} {result.name:<42} "
        f"{result.duration_ms:>5} ms"
    )


def ensure(condition: bool, message: str) -> None:
    if not condition:
        raise TestFailure(message)


def request_bytes(
    method: str,
    target: str,
    host: str,
    headers: Optional[Dict[str, str]] = None,
    body: bytes = b"",
    version: str = "HTTP/1.1",
    add_content_length: bool = True,
) -> bytes:
    final_headers = {"Host": host, "Connection": "close"}
    if headers:
        final_headers.update(headers)
    lower_names = {name.lower() for name in final_headers}
    if body and add_content_length and "content-length" not in lower_names:
        final_headers["Content-Length"] = str(len(body))
    lines = [f"{method} {target} {version}"]
    lines.extend(f"{name}: {value}" for name, value in final_headers.items())
    return ("\r\n".join(lines) + "\r\n\r\n").encode("ascii") + body


def parse_response(raw: bytes) -> HttpResponse:
    if b"\r\n\r\n" in raw:
        header_block, body = raw.split(b"\r\n\r\n", 1)
        lines = header_block.split(b"\r\n")
    elif b"\n\n" in raw:
        header_block, body = raw.split(b"\n\n", 1)
        lines = header_block.split(b"\n")
    else:
        raise TestFailure("response has no complete HTTP header block")

    ensure(bool(lines), "response is empty")
    status_parts = lines[0].decode("latin-1", errors="replace").split(" ", 2)
    ensure(len(status_parts) >= 2, "invalid HTTP status line")
    try:
        status = int(status_parts[1])
    except ValueError as exc:
        raise TestFailure("invalid numeric HTTP status") from exc
    reason = status_parts[2] if len(status_parts) == 3 else ""

    headers: Dict[str, str] = {}
    for line in lines[1:]:
        if not line:
            continue
        ensure(b":" in line, "invalid response header: " + repr(line))
        name, value = line.split(b":", 1)
        headers[name.decode("latin-1").strip().lower()] = (
            value.decode("latin-1").strip()
        )

    if "content-length" in headers:
        try:
            expected_length = int(headers["content-length"])
        except ValueError as exc:
            raise TestFailure("invalid response Content-Length") from exc
        ensure(
            expected_length == len(body),
            f"Content-Length={expected_length}, actual body={len(body)}",
        )

    return HttpResponse(raw, status, reason, headers, body)


def receive_all(sock: socket.socket, timeout: float, slow_read: bool = False) -> bytes:
    sock.settimeout(timeout)
    chunks: List[bytes] = []
    while True:
        try:
            data = sock.recv(257 if slow_read else 65536)
        except socket.timeout as exc:
            raise TestFailure("timed out while waiting for HTTP response") from exc
        if not data:
            break
        chunks.append(data)
        if slow_read:
            time.sleep(0.001)
    return b"".join(chunks)


def exchange(
    host: str,
    port: int,
    payload: bytes,
    timeout: float = DEFAULT_SOCKET_TIMEOUT,
    send_parts: Optional[Iterable[bytes]] = None,
    delay_between_parts: float = 0.03,
    slow_read: bool = False,
) -> HttpResponse:
    with socket.create_connection((host, port), timeout=timeout) as sock:
        if send_parts is None:
            sock.sendall(payload)
        else:
            for part in send_parts:
                sock.sendall(part)
                time.sleep(delay_between_parts)
        raw = receive_all(sock, timeout, slow_read=slow_read)
    return parse_response(raw)


def raw_exchange(
    host: str,
    port: int,
    payload: bytes,
    timeout: float = DEFAULT_SOCKET_TIMEOUT,
) -> HttpResponse:
    return exchange(host, port, payload, timeout=timeout)


def expect_response(
    response: HttpResponse,
    status: int,
    body_contains: Iterable[str] = (),
    headers: Optional[Dict[str, str]] = None,
) -> None:
    ensure(
        response.status == status,
        f"expected status {status}, got {response.status}",
    )
    text = response.text()
    for fragment in body_contains:
        ensure(fragment in text, f"response body does not contain {fragment!r}")
    if headers:
        for name, expected_value in headers.items():
            actual = response.headers.get(name.lower())
            ensure(
                actual == expected_value,
                f"expected header {name}: {expected_value!r}, got {actual!r}",
            )


class WebservProcess:
    def __init__(
        self,
        project_root: Path,
        binary: Path,
        config_path: Path,
        host: str,
        ports: Tuple[int, int],
        log_path: Path,
    ) -> None:
        self.project_root = project_root
        self.binary = binary
        self.config_path = config_path
        self.host = host
        self.ports = ports
        self.log_path = log_path
        self.process = None
        self.log_file = None

    @staticmethod
    def port_is_free(host: str, port: int) -> bool:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(0.2)
            return sock.connect_ex((host, port)) != 0

    def start(self) -> None:
        for port in self.ports:
            if not self.port_is_free(self.host, port):
                raise RuntimeError(
                    f"{self.host}:{port} is already in use; choose another --port"
                )

        self.log_file = self.log_path.open("wb")
        command = [str(self.binary), str(self.config_path)]
        self.process = subprocess.Popen(
            command,
            cwd=str(self.project_root),
            stdout=self.log_file,
            stderr=subprocess.STDOUT,
        )

        deadline = time.monotonic() + 8.0
        waiting = set(self.ports)
        while waiting and time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError(
                    f"webserv exited during startup with code {self.process.returncode}"
                )
            for port in tuple(waiting):
                try:
                    with socket.create_connection((self.host, port), timeout=0.2):
                        waiting.remove(port)
                except OSError:
                    pass
            time.sleep(0.05)

        if waiting:
            raise RuntimeError(
                "webserv did not start listening on: "
                + ", ".join(str(port) for port in sorted(waiting))
            )

    def alive(self) -> bool:
        return self.process is not None and self.process.poll() is None

    def stop(self) -> None:
        if self.process is not None and self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=2)
        if self.log_file is not None:
            self.log_file.close()

    def __enter__(self) -> "WebservProcess":
        self.start()
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        self.stop()


class Fixture:
    def __init__(
        self,
        project_root: Path,
        runtime_root: Path,
        host: str,
        port: int,
        second_port: int,
    ) -> None:
        self.project_root = project_root
        self.runtime_root = runtime_root
        self.host = host
        self.port = port
        self.second_port = second_port
        self.www1 = runtime_root / "www1"
        self.www2 = runtime_root / "www2"
        self.uploads = self.www1 / "uploads"
        self.config_path = runtime_root / "regression.conf"
        self.large_content = (
            b"WEBServ regression large file\n"
            + bytes(range(256)) * 2048
        )

    def relative(self, path: Path) -> str:
        return "./" + str(path.relative_to(self.project_root)).replace(os.sep, "/")

    @staticmethod
    def write(path: Path, content: Union[str, bytes]) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(content, bytes):
            path.write_bytes(content)
        else:
            path.write_text(content, encoding="utf-8")

    def prepare(self) -> None:
        if self.runtime_root.exists():
            shutil.rmtree(self.runtime_root)
        self.runtime_root.mkdir(parents=True)

        self.write(self.www1 / "index.html", "<h1>WEBSERV_REGRESSION_PORT_1</h1>\n")
        self.write(self.www2 / "index.html", "<h1>WEBSERV_REGRESSION_PORT_2</h1>\n")
        self.write(self.www1 / "files" / "alpha.txt", "ALPHA_FILE\n")
        self.write(self.www1 / "files" / "nested" / "beta.txt", "BETA_FILE\n")
        self.write(self.www1 / "private" / "secret.txt", "PRIVATE_FILE\n")
        self.write(self.www1 / "large.bin", self.large_content)
        self.uploads.mkdir(parents=True, exist_ok=True)

        for code in (400, 403, 404, 405, 413, 414, 431, 500, 501, 502, 504):
            self.write(
                self.www1 / "errors" / f"{code}.html",
                f"<h1>CUSTOM_ERROR_{code}</h1>\n",
            )
        for code in (400, 403, 404, 405, 413, 500, 501, 502, 504):
            self.write(
                self.www2 / "errors" / f"{code}.html",
                f"<h1>SECOND_SERVER_ERROR_{code}</h1>\n",
            )

        self.write(
            self.www1 / "cgi-bin" / "hello.py",
            """#!/usr/bin/env python3
print("Content-Type: text/plain")
print()
print("HELLO_FROM_REGRESSION_CGI")
""",
        )
        self.write(
            self.www1 / "cgi-bin" / "hello.sh",
            """#!/bin/sh
echo "Content-Type: text/plain"
echo ""
echo "HELLO_FROM_SHELL_CGI"
echo "REQUEST_METHOD=$REQUEST_METHOD"
echo "QUERY_STRING=$QUERY_STRING"
""",
        )
        self.write(
            self.www1 / "cgi-bin" / "env.py",
            """#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/plain")
print()
keys = [
    "CONTENT_LENGTH",
    "CONTENT_TYPE",
    "GATEWAY_INTERFACE",
    "PATH_INFO",
    "PATH_TRANSLATED",
    "QUERY_STRING",
    "REMOTE_ADDR",
    "REQUEST_METHOD",
    "SCRIPT_NAME",
    "SERVER_NAME",
    "SERVER_PORT",
    "SERVER_PROTOCOL",
    "SERVER_SOFTWARE",
]
for key in keys:
    print("%s=%s" % (key, os.environ.get(key, "<missing>")))
for key in sorted(key for key in os.environ if key.startswith("HTTP_")):
    print("%s=%s" % (key, os.environ.get(key, "<missing>")))
print("BODY:")
print(sys.stdin.read())
""",
        )
        self.write(
            self.www1 / "cgi-bin" / "no_headers.py",
            """#!/usr/bin/env python3
print("CGI_WITHOUT_HEADERS")
""",
        )
        self.write(
            self.www1 / "cgi-bin" / "status.py",
            """#!/usr/bin/env python3
print("Status: 201 Created")
print("Content-Type: text/plain")
print()
print("CGI_CUSTOM_STATUS")
""",
        )
        self.write(
            self.www1 / "cgi-bin" / "slow.py",
            """#!/usr/bin/env python3
import time
time.sleep(2)
print("Content-Type: text/plain")
print()
print("SLOW_CGI_FINISHED")
""",
        )
        self.write(
            self.www1 / "cgi-bin" / "timeout.py",
            """#!/usr/bin/env python3
import time
time.sleep(20)
print("Content-Type: text/plain")
print()
print("THIS_SHOULD_NOT_BE_RETURNED")
""",
        )
        self.write(self.www1 / "cgi-bin" / "not_cgi.txt", "NOT_A_CGI\n")

        root1 = self.relative(self.www1)
        root2 = self.relative(self.www2)
        uploads = self.relative(self.uploads)
        errors1 = self.relative(self.www1 / "errors")
        errors2 = self.relative(self.www2 / "errors")
        cgi_root = self.relative(self.www1 / "cgi-bin")
        files_root = self.relative(self.www1 / "files")
        private_root = self.relative(self.www1 / "private")

        config = f"""server {{
    listen {self.host}:{self.port};
    server_name regression_primary;

    root {root1};
    index index.html;
    client_max_body_size 65536;

    error_page 400 {errors1}/400.html;
    error_page 403 {errors1}/403.html;
    error_page 404 {errors1}/404.html;
    error_page 405 {errors1}/405.html;
    error_page 413 {errors1}/413.html;
    error_page 414 {errors1}/414.html;
    error_page 431 {errors1}/431.html;
    error_page 500 {errors1}/500.html;
    error_page 501 {errors1}/501.html;
    error_page 502 {errors1}/502.html;
    error_page 504 {errors1}/504.html;

    location / {{
        methods GET;
        root {root1};
        index index.html;
        autoindex off;
        upload_enable off;
    }}

    location /files {{
        methods GET;
        root {files_root};
        autoindex on;
        upload_enable off;
    }}

    location /private {{
        methods GET;
        root {private_root};
        autoindex off;
        upload_enable off;
    }}

    location /upload {{
        methods POST;
        root {root1};
        autoindex off;
        upload_enable on;
        upload_store {uploads};
    }}

    location /uploads {{
        methods GET DELETE;
        root {uploads};
        autoindex on;
        upload_enable on;
        upload_store {uploads};
    }}

    location /cgi-bin {{
        methods GET POST;
        root {cgi_root};
        autoindex off;
        upload_enable off;
        cgi .py /usr/bin/python3;
        cgi .sh /bin/sh;
    }}

    location /redirect-me {{
        methods GET;
        return 301 /;
    }}
}}

server {{
    listen {self.host}:{self.second_port};
    server_name regression_secondary;

    root {root2};
    index index.html;
    client_max_body_size 65536;

    error_page 400 {errors2}/400.html;
    error_page 403 {errors2}/403.html;
    error_page 404 {errors2}/404.html;
    error_page 405 {errors2}/405.html;
    error_page 413 {errors2}/413.html;
    error_page 500 {errors2}/500.html;
    error_page 501 {errors2}/501.html;
    error_page 502 {errors2}/502.html;
    error_page 504 {errors2}/504.html;

    location / {{
        methods GET;
        root {root2};
        index index.html;
        autoindex off;
        upload_enable off;
    }}
}}
"""
        self.write(self.config_path, config)


class RegressionSuite:
    def __init__(
        self,
        fixture: Fixture,
        server: WebservProcess,
        timeout: float,
        stress: bool,
    ) -> None:
        self.fixture = fixture
        self.server = server
        self.host = fixture.host
        self.port = fixture.port
        self.second_port = fixture.second_port
        self.timeout = timeout
        self.stress = stress
        self.results: List[TestResult] = []

    def run_case(
        self,
        name: str,
        category: str,
        expected: str,
        action: Callable[[], object],
    ) -> None:
        started = time.monotonic()
        snapshot: Dict[str, object] = {}
        passed = False
        actual = ""
        try:
            value = action()
            if isinstance(value, HttpResponse):
                snapshot = value.snapshot()
                actual = f"HTTP {value.status}, {len(value.body)} body bytes"
            elif isinstance(value, dict):
                snapshot = value
                actual = json.dumps(value, sort_keys=True)
            else:
                snapshot = {"value": value}
                actual = str(value)
            passed = True
        except Exception as exc:
            actual = f"{type(exc).__name__}: {exc}"
            snapshot = {"error": actual}
        duration = int((time.monotonic() - started) * 1000)
        self.results.append(
            TestResult(
                name=name,
                category=category,
                passed=passed,
                expected=expected,
                actual=actual,
                snapshot=snapshot,
                duration_ms=duration,
            )
        )

    def get(
        self,
        target: str,
        port: Optional[int] = None,
        version: str = "HTTP/1.1",
        headers: Optional[Dict[str, str]] = None,
        slow_read: bool = False,
    ) -> HttpResponse:
        used_port = port if port is not None else self.port
        payload = request_bytes(
            "GET", target, self.host, headers=headers, version=version
        )
        return exchange(
            self.host,
            used_port,
            payload,
            timeout=self.timeout,
            slow_read=slow_read,
        )

    def test_response(
        self,
        name: str,
        category: str,
        payload: bytes,
        expected_status: int,
        body_contains: Iterable[str] = (),
        headers: Optional[Dict[str, str]] = None,
        port: Optional[int] = None,
        timeout: Optional[float] = None,
    ) -> Callable[[], HttpResponse]:
        def action() -> HttpResponse:
            response = raw_exchange(
                self.host,
                self.port if port is None else port,
                payload,
                timeout=self.timeout if timeout is None else timeout,
            )
            expect_response(
                response,
                expected_status,
                body_contains=body_contains,
                headers=headers,
            )
            return response

        return action

    def run(self) -> List[TestResult]:
        host_header = self.host

        self.run_case(
            "primary server index",
            "static",
            "200 and primary marker",
            lambda: self._checked_get("/", 200, ("WEBSERV_REGRESSION_PORT_1",)),
        )
        self.run_case(
            "second listening port",
            "network",
            "200 and secondary marker",
            lambda: self._checked_get(
                "/",
                200,
                ("WEBSERV_REGRESSION_PORT_2",),
                port=self.second_port,
            ),
        )
        self.run_case(
            "static text file",
            "static",
            "200 and ALPHA_FILE",
            lambda: self._checked_get("/files/alpha.txt", 200, ("ALPHA_FILE",)),
        )
        self.run_case(
            "autoindex listing",
            "static",
            "200 with sorted file and directory entries",
            self._autoindex,
        )
        self.run_case(
            "directory listing disabled",
            "static",
            "403 custom error page",
            lambda: self._checked_get("/private/", 403, ("CUSTOM_ERROR_403",)),
        )
        self.run_case(
            "missing resource",
            "errors",
            "404 custom error page",
            lambda: self._checked_get("/does-not-exist", 404, ("CUSTOM_ERROR_404",)),
        )

        redirect_payload = request_bytes("GET", "/redirect-me", host_header)
        self.run_case(
            "HTTP redirect",
            "routing",
            "301 with Location: /",
            self.test_response(
                "HTTP redirect",
                "routing",
                redirect_payload,
                301,
                headers={"Location": "/"},
            ),
        )

        method_payload = request_bytes(
            "POST",
            "/",
            host_header,
            headers={"Content-Type": "text/plain"},
            body=b"x",
        )
        self.run_case(
            "method not allowed",
            "methods",
            "405 and Allow: GET",
            self.test_response(
                "method not allowed",
                "methods",
                method_payload,
                405,
                body_contains=("CUSTOM_ERROR_405",),
                headers={"Allow": "GET"},
            ),
        )

        patch_payload = request_bytes("PATCH", "/", host_header)
        self.run_case(
            "unknown method",
            "methods",
            "501 custom error page",
            self.test_response(
                "unknown method",
                "methods",
                patch_payload,
                501,
                body_contains=("CUSTOM_ERROR_501",),
            ),
        )

        self.run_case(
            "HTTP/1.0 request",
            "parser",
            "200 response",
            lambda: self._checked_get(
                "/", 200, ("WEBSERV_REGRESSION_PORT_1",), version="HTTP/1.0"
            ),
        )
        self.run_case(
            "request split across packets",
            "parser",
            "same 200 response after incremental input",
            self._split_request,
        )

        malformed = b"BROKEN_REQUEST\r\nHost: localhost\r\n\r\n"
        self.run_case(
            "malformed request line",
            "parser",
            "400 custom error page",
            self.test_response(
                "malformed request line",
                "parser",
                malformed,
                400,
                body_contains=("CUSTOM_ERROR_400",),
            ),
        )

        conflicting = (
            b"POST /cgi-bin/env.py HTTP/1.1\r\n"
            + b"Host: localhost\r\n"
            + b"Content-Length: 3\r\n"
            + b"Content-Length: 4\r\n"
            + b"Connection: close\r\n\r\n"
            + b"abcd"
        )
        self.run_case(
            "conflicting Content-Length",
            "parser",
            "400",
            self.test_response(
                "conflicting Content-Length",
                "parser",
                conflicting,
                400,
            ),
        )

        invalid_length = (
            b"POST /cgi-bin/env.py HTTP/1.1\r\n"
            + b"Host: localhost\r\n"
            + b"Content-Length: nope\r\n"
            + b"Connection: close\r\n\r\n"
        )
        self.run_case(
            "invalid Content-Length",
            "parser",
            "400",
            self.test_response(
                "invalid Content-Length",
                "parser",
                invalid_length,
                400,
            ),
        )

        oversized_body = b"X" * 65537
        oversized = request_bytes(
            "POST",
            "/upload/too-large.bin",
            host_header,
            headers={"Content-Type": "application/octet-stream"},
            body=oversized_body,
        )
        self.run_case(
            "client_max_body_size",
            "limits",
            "413 custom error page",
            self.test_response(
                "client_max_body_size",
                "limits",
                oversized,
                413,
                body_contains=("CUSTOM_ERROR_413",),
                timeout=max(self.timeout, 10.0),
            ),
        )

        self.run_case(
            "valid chunked CGI body",
            "chunked",
            "200; CGI receives decoded body",
            self._chunked_post,
        )
        self.run_case(
            "chunk extensions and trailer",
            "chunked",
            "200; extensions ignored and trailer accepted",
            self._chunked_extension,
        )

        bad_chunk = (
            b"POST /cgi-bin/env.py HTTP/1.1\r\n"
            + b"Host: localhost\r\n"
            + b"Transfer-Encoding: chunked\r\n"
            + b"Connection: close\r\n\r\n"
            + b"ZZ\r\nbroken\r\n0\r\n\r\n"
        )
        self.run_case(
            "malformed chunk size",
            "chunked",
            "400",
            self.test_response(
                "malformed chunk size",
                "chunked",
                bad_chunk,
                400,
            ),
        )

        ambiguous_body = (
            b"POST /cgi-bin/env.py HTTP/1.1\r\n"
            + b"Host: localhost\r\n"
            + b"Content-Length: 4\r\n"
            + b"Transfer-Encoding: chunked\r\n"
            + b"Connection: close\r\n\r\n"
            + b"0\r\n\r\n"
        )
        self.run_case(
            "Content-Length plus chunked",
            "chunked",
            "400",
            self.test_response(
                "Content-Length plus chunked",
                "chunked",
                ambiguous_body,
                400,
            ),
        )

        self.run_case(
            "CGI basic GET",
            "cgi",
            "200 and CGI output",
            lambda: self._checked_get(
                "/cgi-bin/hello.py", 200, ("HELLO_FROM_REGRESSION_CGI",)
            ),
        )
        self.run_case(
            "CGI shell type",
            "cgi",
            "200 and shell CGI output",
            lambda: self._checked_get(
                "/cgi-bin/hello.sh?mode=multi",
                200,
                (
                    "HELLO_FROM_SHELL_CGI",
                    "REQUEST_METHOD=GET",
                    "QUERY_STRING=mode=multi",
                ),
            ),
        )
        self.run_case(
            "CGI query and HTTP headers",
            "cgi",
            "query and custom header exported to environment",
            self._cgi_query_headers,
        )
        self.run_case(
            "CGI POST stdin",
            "cgi",
            "body and content metadata reach CGI",
            self._cgi_post,
        )
        self.run_case(
            "CGI PATH_INFO",
            "cgi",
            "SCRIPT_NAME and PATH_INFO are split",
            self._cgi_path_info,
        )
        self.run_case(
            "CGI output without headers",
            "cgi",
            "200 fallback and plain-text body",
            self._cgi_without_headers,
        )
        self.run_case(
            "missing CGI script",
            "cgi",
            "404",
            lambda: self._checked_get(
                "/cgi-bin/missing.py", 404, ("CUSTOM_ERROR_404",)
            ),
        )
        self.run_case(
            "unsupported CGI extension",
            "cgi",
            "403",
            lambda: self._checked_get(
                "/cgi-bin/not_cgi.txt", 403, ("CUSTOM_ERROR_403",)
            ),
        )
        self.run_case(
            "CGI Status header",
            "cgi",
            "HTTP status line becomes 201",
            self._cgi_status,
        )
        self.run_case(
            "CGI timeout",
            "cgi",
            "504 and server remains alive",
            self._cgi_timeout,
        )

        self.run_case(
            "raw upload, GET and DELETE",
            "upload",
            "201, exact stored bytes, 200 GET, 200 DELETE, then 404",
            self._upload_lifecycle,
        )
        self.run_case(
            "encoded path traversal",
            "security",
            "403",
            lambda: self._checked_get(
                "/files/%2e%2e/index.html", 403, ("CUSTOM_ERROR_403",)
            ),
        )
        self.run_case(
            "large partial response",
            "network",
            "complete body while client reads slowly",
            self._large_slow_read,
        )
        self.run_case(
            "disconnect during response",
            "resilience",
            "server survives immediate client disconnect",
            self._disconnect_during_response,
        )
        self.run_case(
            "slow incomplete client",
            "resilience",
            "one incomplete request does not block other clients",
            self._slow_client_does_not_block,
        )
        self.run_case(
            "slow CGI does not block event loop",
            "resilience",
            "normal GET completes while CGI sleeps",
            self._slow_cgi_non_blocking,
        )
        self.run_case(
            "parallel clients",
            "stress",
            "50/50 deterministic responses",
            lambda: self._parallel_requests(50),
        )

        if self.stress:
            self.run_case(
                "extended parallel stress",
                "stress",
                "500/500 deterministic responses",
                lambda: self._parallel_requests(500),
            )

        self.run_case(
            "final health check",
            "resilience",
            "server still alive and returns 200",
            self._final_health,
        )

        return self.results

    def _checked_get(
        self,
        target: str,
        status: int,
        fragments: Iterable[str] = (),
        port: Optional[int] = None,
        version: str = "HTTP/1.1",
    ) -> HttpResponse:
        response = self.get(target, port=port, version=version)
        expect_response(response, status, fragments)
        return response

    def _autoindex(self) -> HttpResponse:
        response = self.get("/files/")
        expect_response(response, 200, ("alpha.txt", "nested/"))
        text = response.text()
        ensure(
            text.find("nested/") < text.find("alpha.txt"),
            "directories should be listed before files",
        )
        return response

    def _split_request(self) -> HttpResponse:
        parts = [
            b"GET /files/alpha.txt HTTP/1.1\r\n",
            b"Host: localhost\r\n",
            b"Connection: close\r\n",
            b"\r\n",
        ]
        response = exchange(
            self.host,
            self.port,
            b"",
            timeout=self.timeout,
            send_parts=parts,
        )
        expect_response(response, 200, ("ALPHA_FILE",))
        return response

    def _chunked_post(self) -> HttpResponse:
        payload = (
            b"POST /cgi-bin/env.py?mode=chunked HTTP/1.1\r\n"
            + b"Host: localhost\r\n"
            + b"Content-Type: text/plain\r\n"
            + b"Transfer-Encoding: chunked\r\n"
            + b"Connection: close\r\n\r\n"
            + b"6\r\nhello \r\n"
            + b"7\r\nchunked\r\n"
            + b"0\r\n\r\n"
        )
        response = raw_exchange(self.host, self.port, payload, timeout=self.timeout)
        expect_response(
            response,
            200,
            (
                "REQUEST_METHOD=POST",
                "QUERY_STRING=mode=chunked",
                "BODY:\nhello chunked",
            ),
        )
        return response

    def _chunked_extension(self) -> HttpResponse:
        payload = (
            b"POST /cgi-bin/env.py HTTP/1.1\r\n"
            + b"Host: localhost\r\n"
            + b"Content-Type: text/plain\r\n"
            + b"Transfer-Encoding: chunked\r\n"
            + b"Connection: close\r\n\r\n"
            + b"4;name=value\r\ntest\r\n"
            + b"0\r\nX-Trailer: accepted\r\n\r\n"
        )
        response = raw_exchange(self.host, self.port, payload, timeout=self.timeout)
        expect_response(response, 200, ("BODY:\ntest",))
        return response

    def _cgi_query_headers(self) -> HttpResponse:
        response = self.get(
            "/cgi-bin/env.py?x=42",
            headers={
                "X-Test-Header": "regression",
                "User-Agent": "webserv-regression-tester",
            },
        )
        expect_response(
            response,
            200,
            (
                "REQUEST_METHOD=GET",
                "QUERY_STRING=x=42",
                "HTTP_X_TEST_HEADER=regression",
                "HTTP_USER_AGENT=webserv-regression-tester",
                "SERVER_PORT=" + str(self.port),
            ),
        )
        return response

    def _cgi_post(self) -> HttpResponse:
        body = b"name=tigran&mode=regression"
        payload = request_bytes(
            "POST",
            "/cgi-bin/env.py?source=tester",
            self.host,
            headers={"Content-Type": "application/x-www-form-urlencoded"},
            body=body,
        )
        response = raw_exchange(self.host, self.port, payload, timeout=self.timeout)
        expect_response(
            response,
            200,
            (
                "REQUEST_METHOD=POST",
                "QUERY_STRING=source=tester",
                "CONTENT_TYPE=application/x-www-form-urlencoded",
                "CONTENT_LENGTH=" + str(len(body)),
                "BODY:\n" + body.decode("ascii"),
            ),
        )
        return response

    def _cgi_path_info(self) -> HttpResponse:
        response = self.get("/cgi-bin/env.py/extra/path?x=42")
        expect_response(
            response,
            200,
            (
                "SCRIPT_NAME=/cgi-bin/env.py",
                "PATH_INFO=/extra/path",
                "QUERY_STRING=x=42",
            ),
        )
        return response

    def _cgi_without_headers(self) -> HttpResponse:
        response = self.get("/cgi-bin/no_headers.py")
        expect_response(
            response,
            200,
            ("CGI_WITHOUT_HEADERS",),
            headers={"Content-Type": "text/plain"},
        )
        return response

    def _cgi_status(self) -> HttpResponse:
        response = self.get("/cgi-bin/status.py")
        expect_response(response, 201, ("CGI_CUSTOM_STATUS",))
        return response

    def _cgi_timeout(self) -> HttpResponse:
        payload = request_bytes("GET", "/cgi-bin/timeout.py", self.host)
        response = raw_exchange(
            self.host,
            self.port,
            payload,
            timeout=max(self.timeout, 15.0),
        )
        expect_response(response, 504, ("CUSTOM_ERROR_504",))
        ensure(self.server.alive(), "server exited after CGI timeout")
        health = self.get("/")
        expect_response(health, 200, ("WEBSERV_REGRESSION_PORT_1",))
        return response

    def _upload_lifecycle(self) -> Dict[str, object]:
        name = "regression_upload.bin"
        body = b"\x00WEBServ upload regression\xff\n"
        target_file = self.fixture.uploads / name
        if target_file.exists():
            target_file.unlink()

        upload = request_bytes(
            "POST",
            "/upload/" + name,
            self.host,
            headers={"Content-Type": "application/octet-stream"},
            body=body,
        )
        upload_response = raw_exchange(
            self.host, self.port, upload, timeout=self.timeout
        )
        expect_response(upload_response, 201)
        ensure(target_file.exists(), "uploaded file was not created")
        ensure(target_file.read_bytes() == body, "uploaded file bytes differ")

        get_response = self.get("/uploads/" + name)
        expect_response(get_response, 200)
        ensure(get_response.body == body, "GET body differs from uploaded bytes")

        delete_payload = request_bytes("DELETE", "/uploads/" + name, self.host)
        delete_response = raw_exchange(
            self.host, self.port, delete_payload, timeout=self.timeout
        )
        expect_response(delete_response, 200)
        ensure(not target_file.exists(), "file still exists after DELETE")

        second_delete = raw_exchange(
            self.host, self.port, delete_payload, timeout=self.timeout
        )
        expect_response(second_delete, 404)

        return {
            "upload_status": upload_response.status,
            "stored_sha256": hashlib.sha256(body).hexdigest(),
            "get_status": get_response.status,
            "get_sha256": hashlib.sha256(get_response.body).hexdigest(),
            "delete_status": delete_response.status,
            "second_delete_status": second_delete.status,
        }

    def _large_slow_read(self) -> HttpResponse:
        response = self.get("/large.bin", slow_read=True)
        expect_response(response, 200)
        ensure(
            response.body == self.fixture.large_content,
            "large response body was truncated or corrupted",
        )
        return response

    def _disconnect_during_response(self) -> Dict[str, object]:
        payload = request_bytes("GET", "/large.bin", self.host)
        sock = socket.create_connection((self.host, self.port), timeout=self.timeout)
        sock.sendall(payload)
        sock.close()
        time.sleep(0.2)
        ensure(self.server.alive(), "server exited after client disconnect")
        response = self.get("/")
        expect_response(response, 200, ("WEBSERV_REGRESSION_PORT_1",))
        return {
            "server_alive": self.server.alive(),
            "health_status": response.status,
        }

    def _slow_client_does_not_block(self) -> Dict[str, object]:
        slow = socket.create_connection((self.host, self.port), timeout=self.timeout)
        try:
            slow.sendall(
                b"GET / HTTP/1.1\r\n"
                + b"Host: localhost\r\n"
                + b"X-Incomplete: still-waiting"
            )
            started = time.monotonic()
            response = self.get("/")
            elapsed = time.monotonic() - started
            expect_response(response, 200, ("WEBSERV_REGRESSION_PORT_1",))
            ensure(elapsed < 1.5, f"normal request was delayed for {elapsed:.2f}s")
            return {
                "health_status": response.status,
                "completed_without_blocking": True,
            }
        finally:
            slow.close()

    def _slow_cgi_non_blocking(self) -> Dict[str, object]:
        holder: Dict[str, object] = {}

        def run_slow() -> None:
            try:
                holder["response"] = self.get("/cgi-bin/slow.py")
            except Exception as exc:
                holder["error"] = repr(exc)

        thread = threading.Thread(target=run_slow)
        thread.start()
        time.sleep(0.25)

        started = time.monotonic()
        fast = self.get("/")
        fast_elapsed = time.monotonic() - started
        expect_response(fast, 200, ("WEBSERV_REGRESSION_PORT_1",))
        ensure(
            fast_elapsed < 1.5,
            f"static request waited {fast_elapsed:.2f}s for slow CGI",
        )

        thread.join(timeout=5.0)
        ensure(not thread.is_alive(), "slow CGI request did not finish")
        ensure("error" not in holder, "slow CGI failed: " + str(holder.get("error")))
        slow = holder.get("response")
        ensure(isinstance(slow, HttpResponse), "slow CGI returned no response")
        expect_response(slow, 200, ("SLOW_CGI_FINISHED",))

        return {
            "fast_status": fast.status,
            "fast_completed_without_blocking": True,
            "slow_status": slow.status,
        }

    def _parallel_requests(self, count: int) -> Dict[str, object]:
        targets = ["/", "/files/alpha.txt", "/cgi-bin/hello.py"]

        def worker(index: int) -> Tuple[int, str]:
            target = targets[index % len(targets)]
            response = self.get(target)
            if target == "/":
                expect_response(response, 200, ("WEBSERV_REGRESSION_PORT_1",))
            elif target == "/files/alpha.txt":
                expect_response(response, 200, ("ALPHA_FILE",))
            else:
                expect_response(response, 200, ("HELLO_FROM_REGRESSION_CGI",))
            return response.status, hashlib.sha256(response.body).hexdigest()

        failures: List[str] = []
        results: List[Tuple[int, str]] = []
        workers = min(32, max(4, count))
        with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
            futures = [executor.submit(worker, index) for index in range(count)]
            for future in concurrent.futures.as_completed(futures):
                try:
                    results.append(future.result())
                except Exception as exc:
                    failures.append(repr(exc))

        ensure(not failures, f"{len(failures)} parallel requests failed: {failures[:3]}")
        ensure(len(results) == count, f"only {len(results)}/{count} requests completed")
        ensure(self.server.alive(), "server exited during parallel test")
        return {
            "requested": count,
            "completed": len(results),
            "failed": len(failures),
            "statuses": sorted(set(status for status, _ in results)),
        }

    def _final_health(self) -> HttpResponse:
        ensure(self.server.alive(), "webserv process is no longer running")
        response = self.get("/")
        expect_response(response, 200, ("WEBSERV_REGRESSION_PORT_1",))
        return response


def run_build(project_root: Path, check_relink: bool) -> None:
    print("Building webserv...")
    subprocess.run(["make"], cwd=str(project_root), check=True)
    binary = project_root / "webserv"
    if not binary.exists():
        raise RuntimeError("make succeeded but ./webserv does not exist")

    if check_relink:
        before = binary.stat().st_mtime_ns
        time.sleep(0.02)
        subprocess.run(["make"], cwd=str(project_root), check=True)
        after = binary.stat().st_mtime_ns
        if before != after:
            raise RuntimeError("second make changed ./webserv: unnecessary relink detected")
        print("No unnecessary relink detected.")


def baseline_payload(results: List[TestResult]) -> Dict[str, object]:
    return {
        "format": 1,
        "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "tests": {
            result.name: {
                "passed": result.passed,
                "snapshot": result.snapshot,
            }
            for result in results
        },
    }


def compare_baseline(
    baseline: Dict[str, object],
    results: List[TestResult],
) -> List[str]:
    differences: List[str] = []
    old_tests = baseline.get("tests", {})
    if not isinstance(old_tests, dict):
        return ["baseline has invalid 'tests' object"]

    current = {
        result.name: {
            "passed": result.passed,
            "snapshot": result.snapshot,
        }
        for result in results
    }

    for name in sorted(set(old_tests) | set(current)):
        if name not in old_tests:
            differences.append(f"NEW TEST: {name}")
            continue
        if name not in current:
            differences.append(f"MISSING TEST: {name}")
            continue
        old_value = old_tests[name]
        new_value = current[name]
        if old_value != new_value:
            differences.append(
                f"CHANGED: {name}\n"
                f"  before: {json.dumps(old_value, sort_keys=True)}\n"
                f"  after:  {json.dumps(new_value, sort_keys=True)}"
            )
    return differences


def print_summary(
    results: List[TestResult],
    use_colour: bool,
    differences: Optional[List[str]] = None,
) -> None:
    print()
    for result in results:
        print(status_line(result, use_colour))
        if not result.passed:
            print(f"       expected: {result.expected}")
            print(f"       actual:   {result.actual}")

    passed = sum(result.passed for result in results)
    failed = len(results) - passed
    print()
    print(f"Functional checks: {passed}/{len(results)} passed, {failed} failed")

    if differences is not None:
        print(f"Regression comparison: {len(differences)} difference(s)")
        for difference in differences:
            print("  - " + difference.replace("\n", "\n    "))


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Functional and regression tester for the Webserv project"
    )
    parser.add_argument(
        "mode",
        choices=("record", "compare", "check"),
        help="record baseline, compare with baseline, or only run assertions",
    )
    parser.add_argument(
        "--project-root",
        default=".",
        help="Webserv repository root (default: current directory)",
    )
    parser.add_argument(
        "--binary",
        default="./webserv",
        help="path to webserv binary relative to project root",
    )
    parser.add_argument(
        "--baseline",
        default="tests/baselines/webserv_before.json",
        help="baseline JSON path relative to project root",
    )
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--second-port", type=int, default=DEFAULT_SECOND_PORT)
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_SOCKET_TIMEOUT,
        help="normal socket timeout in seconds",
    )
    parser.add_argument(
        "--build",
        action="store_true",
        help="run make before testing",
    )
    parser.add_argument(
        "--check-relink",
        action="store_true",
        help="run make twice and ensure ./webserv is not relinked",
    )
    parser.add_argument(
        "--stress",
        action="store_true",
        help="also run the extended 500-request stress test",
    )
    parser.add_argument(
        "--keep-runtime",
        action="store_true",
        help="keep generated fixtures and server log",
    )
    parser.add_argument(
        "--no-colour",
        action="store_true",
        help="disable ANSI colours",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    project_root = Path(args.project_root).resolve()
    binary = Path(args.binary)
    if not binary.is_absolute():
        binary = (project_root / binary).resolve()
    baseline_path = Path(args.baseline)
    if not baseline_path.is_absolute():
        baseline_path = (project_root / baseline_path).resolve()
    runtime_root = project_root / RUNTIME_DIRECTORY

    if args.port == args.second_port:
        print("--port and --second-port must be different", file=sys.stderr)
        return 2

    try:
        if args.build or args.check_relink:
            run_build(project_root, args.check_relink)

        if not binary.exists():
            raise RuntimeError(
                f"binary not found: {binary}; run make or pass --build"
            )

        fixture = Fixture(
            project_root,
            runtime_root,
            args.host,
            args.port,
            args.second_port,
        )
        fixture.prepare()

        server = WebservProcess(
            project_root=project_root,
            binary=binary,
            config_path=fixture.config_path,
            host=args.host,
            ports=(args.port, args.second_port),
            log_path=runtime_root / "server.log",
        )

        with server:
            suite = RegressionSuite(
                fixture=fixture,
                server=server,
                timeout=args.timeout,
                stress=args.stress,
            )
            results = suite.run()

        differences: Optional[List[str]] = None
        if args.mode == "record":
            baseline_path.parent.mkdir(parents=True, exist_ok=True)
            baseline_path.write_text(
                json.dumps(baseline_payload(results), indent=2, sort_keys=True),
                encoding="utf-8",
            )
            print(f"\nBaseline written to: {baseline_path}")
        elif args.mode == "compare":
            if not baseline_path.exists():
                raise RuntimeError(f"baseline does not exist: {baseline_path}")
            baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
            differences = compare_baseline(baseline, results)

        use_colour = sys.stdout.isatty() and not args.no_colour
        print_summary(results, use_colour, differences=differences)

        failed_checks = any(not result.passed for result in results)
        changed = differences is not None and bool(differences)

        if not args.keep_runtime:
            shutil.rmtree(runtime_root, ignore_errors=True)
        else:
            print(f"Runtime files kept at: {runtime_root}")

        if failed_checks or changed:
            return 1
        return 0

    except KeyboardInterrupt:
        print("\nInterrupted.", file=sys.stderr)
        return 130
    except Exception as exc:
        print(f"\nTester error: {type(exc).__name__}: {exc}", file=sys.stderr)
        log_path = runtime_root / "server.log"
        if log_path.exists():
            print(f"Server log: {log_path}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
