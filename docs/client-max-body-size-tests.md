# Client Max Body Size Tests

This document describes how to test the `client_max_body_size` directive in the `webserv` project.

The goal is to verify that the server:

* accepts request bodies smaller than or equal to `client_max_body_size`;
* rejects request bodies larger than `client_max_body_size`;
* returns `413 Payload Too Large` for oversized requests;
* waits for the full request body before parsing POST requests;
* handles `Expect: 100-continue` correctly.

---

## 1. Configuration used for the tests

The default configuration contains:

```conf
server {
    listen 127.0.0.1:8080;
    server_name default_server;

    root ./www;
    index index.html;
    client_max_body_size 1048576;

    location /uploads {
        methods GET POST DELETE;
        root ./www/uploads;
        autoindex off;
        upload_enable on;
        upload_store ./www/uploads;
    }
}
```

The important value is:

```conf
client_max_body_size 1048576;
```

This means the maximum allowed request body size is:

```txt
1048576 bytes = 1 MB
```

Expected behavior:

```txt
body size <= 1048576 bytes  -> accepted
body size >  1048576 bytes  -> 413 Payload Too Large
```

---

## 2. Test files location

The test files are stored in:

```txt
www/static/client-max-body-size-tests/
```

Expected files:

```txt
bigfile.txt
exact_1mb.txt
over_1mb.txt
small.txt
```

Check that the files exist:

```bash
ls www/static/client-max-body-size-tests
```

Expected output:

```txt
bigfile.txt  exact_1mb.txt  over_1mb.txt  small.txt
```

---

## 3. Prepare the upload directory

The upload directory must exist before running the POST tests.

From the project root:

```bash
mkdir -p www/uploads
```

If the directory does not exist, upload may fail because the server cannot resolve the configured `upload_store`.

---

## 4. Start the server

From the project root:

```bash
make re
./webserv configs/default.conf
```

Expected server output should include something similar to:

```txt
Listening on 127.0.0.1:8080
WebServ run called!
```

---

## 5. Test small upload

Send the small file with POST:

```bash
curl -i -X POST http://127.0.0.1:8080/uploads/small.txt \
  --data-binary @www/static/client-max-body-size-tests/small.txt
```

Expected response:

```http
HTTP/1.1 201 Created
```

Example:

```http
HTTP/1.1 201 Created
Connection: close
Content-Length: 57
Content-Type: text/html

<html><body><h1>Created: File uploaded</h1></body></html>
```

Verify that the file was uploaded:

```bash
cat www/uploads/small.txt
```

Expected output:

```txt
hello from small file
```

This confirms that normal POST upload still works.

---

## 6. Test large upload over the limit

The file `bigfile.txt` is larger than `client_max_body_size`.

Send it with POST:

```bash
curl -i -X POST http://127.0.0.1:8080/uploads/bigfile.txt \
  --data-binary @www/static/client-max-body-size-tests/bigfile.txt
```

Expected response:

```http
HTTP/1.1 413 Payload Too Large
```

Example:

```http
HTTP/1.1 413 Payload Too Large
Connection: close
Content-Length: 64
Content-Type: text/html

<html><body><h1>Custom 413 Payload Too Large</h1></body></html>
```

This confirms that the server rejects bodies larger than the configured limit.

---

## 7. Test exact boundary: exactly 1 MB

The file `exact_1mb.txt` must be exactly equal to the configured limit:

```txt
1048576 bytes
```

Check its size:

```bash
wc -c www/static/client-max-body-size-tests/exact_1mb.txt
```

Expected size:

```txt
1048576 www/static/client-max-body-size-tests/exact_1mb.txt
```

Send it with POST:

```bash
curl -i -X POST http://127.0.0.1:8080/uploads/exact_1mb.txt \
  -H "Expect:" \
  --data-binary @www/static/client-max-body-size-tests/exact_1mb.txt
```

Expected response:

```http
HTTP/1.1 201 Created
```

This confirms that a body exactly equal to the limit is accepted.

---

## 8. Test boundary: 1 byte over the limit

The file `over_1mb.txt` must be one byte larger than the configured limit:

```txt
1048577 bytes
```

Check its size:

```bash
wc -c www/static/client-max-body-size-tests/over_1mb.txt
```

Expected size:

```txt
1048577 www/static/client-max-body-size-tests/over_1mb.txt
```

Send it with POST:

```bash
curl -i -X POST http://127.0.0.1:8080/uploads/over_1mb.txt \
  -H "Expect:" \
  --data-binary @www/static/client-max-body-size-tests/over_1mb.txt
```

Expected response:

```http
HTTP/1.1 413 Payload Too Large
```

This confirms that the limit check uses:

```cpp
contentLength > clientMaxBodySize
```

and not:

```cpp
contentLength >= clientMaxBodySize
```

Correct behavior:

```txt
1048576 bytes -> accepted
1048577 bytes -> rejected
```

---

## 9. Test with Expect: 100-continue

Some clients, including `curl`, may send:

```http
Expect: 100-continue
```

for POST requests with a body.

The server should still correctly wait for the body and process the request.

Test with a small file:

```bash
curl -i -X POST http://127.0.0.1:8080/uploads/small_expect.txt \
  -H "Expect: 100-continue" \
  --data-binary @www/static/client-max-body-size-tests/small.txt
```

Expected response:

```http
HTTP/1.1 201 Created
```

Verify the uploaded file:

```bash
cat www/uploads/small_expect.txt
```

Expected output:

```txt
hello from small file
```

This confirms that `Expect: 100-continue` does not cause the server to parse the request before the body is received.

---

## 10. Test large body without Expect header

To force `curl` not to send the `Expect` header:

```bash
curl -i -X POST http://127.0.0.1:8080/uploads/bigfile.txt \
  -H "Expect:" \
  --data-binary @www/static/client-max-body-size-tests/bigfile.txt
```

Expected response:

```http
HTTP/1.1 413 Payload Too Large
```

This confirms that oversized body handling works even when the client sends the body immediately.

---

## 11. Optional: generate test files again

If the test files are missing, regenerate them:

```bash
mkdir -p www/static/client-max-body-size-tests

echo "hello from small file" > www/static/client-max-body-size-tests/small.txt

python3 - << 'EOF'
with open("www/static/client-max-body-size-tests/bigfile.txt", "wb") as f:
    f.write(b"A" * 2 * 1024 * 1024)

with open("www/static/client-max-body-size-tests/exact_1mb.txt", "wb") as f:
    f.write(b"A" * 1048576)

with open("www/static/client-max-body-size-tests/over_1mb.txt", "wb") as f:
    f.write(b"A" * 1048577)
EOF
```

Check sizes:

```bash
wc -c www/static/client-max-body-size-tests/*
```

Expected important values:

```txt
1048576 www/static/client-max-body-size-tests/exact_1mb.txt
1048577 www/static/client-max-body-size-tests/over_1mb.txt
2097152 www/static/client-max-body-size-tests/bigfile.txt
```

---

## 12. Clean up uploaded files

After testing, remove uploaded files from `www/uploads`:

```bash
rm -f www/uploads/small.txt
rm -f www/uploads/small_expect.txt
rm -f www/uploads/exact_1mb.txt
rm -f www/uploads/bigfile.txt
rm -f www/uploads/over_1mb.txt
```

Keep the upload directory in the repository with an empty `.gitkeep` file:

```bash
mkdir -p www/uploads
touch www/uploads/.gitkeep
```

---

## 13. Summary of expected results

| Test case                              | Expected result         |
| -------------------------------------- | ----------------------- |
| Small file upload                      | `201 Created`           |
| Small file with `Expect: 100-continue` | `201 Created`           |
| Body exactly `1048576` bytes           | `201 Created`           |
| Body `1048577` bytes                   | `413 Payload Too Large` |
| Body 2 MB                              | `413 Payload Too Large` |
| Large body without `Expect` header     | `413 Payload Too Large` |

---

## 14. What this test validates

These tests validate that:

* `client_max_body_size` is parsed from the configuration;
* the server checks `Content-Length` before accepting oversized bodies;
* oversized requests return `413 Payload Too Large`;
* valid POST bodies are not parsed before they are fully received;
* small uploads still work correctly;
* boundary behavior is correct;
* `Expect: 100-continue` does not break POST upload handling.
