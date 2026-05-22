# CGI Tests

This document contains manual tests for the CGI implementation of `webserv`.

The goal is not only to check that a Python script can be executed, but also to verify the full CGI contract required by the subject:

* CGI execution based on configured file extensions;
* standard CGI meta-variables;
* HTTP request headers converted to `HTTP_*` variables;
* implementation-specific variables used by common CGI/PHP-style environments;
* request body forwarding through `stdin`;
* correct query string and `PATH_INFO` splitting;
* CGI response header parsing;
* invalid CGI extension handling;
* current known limitations and missing tests.

---

## 1. Test setup

### Server

Start the server with the default configuration:

```bash
make
./webserv configs/default.conf
```

The tests assume that the server listens on:

```txt
http://localhost:8080
```

The default configuration should contain a CGI location similar to:

```nginx
location /cgi-bin {
    methods GET POST;
    root ./www/cgi-bin;
    autoindex off;
    cgi .py /usr/bin/python3;
    cgi .php /usr/bin/php-cgi;
}
```

### Test scripts

The following scripts are expected in `www/cgi-bin/`:

```txt
env.py
test-spec-vars.py
hello.py
time.py
forbidden.txt
```

Make CGI scripts executable:

```bash
chmod +x ./www/cgi-bin/*.py
```

---

## 2. Main CGI debug script: `env.py`

Use this script to test standard variables, `HTTP_*` variables and body forwarding.

```python
#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/plain")
print()

keys = [
    "AUTH_TYPE",
    "CONTENT_LENGTH",
    "CONTENT_TYPE",
    "GATEWAY_INTERFACE",
    "PATH_INFO",
    "PATH_TRANSLATED",
    "QUERY_STRING",
    "REMOTE_ADDR",
    "REMOTE_HOST",
    "REMOTE_IDENT",
    "REMOTE_USER",
    "REQUEST_METHOD",
    "SCRIPT_NAME",
    "SERVER_NAME",
    "SERVER_PORT",
    "SERVER_PROTOCOL",
    "SERVER_SOFTWARE",
]

for key in keys:
    print(f"{key}={os.environ.get(key, '<missing>')}")

http_keys = sorted(key for key in os.environ if key.startswith("HTTP_"))
for key in http_keys:
    print(f"{key}={os.environ.get(key, '<missing>')}")

print()
print("BODY:")
print(sys.stdin.read())
```

Important:

* `<missing>` means the variable was not passed to the CGI process at all;
* an empty value like `QUERY_STRING=` means the variable exists but is empty.

---

## 3. Standard CGI meta-variable tests

### 3.1 GET with query string

```bash
curl -i "http://localhost:8080/cgi-bin/env.py?x=42"
```

Expected important response headers:

```http
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: <computed by server>
```

Expected important CGI output:

```txt
REQUEST_METHOD=GET
SCRIPT_NAME=/cgi-bin/env.py
PATH_INFO=
PATH_TRANSLATED=
QUERY_STRING=x=42
REMOTE_ADDR=127.0.0.1
GATEWAY_INTERFACE=CGI/1.1
SERVER_NAME=default_server
SERVER_PORT=8080
SERVER_PROTOCOL=HTTP/1.1
SERVER_SOFTWARE=webserv/1.0
```

This checks:

* CGI detection by `.py` extension;
* query string extraction;
* `SCRIPT_NAME` does not include the query string;
* standard server variables are present;
* `REMOTE_ADDR` is forwarded to CGI.

### 3.2 GET without query string

```bash
curl -i "http://localhost:8080/cgi-bin/env.py"
```

Expected important values:

```txt
REQUEST_METHOD=GET
SCRIPT_NAME=/cgi-bin/env.py
QUERY_STRING=
PATH_INFO=
PATH_TRANSLATED=
```

This checks that `QUERY_STRING` exists but is empty when no query string is provided.

### 3.3 `PATH_INFO` splitting

```bash
curl -i "http://localhost:8080/cgi-bin/env.py/extra/path?x=42"
```

Expected important values:

```txt
REQUEST_METHOD=GET
SCRIPT_NAME=/cgi-bin/env.py
PATH_INFO=/extra/path
PATH_TRANSLATED=./www/cgi-bin/extra/path
QUERY_STRING=x=42
```

This checks that the request target is split into:

```txt
SCRIPT_NAME=/cgi-bin/env.py
PATH_INFO=/extra/path
```

### 3.4 All standard variables are present

```bash
curl -i "http://localhost:8080/cgi-bin/env.py?x=42"
```

Expected variables:

```txt
AUTH_TYPE=
CONTENT_LENGTH=
CONTENT_TYPE=
GATEWAY_INTERFACE=CGI/1.1
PATH_INFO=
PATH_TRANSLATED=
QUERY_STRING=x=42
REMOTE_ADDR=127.0.0.1
REMOTE_HOST=
REMOTE_IDENT=
REMOTE_USER=
REQUEST_METHOD=GET
SCRIPT_NAME=/cgi-bin/env.py
SERVER_NAME=default_server
SERVER_PORT=8080
SERVER_PROTOCOL=HTTP/1.1
SERVER_SOFTWARE=webserv/1.0
```

Expected empty values for a simple GET request:

```txt
AUTH_TYPE=
CONTENT_LENGTH=
CONTENT_TYPE=
PATH_INFO=
PATH_TRANSLATED=
REMOTE_HOST=
REMOTE_IDENT=
REMOTE_USER=
```

---

## 4. HTTP header to `HTTP_*` variable tests

The CGI implementation should convert normal request headers into CGI environment variables using this rule:

```txt
Header-Name: value
=> HTTP_HEADER_NAME=value
```

Hyphens are replaced with underscores and letters are uppercased.

`Content-Type` and `Content-Length` are special cases and must not be duplicated as `HTTP_CONTENT_TYPE` or `HTTP_CONTENT_LENGTH`. They must be exposed as `CONTENT_TYPE` and `CONTENT_LENGTH`.

### 4.1 Basic custom headers

```bash
curl -i \
  -H "X-Test-Header: hello" \
  -H "User-Agent: webserv-cgi-test" \
  -H "Accept-Language: en-US" \
  "http://localhost:8080/cgi-bin/env.py?x=42"
```

Expected important values:

```txt
HTTP_X_TEST_HEADER=hello
HTTP_USER_AGENT=webserv-cgi-test
HTTP_ACCEPT_LANGUAGE=en-US
```

This checks that normal request headers are passed to CGI as `HTTP_*` variables.

### 4.2 Header name normalization

```bash
curl -i \
  -H "X-Long-Custom-Header: normalized" \
  "http://localhost:8080/cgi-bin/env.py"
```

Expected:

```txt
HTTP_X_LONG_CUSTOM_HEADER=normalized
```

This checks that `-` becomes `_` and the name is uppercased.

### 4.3 No duplication of content headers

```bash
curl -i -X POST \
  -H "Content-Type: text/plain" \
  -H "X-Test-Header: hello" \
  -d "abc" \
  "http://localhost:8080/cgi-bin/env.py"
```

Expected:

```txt
CONTENT_TYPE=text/plain
CONTENT_LENGTH=3
HTTP_X_TEST_HEADER=hello
```

Not expected:

```txt
HTTP_CONTENT_TYPE=
HTTP_CONTENT_LENGTH=
```

This checks that content headers are treated according to CGI rules and are not duplicated in the `HTTP_*` namespace.

### 4.4 Case-insensitive content headers

```bash
curl -i -X POST \
  -H "content-type: text/otherTest" \
  -d "name=tigran" \
  "http://localhost:8080/cgi-bin/env.py"
```

Expected:

```txt
CONTENT_TYPE=text/otherTest
CONTENT_LENGTH=11
```

Not expected:

```txt
HTTP_CONTENT_TYPE=text/otherTest
```

This checks that the implementation treats header names case-insensitively.

---

## 5. Request body / stdin tests

The CGI process must receive the request body through `stdin`.

### 5.1 POST form body

```bash
curl -i -X POST \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "name=tigran" \
  "http://localhost:8080/cgi-bin/env.py"
```

Expected important values:

```txt
REQUEST_METHOD=POST
CONTENT_TYPE=application/x-www-form-urlencoded
CONTENT_LENGTH=11
```

Expected body section:

```txt
BODY:
name=tigran
```

This checks that the request body is available to the CGI script via `stdin`.

### 5.2 POST body with query string

```bash
curl -i -X POST \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "name=tigran" \
  "http://localhost:8080/cgi-bin/env.py?mode=test"
```

Expected:

```txt
REQUEST_METHOD=POST
QUERY_STRING=mode=test
CONTENT_TYPE=application/x-www-form-urlencoded
CONTENT_LENGTH=11
BODY:
name=tigran
```

This checks that query string and body can exist together.

### 5.3 Empty POST body

```bash
curl -i -X POST \
  -H "Content-Type: text/plain" \
  "http://localhost:8080/cgi-bin/env.py"
```

Expected:

```txt
REQUEST_METHOD=POST
CONTENT_TYPE=text/plain
CONTENT_LENGTH=
BODY:
```

Depending on the parser behavior, `CONTENT_LENGTH` may be empty or `0`. The important part is that the CGI does not hang waiting for stdin. The server must close the CGI stdin pipe so the script receives EOF.

### 5.4 Larger body within `client_max_body_size`

```bash
python3 - <<'PY' | curl -i -X POST \
  -H "Content-Type: text/plain" \
  --data-binary @- \
  "http://localhost:8080/cgi-bin/env.py"
print("A" * 4096)
PY
```

Expected:

```txt
REQUEST_METHOD=POST
CONTENT_TYPE=text/plain
BODY:
AAAA...
```

This checks that body forwarding is not limited to tiny payloads.

---

## 6. CGI response parsing tests

### 6.1 CGI `Content-Type` controls HTTP response header

Ensure `env.py` prints:

```python
print("Content-Type: text/plain")
```

Then run:

```bash
curl -i "http://localhost:8080/cgi-bin/env.py"
```

Expected HTTP header:

```http
Content-Type: text/plain
```

Now temporarily change `env.py` to:

```python
print("Content-Type: text/test")
```

Run:

```bash
curl -i "http://localhost:8080/cgi-bin/env.py"
```

Expected HTTP header:

```http
Content-Type: text/test
```

This checks that the response `Content-Type` comes from the CGI output, not from the request headers.

After the test, restore:

```python
print("Content-Type: text/plain")
```

### 6.2 CGI output without headers

Use `test.py`:

```python
print("HELLO FROM PYTHON CGI!")
```

Run:

```bash
curl -i "http://localhost:8080/cgi-bin/test.py"
```

Expected behavior:

```http
HTTP/1.1 200 OK
Content-Type: text/plain
```

Expected body:

```txt
HELLO FROM PYTHON CGI!
```

This checks fallback behavior when CGI output has no CGI header block.

---

## 7. Implementation-specific variable tests

Use `test-spec-vars.py`:

```python
#!/usr/bin/env python3
import os

print("Content-Type: text/plain")
print()

keys = [
    "SCRIPT_FILENAME",
    "DOCUMENT_ROOT",
    "REQUEST_URI",
    "REQUEST_SCHEME",
    "HTTPS",
    "SERVER_ADMIN",
    "REDIRECT_STATUS",
    "FCGI_ROLE",
    "PHP_SELF",
    "PATH",
    "PWD",
    "REQUEST_TIME",
    "REQUEST_TIME_FLOAT",
]

for key in keys:
    print(f"{key}={os.environ.get(key, '')}")
```

Run:

```bash
curl -i "http://localhost:8080/cgi-bin/test-spec-vars.py/extra/path?x=42"
```

Expected important values:

```txt
SCRIPT_FILENAME=./www/cgi-bin/test-spec-vars.py
DOCUMENT_ROOT=./www/cgi-bin
REQUEST_URI=/cgi-bin/test-spec-vars.py/extra/path?x=42
REQUEST_SCHEME=http
HTTPS=off
SERVER_ADMIN=admin@localhost
REDIRECT_STATUS=200
FCGI_ROLE=RESPONDER
PHP_SELF=/cgi-bin/test-spec-vars.py/extra/path
PATH=/usr/bin:/bin
PWD=./www/cgi-bin
REQUEST_TIME=<unix timestamp>
REQUEST_TIME_FLOAT=<unix timestamp with microseconds>
```

