#!/usr/bin/env python3

import argparse
import socket
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


# ============================================================
# Colors
# ============================================================

GREEN = "\033[32m"
RED = "\033[31m"
YELLOW = "\033[33m"
CYAN = "\033[36m"
BOLD = "\033[1m"
RESET = "\033[0m"


# ============================================================
# Test model
# ============================================================

@dataclass
class TestCase:
    name: str
    parts: list[bytes]
    expected_status: int
    expected_file_content: Optional[bytes] = None
    file_must_remain_unchanged: bool = False
    description: str = ""


@dataclass
class TestResult:
    name: str
    passed: bool
    details: list[str]


# ============================================================
# HTTP helpers
# ============================================================

def build_headers(
    host: str,
    port: int,
    path: str,
    version: str = "HTTP/1.1",
    extra_headers: Optional[list[str]] = None,
) -> bytes:
    lines = [
        f"POST {path} {version}",
        f"Host: {host}:{port}",
    ]

    if extra_headers:
        lines.extend(extra_headers)

    lines.append("Connection: close")
    lines.append("")
    lines.append("")

    return "\r\n".join(lines).encode()


def send_request(
    host: str,
    port: int,
    parts: list[bytes],
    delay: float,
    timeout: float,
) -> tuple[bytes, Optional[str]]:
    response = b""
    connection_error = None

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)

    try:
        sock.connect((host, port))

        for index, part in enumerate(parts):
            print(
                f"    send {index + 1:02d}/{len(parts):02d}: "
                f"{part!r}"
            )

            try:
                sock.sendall(part)
            except BrokenPipeError:
                connection_error = (
                    "Server closed the connection before "
                    "all request parts were sent"
                )
                break
            except ConnectionResetError:
                connection_error = (
                    "Server reset the connection before "
                    "all request parts were sent"
                )
                break

            if delay > 0:
                time.sleep(delay)

        while True:
            try:
                data = sock.recv(4096)
            except socket.timeout:
                connection_error = (
                    connection_error
                    or "Timed out while waiting for response"
                )
                break
            except ConnectionResetError:
                break

            if not data:
                break

            response += data

    except ConnectionRefusedError:
        connection_error = (
            f"Connection refused on {host}:{port}. "
            "Is webserv running?"
        )
    except OSError as error:
        connection_error = str(error)
    finally:
        sock.close()

    return response, connection_error


def get_status_code(response: bytes) -> Optional[int]:
    if not response:
        return None

    first_line = response.split(b"\r\n", 1)[0]

    try:
        parts = first_line.decode("ascii", errors="replace").split()
        if len(parts) < 2:
            return None
        return int(parts[1])
    except (ValueError, IndexError):
        return None


def get_response_body(response: bytes) -> bytes:
    separator = b"\r\n\r\n"
    position = response.find(separator)

    if position != -1:
        return response[position + len(separator):]

    separator = b"\n\n"
    position = response.find(separator)

    if position != -1:
        return response[position + len(separator):]

    return b""


# ============================================================
# File helpers
# ============================================================

def read_file(path: Path) -> Optional[bytes]:
    try:
        return path.read_bytes()
    except FileNotFoundError:
        return None
    except OSError as error:
        print(f"{YELLOW}Cannot read {path}: {error}{RESET}")
        return None


def wait_for_file(
    path: Path,
    expected_content: bytes,
    timeout: float = 1.0,
) -> Optional[bytes]:
    deadline = time.time() + timeout
    last_content = read_file(path)

    while time.time() < deadline:
        last_content = read_file(path)

        if last_content == expected_content:
            return last_content

        time.sleep(0.05)

    return last_content


def display_bytes(data: Optional[bytes]) -> str:
    if data is None:
        return "<file does not exist>"

    if not data:
        return "<empty>"

    result = []

    for byte in data:
        if byte == 13:
            result.append("^M")
        elif byte == 10:
            result.append("$\n")
        elif byte == 9:
            result.append("^I")
        elif 32 <= byte <= 126:
            result.append(chr(byte))
        else:
            result.append(f"\\x{byte:02x}")

    return "".join(result)


