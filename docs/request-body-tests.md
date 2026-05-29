# Request Body Reading Tests

This document describes a small manual test set for verifying that the server correctly reads request bodies based on the `Content-Length` header.

## Goal

Before this fix, the server did not read request bodies reliably.

Two problems existed:

* The raw request was appended with `append(buffer)`, which stops at the first null byte. Any binary body (for example an executable) was silently truncated, so the body never reached its declared size and the server waited forever for more data.
* A malformed or inconsistent `Content-Length` was treated as "no body", so the request was accepted instead of being rejected.

After the fix, the server:

* reads the exact number of bytes given by `Content-Length`, null bytes included;
* keeps waiting while the received body is shorter than `Content-Length` (partial read);
* marks the request complete only once the full body has been received;
* rejects invalid or inconsistent `Content-Length` values with `400 Bad Request`.

---

## Test 1 — Complete text body

Create a small text file:

```bash
echo "small text payload" > /tmp/body.txt
```

Start the server:

```bash
make re
./webserv configs/default.conf
```

Upload it:

```bash
curl http://localhost:8080/uploads/body.txt --data-binary @/tmp/body.txt
```

Expected result:

```txt
HTTP/1.1 201 Created
```

---

## Test 2 — Complete binary body (null bytes)

This is the case that exposed the original bug: a binary file contains null bytes.

```bash
curl http://localhost:8080/uploads/image.bin --data-binary @/bin/ls
```

Expected result:

```txt
HTTP/1.1 201 Created
```

Verify that the uploaded file is identical to the original, byte for byte
(replace `UPLOAD_DIR` with the directory where the server stores uploads):

```bash
cmp /bin/ls UPLOAD_DIR/image.bin
echo $?
```

Expected result:

```txt
0
```

No output from `cmp` means the files are identical.

---

## Test 3 — Partial body (incomplete read)

Send a body shorter than the announced `Content-Length`. The server must keep
waiting instead of answering, so the request times out on the client side.

```bash
curl --max-time 3 \
     -H "Content-Length: 999999" \
     -H "Content-Type: text/plain" \
     --data "too short" \
     http://localhost:8080/uploads/partial.txt
echo $?
```

Expected result:

```txt
28
```

`curl` exit code `28` means the operation timed out: the server correctly kept
the request open instead of treating an incomplete body as complete.

---

## Test 4 — Invalid Content-Length

A non-numeric `Content-Length` must be rejected.

```bash
printf 'POST /uploads/x HTTP/1.1\r\nHost: localhost\r\nContent-Length: abc\r\n\r\nhello' \
  | nc localhost 8080
```

Expected result:

```txt
HTTP/1.1 400 Bad Request
```

A negative value must also be rejected:

```bash
printf 'POST /uploads/x HTTP/1.1\r\nHost: localhost\r\nContent-Length: -5\r\n\r\nhello' \
  | nc localhost 8080
```

Expected result:

```txt
HTTP/1.1 400 Bad Request
```

---

## Test 5 — Inconsistent Content-Length

Two `Content-Length` headers with different values must be rejected.

```bash
printf 'POST /uploads/x HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nContent-Length: 9\r\n\r\nhello' \
  | nc localhost 8080
```

Expected result:

```txt
HTTP/1.1 400 Bad Request
```

---

## Expected behavior

The server should:

* read exactly `Content-Length` bytes, including null bytes;
* keep the connection open while the body is incomplete;
* mark the request complete only after the full body has been received;
* reject invalid or inconsistent `Content-Length` values with `400 Bad Request`.

---

## Failure example before the fix

Before the fix, the binary upload test (Test 2) could hang forever:

```txt
* We are completely uploaded and fine
^C
```

The client finished uploading, but the server never responded because the
truncated body never reached its declared `Content-Length`.

---

## Notes

A successful test does not require the whole body to arrive in a single `recv()`
call. Large bodies are read across several `POLLIN` events.

The important point is that the request is processed only once every byte of the
body has been received, and that the stored body matches the original input
exactly (verified with `cmp` in Test 2).