This checks the implementation-specific variables added for compatibility with common CGI/PHP-style scripts.

Important notes:

* These variables are not all mandatory CGI/1.1 standard variables.
* They are useful for compatibility and debugging.
* Their values must still be internally consistent with the resolved script path and route root.

---

## 8. Multiple script and extension tests

### 8.1 Another valid Python CGI script

```bash
curl -i "http://localhost:8080/cgi-bin/hello.py"
```

Expected:

```http
HTTP/1.1 200 OK
Content-Type: text/html
```

Expected body contains:

```html
<h1>Hello from CGI</h1>
```

This checks that CGI execution is not hardcoded only for `env.py`.

### 8.2 Another valid script with query string

```bash
curl -i "http://localhost:8080/cgi-bin/time.py?format=unix"
```

Expected:

```txt
The script is executed as CGI and receives QUERY_STRING=format=unix.
```

This checks query string handling for different CGI scripts.

### 8.3 Non-configured extension inside CGI location

```bash
curl -i "http://localhost:8080/cgi-bin/forbidden.txt"
```

Expected:

```txt
The file must not be executed as CGI.
```

Depending on route/static-file behavior, the response may be `403`, `404`, or a normal static response. The important requirement is that it must not be executed as CGI.

### 8.4 Extension boundary: `.pybackup` must not execute

```bash
curl -i "http://localhost:8080/cgi-bin/env.pybackup"
```

Expected:

```txt
The request must not be treated as a valid .py CGI script.
```

This checks that `/cgi-bin/env.py` is CGI, but `/cgi-bin/env.pybackup` is not.

### 8.5 Unknown CGI extension

```bash
curl -i "http://localhost:8080/cgi-bin/env.unknown"
```

Expected:

```txt
The request must not be executed as CGI.
```

---

## 9. Method policy tests

The default `/cgi-bin` route allows only `GET` and `POST`.

### 9.1 DELETE should be rejected for CGI route

```bash
curl -i -X DELETE "http://localhost:8080/cgi-bin/env.py"
```

Expected:

```http
HTTP/1.1 405 Method Not Allowed
```

This checks that the route method policy is applied before or during CGI handling.

### 9.2 Unsupported method should not execute CGI

```bash
printf 'PUT /cgi-bin/env.py HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n' | nc 127.0.0.1 8080
```

Expected:

```http
405 Method Not Allowed
```

This checks that unsupported methods do not accidentally execute CGI.

---

## 10. Chunked body tests

The subject requires that chunked requests be unchunked before being passed to CGI. The CGI script should receive EOF as the end of the body.

Run:

```bash
printf 'POST /cgi-bin/env.py HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n' | nc 127.0.0.1 8080
```

Expected CGI body:

```txt
BODY:
Wikipedia
```

Expected not to appear:

```txt
4
Wiki
5
pedia
0
```

If this currently fails, mark it as a missing mandatory CGI-related subject point.

---

## 11. Correct working directory test

The subject requires CGI to run in the correct directory for relative path access.

Create `www/cgi-bin/read-relative.py`:

```python
#!/usr/bin/env python3

print("Content-Type: text/plain")
print()

with open("relative-data.txt", "r") as f:
    print(f.read())
```

Create `www/cgi-bin/relative-data.txt`:

```txt
relative-ok
```

Run:

```bash
chmod +x ./www/cgi-bin/read-relative.py
curl -i "http://localhost:8080/cgi-bin/read-relative.py"
```

Expected body:

```txt
relative-ok
```

If this fails, the CGI process is probably not running with the expected working directory. In the current implementation, `PWD` is passed as an environment variable, but that is not the same thing as calling `chdir()` before `execve()`.

---

## 12. Timeout / hanging CGI tests

A CGI script must not be able to make the whole server hang indefinitely.

Create `www/cgi-bin/sleep.py`:

```python
#!/usr/bin/env python3
import time

print("Content-Type: text/plain")
print()
time.sleep(60)
print("done")
```