def display_hex(data: Optional[bytes]) -> str:
    if data is None:
        return "<none>"

    if not data:
        return "<empty>"

    return " ".join(f"{byte:02x}" for byte in data)


def print_file_state(path: Path, content: Optional[bytes]) -> None:
    print(f"    file: {path}")

    if content is None:
        print("    size: <file does not exist>")
        print("    data: <file does not exist>")
        return

    print(f"    size: {len(content)} bytes")
    print(f"    data: {display_bytes(content)!r}")
    print(f"    hex : {display_hex(content)}")


# ============================================================
# Tests
# ============================================================

def create_tests(host: str, port: int, path: str) -> list[TestCase]:
    normal_headers = build_headers(
        host,
        port,
        path,
        extra_headers=[
            "Transfer-Encoding: chunked",
            "Content-Type: text/plain",
        ],
    )

    tests = [
        TestCase(
            name="Basic fragmented chunks",
            description=(
                "Multiple chunks split across several send() calls"
            ),
            parts=[
                normal_headers,
                b"4\r\nWi",
                b"ki\r\n",
                b"5\r\nped",
                b"ia\r\n",
                b"0\r\n",
                b"\r\n",
            ],
            expected_status=201,
            expected_file_content=b"Wikipedia",
        ),

        TestCase(
            name="Hexadecimal chunk size",
            description="A hexadecimal chunk size must mean 10 bytes",
            parts=[
                normal_headers,
                b"A\r\n01234",
                b"56789\r\n",
                b"0\r\n",
                b"\r\n",
            ],
            expected_status=201,
            expected_file_content=b"0123456789",
        ),

        TestCase(
            name="Trailers",
            description="Trailer headers must not enter the decoded body",
            parts=[
                normal_headers,
                b"5\r\nHello\r\n",
                b"0\r\n",
                b"X-Test: value\r\n",
                b"X-Second: trailer\r\n",
                b"\r\n",
            ],
            expected_status=201,
            expected_file_content=b"Hello",
        ),

        TestCase(
            name="Split chunk-size and CRLF",
            description=(
                "Chunk size and CRLF delimiters are split "
                "across TCP writes"
            ),
            parts=[
                normal_headers,
                b"A",
                b"\r",
                b"\n0123",
                b"456789\r",
                b"\n0\r",
                b"\n\r",
                b"\n",
            ],
            expected_status=201,
            expected_file_content=b"0123456789",
        ),

        TestCase(
            name="Chunk extension",
            description="Chunk extensions may be ignored",
            parts=[
                normal_headers,
                b"5;name=value\r\n",
                b"Hello\r\n",
                b"0\r\n\r\n",
            ],
            expected_status=201,
            expected_file_content=b"Hello",
        ),

        TestCase(
            name="Case-insensitive Transfer-Encoding",
            description="Header name and value are case-insensitive",
            parts=[
                build_headers(
                    host,
                    port,
                    path,
                    extra_headers=[
                        "tRaNsFeR-EnCoDiNg: ChUnKeD",
                        "Content-Type: text/plain",
                    ],
                ),
                b"5\r\nHello\r\n",
                b"0\r\n\r\n",
            ],
            expected_status=201,
            expected_file_content=b"Hello",
        ),

        TestCase(
            name="Empty chunked body",
            description="A zero chunk immediately completes an empty body",
            parts=[
                normal_headers,
                b"0\r\n",
                b"\r\n",
            ],
            expected_status=201,
            expected_file_content=b"",
        ),

        TestCase(
            name="Invalid hexadecimal size",
            description="XYZ is not a hexadecimal chunk size",
            parts=[
                normal_headers,
                b"XYZ\r\nHello\r\n",
                b"0\r\n\r\n",
            ],
            expected_status=400,
            file_must_remain_unchanged=True,
        ),

        TestCase(
            name="Missing CRLF after chunk data",
            description=(
                "Every chunk-data section must be followed by CRLF"
            ),
            parts=[
                normal_headers,
                b"5\r\nHelloXX",
                b"0\r\n\r\n",
            ],
            expected_status=400,
            file_must_remain_unchanged=True,
        ),

        TestCase(
            name="Content-Length and Transfer-Encoding conflict",
            description=(
                "Ambiguous message framing must be rejected"
            ),
            parts=[
                build_headers(
                    host,
                    port,
                    path,
                    extra_headers=[
                        "Content-Length: 5",
                        "Transfer-Encoding: chunked",
                        "Content-Type: text/plain",
                    ],
                ),
                b"5\r\nHello\r\n",
                b"0\r\n\r\n",
            ],
            expected_status=400,
            file_must_remain_unchanged=True,
        ),

        TestCase(
            name="HTTP/1.0 chunked request",
            description=(
                "Transfer-Encoding is not valid HTTP/1.0 framing"
            ),
            parts=[
                build_headers(
                    host,
                    port,
                    path,
                    version="HTTP/1.0",
                    extra_headers=[
                        "Transfer-Encoding: chunked",
                    ],
                ),
                b"5\r\nHello\r\n",
                b"0\r\n\r\n",
            ],
            expected_status=400,
            file_must_remain_unchanged=True,
        ),

        TestCase(
            name="Unsupported Transfer-Encoding",
            description="Unsupported transfer coding should return 501",
            parts=[
                build_headers(
                    host,
                    port,
                    path,
                    extra_headers=[
                        "Transfer-Encoding: gzip",
                    ],
                ),
                b"Hello",
            ],
            expected_status=501,
            file_must_remain_unchanged=True,
        ),

        TestCase(
            name="Chunk-size overflow",
            description="An overflowing hexadecimal size must be rejected",
            parts=[
                normal_headers,
                b"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n",
            ],
            expected_status=400,
            file_must_remain_unchanged=True,
        ),
    ]

    return tests


