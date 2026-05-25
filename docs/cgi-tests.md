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

The subject requires CGI scripts to be executed in the correct working directory so that relative file access works as expected.

This test verifies that the CGI process is started from the directory containing the CGI script, not from the root directory of the `webserv` executable.

### Test files

Create the following CGI script:

```bash
cat > www/cgi-bin/read-relative.py <<'PY'
#!/usr/bin/env python3
import os

print("Content-Type: text/plain")
print()

print("cwd=" + os.getcwd())

try:
    with open("relative-data.txt", "r") as f:
        print("relative_file=" + f.read().strip())
except Exception as e:
    print("relative_file_error=" + str(e))
PY

chmod +x www/cgi-bin/read-relative.py
```

Create the relative data file in the same directory as the CGI script:

```bash
echo "relative-ok" > www/cgi-bin/relative-data.txt
```

### Run the test

Start the server:

```bash
make
./webserv configs/default.conf
```

Then run:

```bash
curl -i "http://127.0.0.1:8080/cgi-bin/read-relative.py"
```

### Expected result

The response must contain:

```txt
cwd=/absolute/path/to/webserv/www/cgi-bin
relative_file=relative-ok
```

Example:

```txt
HTTP/1.1 200 OK
Connection: close
Content-Type: text/plain

cwd=/home/user/Github/webserv/www/cgi-bin
relative_file=relative-ok
```

### What this test validates

This confirms that:

* the CGI process calls `chdir()` before `execve()`;
* the working directory is the directory containing the CGI script;
* relative file access from inside the CGI script works correctly;
* the server does not only set the `PWD` environment variable, but actually changes the process working directory.

### Failure example

If the response looks like this:

```txt
cwd=/home/user/Github/webserv
relative_file_error=[Errno 2] No such file or directory: 'relative-data.txt'
```

then the CGI process is still running from the server root directory instead of the CGI script directory.

In that case, the implementation does not satisfy the subject requirement:

```txt
The CGI should be run in the correct directory for relative path file access.
```

### Regression checks

After this test passes, also verify that normal CGI behavior still works:

```bash
curl -i "http://127.0.0.1:8080/cgi-bin/env.py?x=42"
```

```bash
curl -i "http://127.0.0.1:8080/cgi-bin/env.py/extra/path?x=42"
```

```bash
curl -i -X POST \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "name=tigran" \
  "http://127.0.0.1:8080/cgi-bin/env.py"
```

Expected important values:

```txt
QUERY_STRING=x=42
PATH_INFO=/extra/path
CONTENT_LENGTH=11
CONTENT_TYPE=application/x-www-form-urlencoded
BODY:
name=tigran
```

This ensures that changing the CGI working directory did not break query string handling, `PATH_INFO`, or request body forwarding through stdin.


---

## 12. Timeout / hanging CGI tests

A CGI script must not be able to make the whole server hang indefinitely.

This test verifies that a long-running CGI process is stopped by the server timeout and that the client receives a `504 Gateway Timeout` response.

### Test script

Create `www/cgi-bin/slow.py`:

```bash
cat > www/cgi-bin/slow.py <<'PY'
#!/usr/bin/env python3
import time
import os

print("Content-Type: text/plain")
print()
print("CGI started, pid =", os.getpid(), flush=True)

time.sleep(30)

print("CGI finished")
PY

chmod +x www/cgi-bin/slow.py
```

### Run the test

Start the server:

```bash
make
./webserv configs/default.conf
```

Then run:

```bash
curl -v "http://127.0.0.1:8080/cgi-bin/slow.py"
```

### Expected result

If the CGI script exceeds the configured CGI timeout, the server should return:

```http
HTTP/1.1 504 Gateway Timeout
Connection: close
Content-Type: text/html
```

Example body:

```html
<html><body><h1>Custom 504 Gateway Timeout</h1></body></html>
```

### What this test validates

