# Non-blocking CGI Tests

## Static GET smoke test

### Purpose

Verify that the server still serves normal static files after the non-blocking CGI changes.

### Command

```bash
curl -v http://127.0.0.1:8080/
```

### Expected result

```txt
HTTP/1.1 200 OK
Content-Type: text/html
```

### Tested result

```txt
HTTP/1.1 200 OK
Connection: close
Content-Length: 225
Content-Type: text/html
```

---

## Basic CGI GET test

### Purpose

Verify that a CGI script can be executed and that CGI stdout is converted into an HTTP response.

### Command

```bash
curl -v http://127.0.0.1:8080/cgi-bin/echo.py
```

### Expected result

```txt
METHOD=GET
CONTENT_LENGTH=
BODY=
```

### Tested result

```txt
HTTP/1.1 200 OK
Content-Length: 33
Content-Type: text/plain

METHOD=GET
CONTENT_LENGTH=
BODY=
```

---

## CGI POST body test

### Purpose

Verify that the HTTP request body is correctly written to CGI stdin.

### Command

```bash
curl -v -X POST http://127.0.0.1:8080/cgi-bin/echo.py -d "hello=world"
```

### Expected result

```txt
METHOD=POST
CONTENT_LENGTH=11
BODY=hello=world
```

### Tested result

```txt
HTTP/1.1 200 OK
Content-Length: 47
Content-Type: text/plain

METHOD=POST
CONTENT_LENGTH=11
BODY=hello=world
```

---

## Slow CGI test

### Purpose

Verify that a slow CGI script correctly returns a response after finishing.

### Command

```bash
time curl -v http://127.0.0.1:8080/cgi-bin/sleep.py
```

### Expected result

```txt
done
```

The request should finish after around 10 seconds.

### Tested result

```txt
HTTP/1.1 200 OK
Content-Length: 5
Content-Type: text/plain

done

10.042 total
```

---

## Parallel slow CGI and static request test

### Purpose

Verify that multiple slow CGI requests do not block normal static requests.

### Command

```bash
curl -s http://127.0.0.1:8080/cgi-bin/sleep.py > /tmp/cgi_sleep_1.out &
curl -s http://127.0.0.1:8080/cgi-bin/sleep.py > /tmp/cgi_sleep_2.out &
curl -s http://127.0.0.1:8080/cgi-bin/sleep.py > /tmp/cgi_sleep_3.out &
time curl -s http://127.0.0.1:8080/ > /tmp/static.out
wait
```

Check CGI outputs:

```bash
cat /tmp/cgi_sleep_1.out
cat /tmp/cgi_sleep_2.out
cat /tmp/cgi_sleep_3.out
```

### Expected result

The static request should finish immediately.

Each CGI output should contain:

```txt
done
```

### Tested result

```txt
static request: 0.050 total

/tmp/cgi_sleep_1.out:
done

/tmp/cgi_sleep_2.out:
done

/tmp/cgi_sleep_3.out:
done
```

---

## Large CGI output test

### Purpose

Verify that a large CGI response is read fully and is not truncated.

### Command

```bash
curl -s http://127.0.0.1:8080/cgi-bin/big.py | tail
```

Count all output lines:

```bash
curl -s http://127.0.0.1:8080/cgi-bin/big.py | wc -l
```

### Expected result

The output should reach the last lines:

```txt
line 99995
line 99996
line 99997
line 99998
line 99999
```

The line count should be:

```txt
100000
```

### Tested result

```txt
line 99990
line 99991
line 99992
line 99993
line 99994
line 99995
line 99996
line 99997
line 99998
line 99999
```

Line count:

```txt
100000
```

---

## Static request during large CGI output test

### Purpose

Verify that a large CGI response does not block normal static requests.

### Command

```bash
curl -s http://127.0.0.1:8080/cgi-bin/big.py > /tmp/big.out &
time curl -s http://127.0.0.1:8080/ > /tmp/static_during_big.out
wait
wc -l /tmp/big.out
```

### Expected result

The static request should finish immediately.

The large CGI output should still be complete:

```txt
100000 /tmp/big.out
```

### Tested result

```txt
static request: 0.015 total
big.py output: 100000 /tmp/big.out
```

---

## Parallel POST CGI test

### Purpose

Verify that multiple POST CGI requests can run at the same time and that each CGI process receives its own request body.

### Command

```bash
curl -s -X POST http://127.0.0.1:8080/cgi-bin/echo.py -d "one=1" &
curl -s -X POST http://127.0.0.1:8080/cgi-bin/echo.py -d "two=2" &
curl -s -X POST http://127.0.0.1:8080/cgi-bin/echo.py -d "three=3" &
wait
```

### Expected result

Each response should contain the correct body for that request.

The response order may be different because the requests run in parallel.

