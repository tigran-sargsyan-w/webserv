# Client Max Body Size Tests

This document describes the basic tests used to verify the `client_max_body_size` directive in the `webserv` project.

The tested configuration is:

```conf
client_max_body_size 1048576;
```

This means:

```txt
1048576 bytes = 1 MB
```

Expected behavior:

```txt
body size <= 1048576 bytes  -> accepted
body size >  1048576 bytes  -> 413 Payload Too Large
```

---

## 1. Test files location

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

Check them with:

```bash
ls www/static/client-max-body-size-tests
```

---

## 2. Prepare the upload directory

The upload directory must exist before running POST upload tests:

```bash
mkdir -p www/uploads
```

---

## 3. Start the server

From the project root:

```bash
make re
./webserv configs/default.conf
```

The server should listen on:

```txt
127.0.0.1:8080
```

---

## 4. Test small upload

This test verifies that a normal body smaller than the limit is accepted.

```bash
curl -i -X POST http://127.0.0.1:8080/uploads/small.txt \
  --data-binary @www/static/client-max-body-size-tests/small.txt
```

Expected response:

```http
HTTP/1.1 201 Created
```

Verify the uploaded file:

```bash
cat www/uploads/small.txt
```

Expected output:

```txt
hello from small file
```

---

## 5. Test large upload over the limit

This test verifies that a body larger than `client_max_body_size` is rejected.

```bash
curl -i -X POST http://127.0.0.1:8080/uploads/bigfile.txt \
  --data-binary @www/static/client-max-body-size-tests/bigfile.txt
```

Expected response:

```http
HTTP/1.1 413 Payload Too Large
```

---

## 6. Test exact boundary: exactly 1 MB

This test verifies that a body exactly equal to the configured limit is accepted.

Check file size:

```bash
wc -c www/static/client-max-body-size-tests/exact_1mb.txt
```

Expected size:

```txt
1048576 www/static/client-max-body-size-tests/exact_1mb.txt
```

Send the request:

```bash
curl -i -X POST http://127.0.0.1:8080/uploads/exact_1mb.txt \
  -H "Expect:" \
  --data-binary @www/static/client-max-body-size-tests/exact_1mb.txt
```

Expected response:

```http
HTTP/1.1 201 Created
```

---

## 7. Test boundary: 1 byte over the limit

This test verifies that a body just one byte larger than the configured limit is rejected.

Check file size:

```bash
wc -c www/static/client-max-body-size-tests/over_1mb.txt
```

Expected size:

```txt
1048577 www/static/client-max-body-size-tests/over_1mb.txt
```

Send the request:

```bash
curl -i -X POST http://127.0.0.1:8080/uploads/over_1mb.txt \
  -H "Expect:" \
  --data-binary @www/static/client-max-body-size-tests/over_1mb.txt
```

Expected response:

```http
HTTP/1.1 413 Payload Too Large
```

This confirms the correct boundary behavior:

```txt
1048576 bytes -> accepted
1048577 bytes -> rejected
```

---

## 8. Test with Expect: 100-continue

This test verifies that `Expect: 100-continue` does not break normal upload handling.

```bash
curl -i -X POST http://127.0.0.1:8080/uploads/small_expect.txt \
  -H "Expect: 100-continue" \
  --data-binary @www/static/client-max-body-size-tests/small.txt
```

Expected response:

```http
HTTP/1.1 201 Created
```

---

## 9. Summary of expected results

| Test case                              | Expected result         |
| -------------------------------------- | ----------------------- |
| Small file upload                      | `201 Created`           |
| Big file upload                        | `413 Payload Too Large` |
| Exactly `1048576` bytes                | `201 Created`           |
| `1048577` bytes                        | `413 Payload Too Large` |
| Small file with `Expect: 100-continue` | `201 Created`           |

---

## 10. Clean up uploaded files

After testing, remove files created inside `www/uploads`:

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
