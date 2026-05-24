# Non-blocking CGI Test Guide

This document describes how to manually test the non-blocking CGI system in `webserv`.

The main goal is to verify not only that CGI scripts work, but also that a slow CGI script does **not block the whole server**.

---

## 1. Test environment

Use the branch:

```bash
git checkout non-blocking-cgi-base
```

Build the project:

```bash
make re
```

Run the server:

```bash
./webserv configs/default.conf
```

Default address:

```txt
127.0.0.1:8080
```

CGI route from `configs/default.conf`:

```txt
location /cgi-bin {
    methods GET POST;
    root ./www/cgi-bin;
    autoindex off;
    cgi .py /usr/bin/python3;
    cgi .php /usr/bin/php-cgi;
}
```

Current CGI test scripts:

```txt
www/cgi-bin/echo.py    - reads stdin and prints method/body info
www/cgi-bin/sleep.py   - waits 10 seconds, then returns "done"
www/cgi-bin/big.py     - prints a large response
www/cgi-bin/fail.py    - exits with status 1
www/cgi-bin/hang.py    - sleeps for a very long time
```

---

## 2. Basic CGI GET test

### Purpose

Verify that a normal CGI script can be executed and that the server returns a valid HTTP response.

### Command

```bash
curl -v http://127.0.0.1:8080/cgi-bin/echo.py
```

### Expected result

The response should be successful and the body should contain something similar to:

```txt
METHOD=GET
CONTENT_LENGTH=
BODY=
```

### What this validates

* CGI route matching works.
* Python CGI execution works.
* CGI stdout is converted into an HTTP response.
* The server does not crash after executing CGI.

---

## 3. CGI POST body test

### Purpose

Verify that request body data is written to the CGI process through CGI stdin.

### Command

```bash
curl -v -X POST http://127.0.0.1:8080/cgi-bin/echo.py -d "hello=world"
```

### Expected result

The response body should contain:

```txt
METHOD=POST
CONTENT_LENGTH=11
BODY=hello=world
```

### What this validates

* POST requests can reach CGI.
* Request body is passed to CGI stdin.
* CGI stdin pipe writing works.
* CGI stdout reading still works after writing request body.

---

## 4. Non-blocking sleep test

### Purpose

Verify the most important behavior of the new CGI system:

```txt
A slow CGI request must not block the whole server.
```

The script `sleep.py` waits 10 seconds before returning a response.

### Terminal 1

Run the slow CGI request:

```bash
time curl -v http://127.0.0.1:8080/cgi-bin/sleep.py
```

Expected body after around 10 seconds:

```txt
done
```

### Terminal 2

While Terminal 1 is still waiting, immediately run:

```bash
time curl -v http://127.0.0.1:8080/
```

### Expected result

The normal GET request should respond immediately.

It must **not** wait for `sleep.py` to finish.

### What this validates

* CGI execution does not block the main server loop.
* The server can continue handling normal client requests while a CGI process is running.
* `poll()` still processes normal client sockets while CGI stdout is pending.

### Failure symptom

If the normal GET request waits around 10 seconds and only responds after `sleep.py` finishes, then CGI is still blocking the server.

---

## 5. Parallel CGI and static requests

### Purpose

Verify that multiple slow CGI requests do not prevent static requests from being served.

### Command

```bash
curl -s http://127.0.0.1:8080/cgi-bin/sleep.py > /tmp/cgi_sleep_1.out &
curl -s http://127.0.0.1:8080/cgi-bin/sleep.py > /tmp/cgi_sleep_2.out &
curl -s http://127.0.0.1:8080/cgi-bin/sleep.py > /tmp/cgi_sleep_3.out &
time curl -s http://127.0.0.1:8080/ > /tmp/static.out
wait
```

### Expected result

The static request should finish quickly.

Each CGI output should eventually contain:

```txt
done
```

Check:

```bash
cat /tmp/cgi_sleep_1.out
cat /tmp/cgi_sleep_2.out
cat /tmp/cgi_sleep_3.out
```

### What this validates

* Several CGI processes can be active at the same time.
* CGI fd to client fd mapping works.
* Static requests are not blocked by active CGI requests.

---

## 6. Large CGI output test

### Purpose

Verify that large CGI output is read incrementally and is not truncated.

The script `big.py` prints many lines.

### Command

```bash
curl -s http://127.0.0.1:8080/cgi-bin/big.py | tail
```

### Expected result

The output should reach the final lines, for example:

```txt
line 99995
line 99996
line 99997
line 99998
line 99999
```

You can also count lines:

```bash
curl -s http://127.0.0.1:8080/cgi-bin/big.py | wc -l
```

Expected result should be close to:

```txt
100000
```

### What this validates

* CGI stdout is read in chunks.
* Large CGI output is not cut.
* The server does not depend on a single blocking `read()`.
* Response buffering works with a large CGI response.