def create_body_limit_test(
    host: str,
    port: int,
    path: str,
    configured_limit: int,
) -> TestCase:
    body = b"A" * (configured_limit + 1)
    chunk_size = f"{len(body):X}".encode()

    headers = build_headers(
        host,
        port,
        path,
        extra_headers=[
            "Transfer-Encoding: chunked",
            "Content-Type: application/octet-stream",
        ],
    )

    return TestCase(
        name="client_max_body_size",
        description=(
            f"Decoded body exceeds configured limit "
            f"of {configured_limit} bytes"
        ),
        parts=[
            headers,
            chunk_size + b"\r\n",
            body,
            b"\r\n0\r\n\r\n",
        ],
        expected_status=413,
        file_must_remain_unchanged=True,
    )


# ============================================================
# Test runner
# ============================================================

def run_test(
    test: TestCase,
    host: str,
    port: int,
    upload_file: Path,
    delay: float,
    timeout: float,
) -> TestResult:
    details = []
    passed = True

    before_content = read_file(upload_file)

    print()
    print(f"{BOLD}{CYAN}TEST: {test.name}{RESET}")

    if test.description:
        print(f"  {test.description}")

    response, connection_error = send_request(
        host=host,
        port=port,
        parts=test.parts,
        delay=delay,
        timeout=timeout,
    )

    status_code = get_status_code(response)
    response_body = get_response_body(response)

    print(f"    expected status: {test.expected_status}")
    print(f"    actual status  : {status_code}")

    if response:
        first_line = response.split(b"\r\n", 1)[0]
        print(
            "    response line  : "
            + first_line.decode(errors="replace")
        )
    else:
        print("    response line  : <empty response>")

    if connection_error:
        print(f"    socket warning : {connection_error}")

    if status_code != test.expected_status:
        passed = False
        details.append(
            f"Expected HTTP {test.expected_status}, "
            f"received {status_code}"
        )

    if response_body:
        print(
            "    response body  : "
            + response_body.decode(errors="replace").strip()
        )

    if test.expected_file_content is not None:
        after_content = wait_for_file(
            upload_file,
            test.expected_file_content,
        )

        print_file_state(upload_file, after_content)

        if after_content != test.expected_file_content:
            passed = False
            details.append(
                "Uploaded file content differs from expected body"
            )

            print(
                f"    expected data  : "
                f"{display_bytes(test.expected_file_content)!r}"
            )
            print(
                f"    expected hex   : "
                f"{display_hex(test.expected_file_content)}"
            )

    elif test.file_must_remain_unchanged:
        time.sleep(0.1)
        after_content = read_file(upload_file)

        print("    checking that upload file was not modified")
        print_file_state(upload_file, after_content)

        if after_content != before_content:
            passed = False
            details.append(
                "Invalid request modified the upload file"
            )

            print(
                f"    previous data  : "
                f"{display_bytes(before_content)!r}"
            )

    if passed:
        print(f"{GREEN}  PASS{RESET}")
    else:
        print(f"{RED}  FAIL{RESET}")

        for detail in details:
            print(f"    - {detail}")

    return TestResult(
        name=test.name,
        passed=passed,
        details=details,
    )