Run:

```bash
chmod +x ./www/cgi-bin/sleep.py
curl -i --max-time 5 "http://localhost:8080/cgi-bin/sleep.py"
```

Expected for a production-like implementation:

```txt
The server should not block all other clients while this CGI is running.
```

In the current synchronous implementation, this is a high-risk area because `waitpid()` and pipe reads are blocking.

---

## 13. Non-blocking CGI pipe warning

The subject says that I/O that can wait for data, including pipes/FIFOs, must be non-blocking and driven by the single `poll()` or equivalent event loop.

Therefore, CGI stdin/stdout pipes should eventually be integrated into the main non-blocking event loop.

Current blocking patterns to watch for:

```cpp
write(stdinPipe[1], ...)
read(stdoutPipe[0], ...)
waitpid(pid, &status, 0)
```

These can work in simple tests, but they are not subject-ready for stress tests if they block the server loop.

---

## 14. Debug output policy

During development, it is useful to print CGI variables before `execve()`.

Example:

```txt
===== CGI ENV - STANDARD VARS =====
REQUEST_METHOD=GET
SCRIPT_NAME=/cgi-bin/env.py
QUERY_STRING=x=42
...
```

However, debug output should not always be enabled in the final version. It should be guarded with a macro:

```cpp
#ifdef DEBUG_CGI
    debugPrintEnv("CGI ENV - STANDARD VARS", context.standard.values);
#endif
```

Enable debug build:

```bash
make CXXFLAGS="-Wall -Wextra -Werror -std=c++98 -g -DDEBUG_CGI"
```

Final defense build should not print noisy CGI debug logs by default.

---

## 15. Coverage checklist

| Area                                               | Test section  | Status                            |
| -------------------------------------------------- | ------------- | --------------------------------- |
| CGI extension detection                            | 8             | Covered                           |
| Standard CGI variables                             | 3             | Covered                           |
| Query string                                       | 3.1, 3.2, 5.2 | Covered                           |
| `PATH_INFO` / `PATH_TRANSLATED`                    | 3.3           | Covered                           |
| `HTTP_*` variables                                 | 4             | Covered                           |
| `CONTENT_TYPE` / `CONTENT_LENGTH` special handling | 4.3, 4.4, 5   | Covered                           |
| Request body through stdin                         | 5             | Covered                           |
| CGI response headers                               | 6             | Covered                           |
| Implementation-specific variables                  | 7             | Covered                           |
| Multiple scripts                                   | 8.1, 8.2      | Covered                           |
| Unknown/non-CGI extensions                         | 8.3, 8.4, 8.5 | Covered                           |
| Method restrictions                                | 9             | Covered                           |
| Chunked request body                               | 10            | Must be verified / likely missing |
| Correct working directory                          | 11            | Must be verified / likely missing |
| CGI timeout / no indefinite hang                   | 12            | Must be implemented/tested        |
| Non-blocking CGI pipes                             | 13            | Architectural risk                |
| Debug output guarded by macro                      | 14            | Cleanup needed                    |

---

## 16. Recommended minimum test run before closing CGI

Run at least:

```bash
curl -i "http://localhost:8080/cgi-bin/env.py?x=42"

curl -i "http://localhost:8080/cgi-bin/env.py/extra/path?x=42"

curl -i \
  -H "X-Test-Header: hello" \
  -H "User-Agent: webserv-cgi-test" \
  "http://localhost:8080/cgi-bin/env.py"

curl -i -X POST \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "name=tigran" \
  "http://localhost:8080/cgi-bin/env.py?mode=test"

curl -i "http://localhost:8080/cgi-bin/test-spec-vars.py/extra/path?x=42"

curl -i "http://localhost:8080/cgi-bin/hello.py"

curl -i "http://localhost:8080/cgi-bin/env.pybackup"

curl -i -X DELETE "http://localhost:8080/cgi-bin/env.py"

printf 'POST /cgi-bin/env.py HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nContent-Type: text/plain\r\n\r\n4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n' | nc 127.0.0.1 8080
```

Only after these pass should the CGI documentation be considered reasonably complete for defense preparation.
