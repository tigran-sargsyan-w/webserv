# Client Request Timeout Test Cases

This document contains manual and scripted test cases for client request timeout handling in the `webserv` project.

The goal is to verify that stalled or malicious clients cannot keep connections open indefinitely, while the server remains non-blocking and continues serving other clients normally.

This feature is separate from CGI timeout handling. CGI scripts are covered by `CgiTimeoutHandler` and `docs/cgi-tests.md`. Client request timeout applies to inactive client connections during request reading, body discarding, or response sending.

Invalid `client_timeout` values are covered in `docs/config-validator-tests.md` (sections 9b and 9c).

Covered features:

* per-client `lastActivity` timestamp storage;
* inactivity detection in the main `poll()` loop;
* safe client disconnection through `closeAndRemoveFd()`;
* configurable `client_timeout` server directive;
* `client_timeout` validation;
* timeout logging for debugging;
* slow or incomplete client regression tests.

---

## 1. Why this matters

The subject requires that a request to the server should never hang indefinitely.

A client that:

* connects and sends nothing;
* sends only part of the request headers;
* stops sending data in the middle of a request body;

must not keep a server connection open forever.

The server should:

* detect inactivity based on the configured timeout;
* close the stalled client safely;
* keep the event loop responsive;
* continue serving other clients normally.

---

## 2. Preparation

### Build the project

From the project root:

```bash
make re
```

### Configuration files used by these tests

| File | Purpose |
| ---- | ------- |
| `configs/default.conf` | production-like config with `client_timeout 30;` |
| `configs/test-client-timeout.conf` | short timeout (`5s`) on port `8098` for faster tests |
| `configs/invalid/client-timeout-zero.conf` | invalid config used to verify validator rejection |

### Default timeout values

| Setting | Value | Where |
| ------- | ----: | ----- |
| `client_timeout` in `default.conf` | `30` seconds | configurable per `server {}` block |
| fallback if `serverIndex` is invalid | `30` seconds | `CLIENT_TIMEOUT_SECONDS` in `ClientTimeoutHandler.hpp` |
| CGI timeout | `10` seconds | `CGI_TIMEOUT_SECONDS` in `CgiTimeoutHandler.hpp` |

Clients with an active CGI session are not closed by the client timeout handler. They are handled by the CGI timeout logic instead.

---

## 3. Test: server starts with valid `client_timeout`

Purpose: verify that a valid timeout value is accepted.

Run:

```bash
./webserv configs/default.conf
```

Expected server output should include something similar to:

```txt
Listening on 127.0.0.1:8080
WebServ run called!
```

Then verify normal behavior:

```bash
curl -i http://127.0.0.1:8080/
```

Expected:

```http
HTTP/1.1 200 OK
```

Purpose: confirms that adding `client_timeout` did not break normal server startup.

---

## 4. Test: stalled incomplete client is closed

Purpose: verify that a client sending an incomplete request is disconnected after the configured timeout.

### Start the short-timeout test server

Terminal 1:

```bash
./webserv configs/test-client-timeout.conf
```

The server should listen on:

```txt
127.0.0.1:8098
```

Configured timeout:

```txt
client_timeout 5;
```

### Send an incomplete request and wait

Terminal 2:

```bash
python3 -c "
import socket, time
s = socket.create_connection(('127.0.0.1', 8098))
s.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\n')
time.sleep(7)
try:
    data = s.recv(4096)
    print('recv returned:', repr(data))
finally:
    s.close()
"
```

Expected client-side result:

```txt
recv returned: b''
```

An empty `recv()` result means the server closed the connection.

Expected server-side log in Terminal 1:

```txt
Client timeout for fd <N> after 5s of inactivity
```

Purpose:

* confirms inactivity detection;
* confirms safe client disconnection;
* confirms timeout logging.

---

## 5. Test: server still works after client timeout

Purpose: verify that closing a timed-out client does not break the server.

Keep the same server running from section 5:

```bash
./webserv configs/test-client-timeout.conf
```

After running the stalled-client test from section 5, send a normal request:

```bash
curl -i http://127.0.0.1:8098/
```

Expected:

```http
HTTP/1.1 200 OK
Connection: close
```

Purpose: confirms that the server remains usable after enforcing a client timeout.

---

## 6. Test: stalled client does not block other clients

Purpose: verify that one incomplete client does not prevent the event loop from serving other requests.

Terminal 1:

```bash
./webserv configs/test-client-timeout.conf
```

Terminal 2:

```bash
python3 -c "
import socket, time

slow = socket.create_connection(('127.0.0.1', 8098))
slow.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\n')

started = time.monotonic()
with socket.create_connection(('127.0.0.1', 8098), timeout=3) as fast:
    fast.sendall(
        b'GET / HTTP/1.1\r\n'
        b'Host: localhost\r\n'
        b'Connection: close\r\n'
        b'\r\n'
    )
    response = fast.recv(4096)

elapsed = time.monotonic() - started
print('elapsed:', round(elapsed, 2), 's')
print(response.split(b'\\r\\n', 1)[0])
slow.close()
"
```

Expected:

* elapsed time stays well below the configured client timeout;
* status line is `HTTP/1.1 200 OK`.

Example:

```txt
elapsed: 0.03 s
b'HTTP/1.1 200 OK'
```

Purpose: confirms that timeout handling does not break the non-blocking `poll()` flow.

---

## 7. Automated Python test suite

Purpose: run the timeout regression checks in one command.

Script:

```txt
tests/test_client_timeout.py
```

### Start the test server

Terminal 1:

```bash
./webserv configs/test-client-timeout.conf
```

### Run the tests

Terminal 2:

```bash
python3 tests/test_client_timeout.py --host 127.0.0.1 --port 8098 --client-timeout 5
```

Expected output:

```txt
PASS stalled client closed
PASS server still works
PASS stalled client does not block others
```

Exit code:

```txt
0
```

### What the script validates

| Test name | Purpose |
| --------- | ------- |
| `stalled client closed` | incomplete request is disconnected after timeout |
| `server still works` | normal `GET /` still returns `200` |
| `stalled client does not block others` | another client can be served immediately |

---

## 8. Test: activity refresh resets the timeout

Purpose: verify that real client I/O refreshes `lastActivity` and prevents premature timeout.

Terminal 1:

```bash
./webserv configs/test-client-timeout.conf
```

Terminal 2:

```bash
python3 -c "
import socket, time

s = socket.create_connection(('127.0.0.1', 8098))

# Send partial headers, then keep the connection alive with periodic bytes.
s.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\nX-Keep-Alive: ')

for i in range(4):
    time.sleep(2)
    s.sendall(b'a')
    print('sent keep-alive byte', i + 1)

# Connection should still be open before the 5s inactivity window is exceeded
# because each send() refreshes lastActivity.
time.sleep(1)
s.send(b'b')
print('connection still open after keep-alive traffic')
s.close()
"
```

Expected:

* the connection remains open while bytes are sent regularly;
* no `Client timeout for fd ...` log appears during active sending;
* after sending stops completely, the timeout may still trigger later if the request remains incomplete.

Purpose: confirms that timeout is based on inactivity, not only on connection age.

---

## 9. Test: default config timeout on port 8080

Purpose: verify the production-like timeout value in `configs/default.conf`.

Terminal 1:

```bash
./webserv configs/default.conf
```

Terminal 2:

```bash
python3 -c "
import socket, time
s = socket.create_connection(('127.0.0.1', 8080))
s.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\n')
time.sleep(32)
data = s.recv(4096)
print('recv returned:', repr(data))
s.close()
"
```

Expected:

* after about 30 seconds of inactivity, the server closes the connection;
* server log contains `Client timeout for fd <N> after 30s of inactivity`.

Then verify recovery:

```bash
curl -i http://127.0.0.1:8080/
```

Expected:

```http
HTTP/1.1 200 OK
```

Purpose: confirms that the configurable `client_timeout` value from `default.conf` is actually used at runtime.

---

## 10. What these tests do not cover

These tests intentionally do **not** replace CGI timeout coverage.

Do not use this document to validate:

* long-running CGI scripts;
* `504 Gateway Timeout` for CGI;
* CGI child process cleanup after script timeout.

Use `docs/cgi-tests.md` for those cases.

---

## 11. Quick regression checklist

Before opening or merging the PR, run:

```bash
make re

./webserv configs/client-timeout-zero.conf
python3 tests/test_client_timeout.py --host 127.0.0.1 --port 8098 --client-timeout 5
curl -i http://127.0.0.1:8080/
```

Manual checklist while `configs/test-client-timeout.conf` is running:

```bash
./webserv configs/test-client-timeout.conf
```

Then in another terminal:

```bash
python3 -c "
import socket, time
s = socket.create_connection(('127.0.0.1', 8098))
s.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\n')
time.sleep(7)
print(s.recv(4096))
s.close()
"

curl -i http://127.0.0.1:8098/
```

Expected summary:

| Check | Expected result | Purpose |
| ----- | --------------- | ------- |
| `client-timeout-zero.conf` | startup fails | invalid timeout rejected |
| `test_client_timeout.py` | `3x PASS`, exit `0` | automated regression |
| incomplete client on `8098` | connection closed after ~5s | inactivity timeout enforced |
| server log | `Client timeout for fd ...` | timeout event logged |
| `curl /` on `8098` after timeout | `200 OK` | server still healthy |
| `curl /` on `8080` with `default.conf` | `200 OK` | default config unaffected |

---

## 12. Issue subtask mapping

| Issue subtask | Covered by |
| ------------- | ---------- |
| Store last activity timestamps per client | `Client::lastActivity`, `Client::touchActivity()` |
| Detect inactive connections | `ClientTimeoutHandler::getPollTimeoutMs()`, `isExpired()` |
| Close timed-out clients safely | `WebServ::enforceClientTimeouts()`, `closeAndRemoveFd()` |
| Add configurable timeout values | `client_timeout` in config parser and validator |
| Log or expose timeout events for debugging | `Client timeout for fd ...` log line |
| Test timeout behavior with slow clients | sections 5–8 and `tests/test_client_timeout.py` |
