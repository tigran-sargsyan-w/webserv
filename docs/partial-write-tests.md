# Partial Write Handling Tests

This document describes a small manual test set for verifying that the server correctly handles partial writes.

## Goal

Before this fix, the server could close the client connection after a single successful `send()` call, even if only part of the response had actually been sent.

This caused large responses to be truncated.

After the fix, each client keeps:

* a response buffer;
* the current write offset;
* a response-ready state.

The server continues sending the remaining data on later `POLLOUT` events and closes the connection only when the full response has been sent.

---

## Test 1 — Large static file

Create a large static file:

```bash
dd if=/dev/urandom of=www/static/big.bin bs=1M count=20
```

Start the server:

```bash
make re
./webserv configs/default.conf
```

Download the file:

```bash
curl -o /tmp/big.bin http://localhost:8080/static/big.bin
```

Compare file sizes:

```bash
stat -c%s www/static/big.bin
stat -c%s /tmp/big.bin
```

Expected result:

```txt
20971520
20971520
```

Verify that the files are identical:

```bash
cmp www/static/big.bin /tmp/big.bin
echo $?
```

Expected result:

```txt
0
```

---

## Test 2 — Slow client

This test simulates a slow client by limiting the download speed.

```bash
rm -f /tmp/big.bin
curl --limit-rate 100k -o /tmp/big.bin http://localhost:8080/static/big.bin
```

Verify the downloaded file:

```bash
cmp www/static/big.bin /tmp/big.bin
echo $?
```

Expected result:

```txt
0
```

---

## Expected behavior

The server should:

* send the full response body;
* keep the connection open while data remains to be sent;
* continue sending on later `POLLOUT` events;
* close the connection only after the full response has been sent.

---

## Failure example before the fix

Before the fix, the slow-client test could fail with:

```txt
curl: (18) transfer closed with X bytes remaining to read
```

This meant that the server closed the connection before sending the full response.

---

## Notes

A successful test does not require the server to send the whole response in one `send()` call.

The important point is that the final downloaded file must be identical to the original file.