### Tested result

```txt
METHOD=POST
CONTENT_LENGTH=7
BODY=three=3

METHOD=POST
CONTENT_LENGTH=5
BODY=one=1

METHOD=POST
CONTENT_LENGTH=5
BODY=two=2
```

---

## CGI failure test

### Purpose

Verify that a CGI script exiting with a non-zero status returns an HTTP error response instead of crashing or hanging.

### Command

```bash
curl -v http://127.0.0.1:8080/cgi-bin/fail.py
```

### Expected result

```txt
HTTP/1.1 502 Bad Gateway
```

### Tested result

```txt
HTTP/1.1 502 Bad Gateway
Connection: close
Content-Length: 58
Content-Type: text/html

<html><body><h1>Custom 502 Bad Gateway</h1></body></html>
```

---

## Server survival after failed CGI test

### Purpose

Verify that the server still accepts new requests after a CGI process fails.

### Command

```bash
curl -v http://127.0.0.1:8080/cgi-bin/fail.py
curl -v http://127.0.0.1:8080/
```

### Expected result

The failed CGI request should return an error response.

The following static request should return:

```txt
HTTP/1.1 200 OK
```

### Tested result

```txt
fail.py:
HTTP/1.1 502 Bad Gateway

after fail.py:
HTTP/1.1 200 OK
Content-Length: 225
Content-Type: text/html
```

---

## Server survival after all CGI tests

### Purpose

Verify that after several CGI scenarios, the server still accepts new clients.

### Command

```bash
curl -v http://127.0.0.1:8080/
```

### Expected result

```txt
HTTP/1.1 200 OK
```

### Tested result

```txt
HTTP/1.1 200 OK
Connection: close
Content-Length: 225
Content-Type: text/html
```

---

## Hanging CGI test

### Purpose

Verify that a hanging CGI script does not block the whole server.

This test is only a partial test until server-side CGI timeout is implemented.

### Command

```bash
timeout 3 curl -v http://127.0.0.1:8080/cgi-bin/hang.py
```

While the hanging CGI request is active, run:

```bash
curl -v http://127.0.0.1:8080/
```

### Expected result

Until server-side timeout is implemented, the `hang.py` request may stay pending until the client-side `timeout` command stops it.

The static request must still respond immediately.

### Future expected result

After server-side CGI timeout is implemented, the hanging CGI request should return:

```txt
HTTP/1.1 504 Gateway Timeout
```

---

## Quick regression checklist

```bash
curl -v http://127.0.0.1:8080/
curl -v http://127.0.0.1:8080/cgi-bin/echo.py
curl -v -X POST http://127.0.0.1:8080/cgi-bin/echo.py -d "hello=world"
time curl -v http://127.0.0.1:8080/cgi-bin/sleep.py
curl -s http://127.0.0.1:8080/cgi-bin/sleep.py > /tmp/cgi_sleep_1.out &
curl -s http://127.0.0.1:8080/cgi-bin/sleep.py > /tmp/cgi_sleep_2.out &
curl -s http://127.0.0.1:8080/cgi-bin/sleep.py > /tmp/cgi_sleep_3.out &
time curl -s http://127.0.0.1:8080/ > /tmp/static.out
wait
curl -s http://127.0.0.1:8080/cgi-bin/big.py | tail
curl -s http://127.0.0.1:8080/cgi-bin/big.py | wc -l
curl -s http://127.0.0.1:8080/cgi-bin/big.py > /tmp/big.out &
time curl -s http://127.0.0.1:8080/ > /tmp/static_during_big.out
wait
wc -l /tmp/big.out
curl -s -X POST http://127.0.0.1:8080/cgi-bin/echo.py -d "one=1" &
curl -s -X POST http://127.0.0.1:8080/cgi-bin/echo.py -d "two=2" &
curl -s -X POST http://127.0.0.1:8080/cgi-bin/echo.py -d "three=3" &
wait
curl -v http://127.0.0.1:8080/cgi-bin/fail.py
curl -v http://127.0.0.1:8080/
```

### Expected summary

| Test                               | Expected result                        |
| ---------------------------------- | -------------------------------------- |
| Static GET `/`                     | `200 OK`                               |
| `echo.py` GET                      | `METHOD=GET`                           |
| `echo.py` POST                     | body is echoed                         |
| `sleep.py`                         | returns `done` after around 10 seconds |
| Static request during 3 `sleep.py` | responds immediately                   |
| `big.py` tail                      | reaches `line 99999`                   |
| `big.py` line count                | `100000`                               |
| Static request during `big.py`     | responds immediately                   |
| Parallel POST CGI                  | all bodies returned correctly          |
| `fail.py`                          | returns `502 Bad Gateway`              |
| `/` after CGI tests                | server still responds                  |