This confirms that:

* a hanging CGI process does not block the server forever;
* the server detects CGI timeout;
* the CGI child process is killed;
* the CGI child process is reaped with `waitpid()`;
* the client receives a proper `504 Gateway Timeout` response;
* the server remains usable after the timeout.

### Regression check

After the timeout response, verify that the server still handles normal requests:

```bash
curl -v "http://127.0.0.1:8080/"
```

Expected:

```http
HTTP/1.1 200 OK
```

---

## 13. Client disconnect cleanup test

This test verifies that if the client disconnects while a CGI script is still running, the server immediately cleans up the CGI process.

This is important because a CGI child process must not continue running after its client connection has been closed.

### Test script

Use the same `www/cgi-bin/slow.py` script from the timeout test:

```python
#!/usr/bin/env python3
import time
import os

print("Content-Type: text/plain")
print()
print("CGI started, pid =", os.getpid(), flush=True)

time.sleep(30)

print("CGI finished")
```

Make sure it is executable:

```bash
chmod +x www/cgi-bin/slow.py
```

### Run the test

Start the server:

```bash
make
./webserv configs/default.conf
```

In another terminal, start the slow CGI request:

```bash
curl -v "http://127.0.0.1:8080/cgi-bin/slow.py"
```

While the request is still running, stop `curl` with:

```txt
Ctrl+C
```

Immediately check whether the CGI process is still alive:

```bash
ps aux | grep '[s]low.py'
```

or:

```bash
pgrep -af slow.py
```

### Expected result

After `Ctrl+C`, there should be no running `slow.py` process.

Expected output:

```txt
<no output>
```

The server should continue running and should still be able to handle new requests:

```bash
curl -v "http://127.0.0.1:8080/"
```

Expected:

```http
HTTP/1.1 200 OK
```

### What this test validates

This confirms that the server:

* detects client socket close during active CGI execution;
* calls CGI cleanup before removing the client;
* closes CGI stdin and stdout file descriptors;
* removes CGI file descriptors from `poll`;
* kills the CGI child process if it is still running;
* reaps the CGI child process with `waitpid()`;
* keeps the server alive after the disconnect.

### Failure example

If this command still shows a running process after `Ctrl+C`:

```bash
ps aux | grep '[s]low.py'
```

Example failure:

```txt
user  12345  0.0  0.1  13688  8960 pts/8  S+  12:11  0:00 /usr/bin/python3 slow.py
```

then the server did not clean up the CGI child process correctly.

This means the CGI cleanup on client disconnect is still broken.

### Manual test result example

A successful test should look like this:

```bash
curl -v http://127.0.0.1:8080/cgi-bin/slow.py
# Press Ctrl+C while the request is running

ps aux | grep '[s]low.py'
# No output

curl -v http://127.0.0.1:8080/
# HTTP/1.1 200 OK
```

---

## 14. CGI target validation before fork

The server must validate the resolved CGI script path and the configured CGI executable/interpreter before starting a CGI process.

Invalid CGI targets must be rejected before `fork()` / `execve()`.

This prevents cases where a missing script, a directory, an unreadable file, or an invalid interpreter could produce an empty response, a misleading `200 OK`, or an unnecessary child process.

### 14.1 Missing CGI script

Run:

```bash
curl -v "http://127.0.0.1:8080/cgi-bin/notfound.py"
```

Expected:

```http
HTTP/1.1 404 Not Found
Connection: close
Content-Type: text/html
```

Example body:

```html
<html><body><h1>Custom 404 Not Found</h1></body></html>
```

This checks that a missing CGI script is rejected before starting the CGI process.

---

### 14.2 Directory requested inside CGI location

Run:

```bash
curl -v "http://127.0.0.1:8080/cgi-bin/"
```

Expected:

```http
HTTP/1.1 403 Forbidden
Connection: close
Content-Type: text/html
```

Example body:

```html
<html><body><h1>Custom 403 Forbidden</h1></body></html>
```

This checks that a directory is not executed as a CGI script.

---

### 14.3 Directory pretending to be a CGI script

Create a directory with a CGI extension:

```bash
mkdir -p www/cgi-bin/fake.py
```

Run:

```bash
curl -v "http://127.0.0.1:8080/cgi-bin/fake.py"
```

Expected:

```http
HTTP/1.1 403 Forbidden
Connection: close
Content-Type: text/html
```

Example body:

```html
<html><body><h1>Custom 403 Forbidden</h1></body></html>
```

Cleanup:

```bash
rm -rf www/cgi-bin/fake.py
```

This checks that even if the path has a configured CGI extension, it must not be executed when the resolved target is a directory.

---

### 14.4 Unreadable CGI script

Remove read permission from an existing CGI script:

```bash
chmod a-r www/cgi-bin/test.py
```

Run:

```bash
curl -v "http://127.0.0.1:8080/cgi-bin/test.py"
```

Expected:

```http
HTTP/1.1 403 Forbidden
Connection: close
Content-Type: text/html
```

Example body:

```html
<html><body><h1>Custom 403 Forbidden</h1></body></html>
```

Restore permissions:

```bash
chmod +r www/cgi-bin/test.py
```

Then verify that the script works again:

```bash
curl -v "http://127.0.0.1:8080/cgi-bin/test.py"
```

Expected:

```http
HTTP/1.1 200 OK
Connection: close
Content-Type: text/plain
```

Expected body:

```txt
HELLO FROM PYTHON CGI!
```

This checks that an existing but inaccessible CGI script is rejected with `403 Forbidden`, and that restoring permissions restores normal CGI behavior.

---

### 14.5 Invalid CGI executable / interpreter

Create a temporary config for this test:

```bash
cp configs/default.conf configs/test-invalid-cgi.conf
```

In `configs/test-invalid-cgi.conf`, replace the Python CGI interpreter with an invalid path:

```nginx
cgi .py /bad/python/path;
```

Start the server with this config:

```bash
make
./webserv configs/test-invalid-cgi.conf
```

Run:

```bash
curl -v "http://127.0.0.1:8080/cgi-bin/test.py"
```

Expected:

```http
HTTP/1.1 502 Bad Gateway
Connection: close
Content-Type: text/html
```

Example body:

```html
<html><body><h1>Custom 502 Bad Gateway</h1></body></html>
```

This checks that an invalid configured CGI executable/interpreter is rejected before starting the CGI process.

After the test, restore the normal config or run the server again with:

```bash
./webserv configs/default.conf
```

The normal CGI configuration should use a valid interpreter, for example:

```nginx
cgi .py /usr/bin/python3;
```

---

### What this test validates

This confirms that the server:

* checks that the resolved CGI script path exists;
* rejects missing CGI scripts with `404 Not Found`;
* rejects directories used as CGI scripts with `403 Forbidden`;
* rejects unreadable CGI scripts with `403 Forbidden`;
* checks that the configured CGI executable/interpreter exists;
* checks that the configured CGI executable/interpreter has execute permission;
* returns `502 Bad Gateway` for invalid CGI executable/interpreter;
* rejects invalid CGI targets before `fork()` / `execve()`;
* keeps normal valid CGI execution working.

### Failure examples

A missing CGI script must not return:

```http
HTTP/1.1 200 OK
```

A directory used as a CGI script must not be executed.

An invalid interpreter must not produce an empty response or a misleading successful CGI response.

---

### Successful manual test result example