---

## 7. CGI failure test

### Purpose

Verify that a CGI script exiting with a non-zero status produces an HTTP error response instead of crashing or hanging.

The script `fail.py` exits with status `1`.

### Command

```bash
curl -v http://127.0.0.1:8080/cgi-bin/fail.py
```

### Expected result

The server should return an error response, usually:

```txt
HTTP/1.1 502 Bad Gateway
```

### What this validates

* `waitpid(..., WNOHANG)` result is checked.
* Non-zero CGI exit status is detected.
* The client receives a valid HTTP error response.
* The server remains alive after CGI failure.

After the failed CGI request, verify that the server still works:

```bash
curl -v http://127.0.0.1:8080/
```

Expected result:

```txt
The server should still respond normally.
```

---

## 8. Hanging CGI test

### Purpose

Verify server behavior when a CGI process does not finish.

The script `hang.py` sleeps for a very long time.

### Command

```bash
timeout 3 curl -v http://127.0.0.1:8080/cgi-bin/hang.py
```

### Current expected result

If server-side CGI timeout is not implemented yet, this request is expected to stay pending until the client-side `timeout` command stops it.

This is a known limitation unless server-side CGI timeout has already been implemented.

### Important parallel check

While the hanging CGI request is active, run another request:

```bash
curl -v http://127.0.0.1:8080/
```

### Expected result

The normal request should still respond immediately.

### What this validates

* A hanging CGI does not block the whole server.
* The affected CGI client may remain pending if timeout is not implemented.
* Other clients are still handled normally.

### Future expected result after CGI timeout implementation

After server-side CGI timeout is implemented, this test should return:

```txt
HTTP/1.1 504 Gateway Timeout
```

or another clearly defined CGI timeout error response.

---

## 9. Server survival test after CGI requests

### Purpose

Verify that after several CGI requests, the server still accepts new clients.

### Commands

```bash
curl -v http://127.0.0.1:8080/cgi-bin/echo.py
curl -v http://127.0.0.1:8080/cgi-bin/sleep.py
curl -v http://127.0.0.1:8080/cgi-bin/fail.py
curl -v http://127.0.0.1:8080/
```

### Expected result

The server should not crash.

The last static request should still return a normal response.

### What this validates

* CGI cleanup does not break the event loop.
* CGI failure does not kill the server.
* Closed CGI fds are removed from `poll`.
* The server can continue accepting and responding to clients.

---

## 10. Quick regression checklist

Before considering the non-blocking CGI feature stable, run:

```bash
make re
./webserv configs/default.conf
```

Then in another terminal:

```bash
curl -v http://127.0.0.1:8080/cgi-bin/echo.py
curl -v -X POST http://127.0.0.1:8080/cgi-bin/echo.py -d "hello=world"
time curl -v http://127.0.0.1:8080/cgi-bin/sleep.py
curl -s http://127.0.0.1:8080/cgi-bin/big.py | tail
curl -v http://127.0.0.1:8080/cgi-bin/fail.py
timeout 3 curl -v http://127.0.0.1:8080/cgi-bin/hang.py
curl -v http://127.0.0.1:8080/
```

Expected summary:

| Test                             | Expected result                                          |
| -------------------------------- | -------------------------------------------------------- |
| `echo.py` GET                    | `METHOD=GET`                                             |
| `echo.py` POST                   | body is echoed                                           |
| `sleep.py`                       | returns `done` after around 10 seconds                   |
| static request during `sleep.py` | responds immediately                                     |
| `big.py`                         | response reaches `line 99999`                            |
| `fail.py`                        | returns `502 Bad Gateway` or configured CGI error        |
| `hang.py`                        | client-side timeout if server timeout is not implemented |
| `/` after CGI tests              | server still responds                                    |

---

## 11. Main success condition

The main success condition is not only that CGI returns a response.

The main success condition is:

```txt
While one CGI request is running, the server must still respond to other clients.
```

The most important test is:

```bash
time curl http://127.0.0.1:8080/cgi-bin/sleep.py
```

and, while it is still waiting:

```bash
time curl http://127.0.0.1:8080/
```

The second command must not wait 10 seconds.

---

## 12. Known current limitation

If server-side CGI timeout is not implemented yet, `hang.py` may keep its client connection pending.

This does not necessarily mean the whole non-blocking system failed.

The real failure is if `hang.py` blocks all other clients.

After CGI timeout is implemented, `hang.py` should be used to verify that the server returns `504 Gateway Timeout` or another defined timeout error response.

---

## 13. What should not happen

During any CGI test, the following behavior is incorrect:

```txt
- server crash
- empty reply from server
- all other requests blocked by one slow CGI
- static request waits for sleep.py
- failed CGI leaves client hanging forever without response
- big.py response is truncated
```