def print_summary(results: list[TestResult]) -> None:
    passed_count = sum(1 for result in results if result.passed)
    failed_count = len(results) - passed_count

    print()
    print("=" * 72)
    print(f"{BOLD}CHUNKED TEST SUMMARY{RESET}")
    print("=" * 72)

    for result in results:
        marker = f"{GREEN}PASS{RESET}" if result.passed else f"{RED}FAIL{RESET}"
        print(f"[{marker}] {result.name}")

        if not result.passed:
            for detail in result.details:
                print(f"       {detail}")

    print("-" * 72)
    print(f"Passed: {passed_count}/{len(results)}")
    print(f"Failed: {failed_count}/{len(results)}")

    if failed_count == 0:
        print(f"{GREEN}{BOLD}ALL TESTS PASSED{RESET}")
    else:
        print(f"{RED}{BOLD}SOME TESTS FAILED{RESET}")


# ============================================================
# Arguments
# ============================================================

def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Regression tests for HTTP/1.1 "
            "Transfer-Encoding: chunked"
        )
    )

    parser.add_argument(
        "--host",
        default="127.0.0.1",
        help="Webserv host, default: 127.0.0.1",
    )

    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        help="Webserv port, default: 8080",
    )

    parser.add_argument(
        "--path",
        default="/uploads",
        help="Upload route, default: /uploads",
    )

    parser.add_argument(
        "--upload-file",
        default="www/uploads/uploads",
        help=(
            "File written by the upload handler, "
            "default: www/uploads/uploads"
        ),
    )

    parser.add_argument(
        "--delay",
        type=float,
        default=0.05,
        help=(
            "Delay between send() calls in seconds, "
            "default: 0.05"
        ),
    )

    parser.add_argument(
        "--timeout",
        type=float,
        default=3.0,
        help="Socket timeout in seconds, default: 3",
    )

    parser.add_argument(
        "--body-limit",
        type=int,
        default=None,
        help=(
            "Configured client_max_body_size. "
            "When supplied, a 413 test is added."
        ),
    )

    return parser.parse_args()


# ============================================================
# Main
# ============================================================

def main() -> int:
    args = parse_arguments()
    upload_file = Path(args.upload_file)

    print("=" * 72)
    print(f"{BOLD}TRANSFER-ENCODING: CHUNKED TEST SUITE{RESET}")
    print("=" * 72)
    print(f"Server      : {args.host}:{args.port}")
    print(f"Route       : {args.path}")
    print(f"Upload file : {upload_file}")
    print(f"Send delay  : {args.delay}s")
    print(f"Timeout     : {args.timeout}s")

    tests = create_tests(
        host=args.host,
        port=args.port,
        path=args.path,
    )

    if args.body_limit is not None:
        if args.body_limit < 0:
            print(
                f"{RED}--body-limit cannot be negative{RESET}"
            )
            return 1

        tests.append(
            create_body_limit_test(
                host=args.host,
                port=args.port,
                path=args.path,
                configured_limit=args.body_limit,
            )
        )

    results = []

    for test in tests:
        result = run_test(
            test=test,
            host=args.host,
            port=args.port,
            upload_file=upload_file,
            delay=args.delay,
            timeout=args.timeout,
        )
        results.append(result)

    print_summary(results)

    if all(result.passed for result in results):
        return 0

    return 1


if __name__ == "__main__":
    sys.exit(main())