```bash
curl -v http://127.0.0.1:8080/cgi-bin/notfound.py
# HTTP/1.1 404 Not Found

curl -v http://127.0.0.1:8080/cgi-bin/
# HTTP/1.1 403 Forbidden

mkdir -p www/cgi-bin/fake.py
curl -v http://127.0.0.1:8080/cgi-bin/fake.py
# HTTP/1.1 403 Forbidden
rm -rf www/cgi-bin/fake.py

chmod a-r www/cgi-bin/test.py
curl -v http://127.0.0.1:8080/cgi-bin/test.py
# HTTP/1.1 403 Forbidden

chmod +r www/cgi-bin/test.py
curl -v http://127.0.0.1:8080/cgi-bin/test.py
# HTTP/1.1 200 OK

# With invalid interpreter config:
curl -v http://127.0.0.1:8080/cgi-bin/test.py
# HTTP/1.1 502 Bad Gateway
```

---

### Coverage checklist update

Add this row to the `Coverage checklist` table:

```md
| CGI target validation before fork                  | 14            | Covered                           |
```

If you inserted this section as `## 14`, then the old following sections should be renumbered:

```txt
Old 14 -> New 15
Old 15 -> New 16
Old 16 -> New 17
Old 17 -> New 18
```

---

### Recommended minimum test run update

Add these commands to the `Recommended minimum test run before closing CGI` section:

```bash
# Missing CGI script
curl -v "http://127.0.0.1:8080/cgi-bin/notfound.py"

# Directory inside CGI location
curl -v "http://127.0.0.1:8080/cgi-bin/"

# Directory pretending to be a CGI script
mkdir -p www/cgi-bin/fake.py
curl -v "http://127.0.0.1:8080/cgi-bin/fake.py"
rm -rf www/cgi-bin/fake.py

# Unreadable CGI script
chmod a-r www/cgi-bin/test.py
curl -v "http://127.0.0.1:8080/cgi-bin/test.py"
chmod +r www/cgi-bin/test.py

# Normal CGI still works
curl -v "http://127.0.0.1:8080/cgi-bin/test.py"

# Invalid interpreter test requires a temporary config:
# cgi .py /bad/python/path;
curl -v "http://127.0.0.1:8080/cgi-bin/test.py"
```

---

## 15. Non-blocking CGI pipe warning

The subject says that I/O that can wait for data, including pipes/FIFOs, must be non-blocking and driven by the single `poll()` or equivalent event loop.

Therefore, CGI stdin/stdout pipes should be integrated into the main non-blocking event loop.

Current CGI behavior should be checked with:

```bash
curl -v "http://127.0.0.1:8080/cgi-bin/test.py"
curl -v "http://127.0.0.1:8080/cgi-bin/slow.py"
```

Expected:

* normal CGI requests still return valid responses;
* long-running CGI requests do not block the whole server forever;
* CGI timeout returns `504 Gateway Timeout`;
* client disconnect during CGI does not leave child processes alive.

---

## 16. Debug output policy

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

## 17. Coverage checklist

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
| Correct working directory                          | 11            | Covered                           |
| CGI timeout / no indefinite hang                   | 12            | Covered                           |
| CGI cleanup on client disconnect                   | 13            | Covered                           |
| Non-blocking CGI pipes                             | 14            | Covered                           |
| Debug output guarded by macro                      | 15            | Cleanup needed                    |

---

## 18. Recommended minimum test run before closing CGI

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

Also run the CGI timeout test:

```bash
curl -v "http://127.0.0.1:8080/cgi-bin/slow.py"
```

Expected if the script exceeds the CGI timeout:

```http
HTTP/1.1 504 Gateway Timeout
```

Run the CGI client disconnect cleanup test:

```bash
curl -v "http://127.0.0.1:8080/cgi-bin/slow.py"
```

Press `Ctrl+C` while the request is still running.

Then check:

```bash
ps aux | grep '[s]low.py'
```

Expected:

```txt
<no output>
```

Finally verify that the server still works:

```bash
curl -v "http://127.0.0.1:8080/"
```

Expected:

```http
HTTP/1.1 200 OK
```

Only after these pass should the CGI documentation be considered reasonably complete for defense preparation.

---
