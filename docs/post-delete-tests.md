# POST and DELETE Test Cases

This document contains manual test cases for the `POST` (file upload) and `DELETE` (file deletion) methods implemented in `RequestHandler`.

The goal is to verify that every possible outcome of `handlePost` and `handleHttpDelete` is covered, including security edge cases.

---

## 1. How uploads and deletions work

### POST

`handlePost` writes the request body as a file to `uploadStore`. It requires:

- `upload_enable on` in the route config
- a valid `upload_store` path
- a non-empty request body
- a valid, safe filename in the request path

### Upload authorization (per route)

Before `handlePost` runs, `handleRequest` checks that the HTTP method is listed in the route `methods` directive.

| Situation | Where it is checked | Expected status |
|-----------|---------------------|-----------------|
| `POST` not in `methods` (e.g. `/static`) | `validateMethod` in `handleRequest` | `405 Method Not Allowed` + `Allow` header |
| `POST` allowed but `upload_enable off` (e.g. `/`) | `handlePost` | `403 Forbidden` |
| `POST` allowed and `upload_enable on` with valid `upload_store` (e.g. `/uploads`) | `handlePost` | `201 Created` (if body and path are valid) |

With `configs/default.conf`, you do **not** need an extra `location /no-upload`: `location /` already has `methods GET POST DELETE` and `upload_enable off`.

### DELETE

`handleHttpDelete` deletes a file from `uploadStore`. It requires:

- a valid, safe filename in the request path
- the file to exist and not be a directory

Both methods use `getSafeUploadPath` to resolve the destination path, which:

1. URL-decodes the request path
2. Checks for path traversal sequences (`..`), null bytes, or empty names
3. Extracts and isolates the filename safely
4. Concatenates the configured `upload_store` path with the verified filename
5. Returns an empty string if any security check fails, triggering a `400 Bad Request` for POST or a `403 Forbidden` for DELETE

---

## 2. Preparation

### Config

Make sure your config contains a route similar to:

```conf
location /uploads {
    methods GET POST DELETE;
    root ./www/uploads;
    autoindex off;
    upload_enable on;
    upload_store ./www/uploads;
}
```

And a route without upload enabled, for example:

```conf
location /static {
    methods GET;
    root ./www/static;
    autoindex off;
}
```

### Start the server

```bash
make re
./webserv configs/default.conf
```

Expected output:

```txt
Listening on 127.0.0.1:8080
```

### Make sure the upload directory exists

```bash
mkdir -p www/uploads
```

---

## 3. POST tests

### 3.1 Successful upload

```bash
curl -X POST http://localhost:8080/uploads/test.txt \
  --data-binary "hello world" -v
```

Expected:

```http
HTTP/1.1 201 Created
```

```html
<html><body><h1>Created: File uploaded</h1></body></html>
```

Verify the file exists on disk:

```bash
cat www/uploads/test.txt
```

Expected output:

```txt
hello world
```

---

### 3.2 Upload authorization (per route)

#### 3.2.a POST not in route methods

```bash
curl -X POST http://localhost:8080/static/test.txt \
  --data-binary "hello" -v
```

Expected:

```http
HTTP/1.1 405 Method Not Allowed
Allow: GET
```

Rejection happens in `validateMethod` before `handlePost` is even called.

---

#### 3.2.b POST allowed but upload_enable off

With `configs/default.conf`, `location /` has `methods GET POST DELETE` and `upload_enable off`, so it covers this case directly. No extra route needed.

```bash
curl -X POST http://localhost:8080/test.txt \
  --data-binary "hello" -v
```

Expected:

```http
HTTP/1.1 403 Forbidden
```

```html
<html><body><h1>403 Forbidden: Upload is disabled for this route</h1></body></html>
```

This is checked inside `handlePost` after method validation passes.

---

### 3.3 Empty body

```bash
curl -X POST http://localhost:8080/uploads/test.txt \
  --data-binary "" -v
```

Expected:

```http
HTTP/1.1 400 Bad Request
```

```html
<html><body><h1>400 Bad request: Empty body</h1></body></html>
```

---

### 3.4 Empty filename (path ends with `/`)

```bash
curl -X POST http://localhost:8080/uploads/ \
  --data-binary "hello" -v
```

Expected:

```http
HTTP/1.1 400 Bad Request
```

```html
<html><body><h1>400 Bad Request: Invalid file name</h1></body></html>
```

---

### 3.5 Path traversal with `..` (plain)

Check the size before:
```bash
wc -c /etc/passwd
```

Then do the test:

```bash
curl -X POST "http://localhost:8080/uploads/../etc/passwd" \
  --data-binary "hack" --path-as-is -v
```

Expected:

```http
HTTP/1.1 400 Bad Request
```

```html
<html><body><h1>400 Bad Request: Invalid file name</h1></body></html>
```

Verify nothing was written by checking the size after is identical to the size before:

```bash
wc -c /etc/passwd
```

The file must not have been modified.

---

### 3.6 Path traversal with URL encoding (`%2F`, `%2E%2E`)

```bash
curl -X POST "http://localhost:8080/uploads/..%2Fetc%2Fpasswd" \
  --data-binary "hack" --path-as-is -v
```

Expected:

```http
HTTP/1.1 400 Bad Request
```

```html
<html><body><h1>400 Bad Request: Invalid file name</h1></body></html>
```

---

### 3.7 Null byte in path

```bash
curl -X POST "http://localhost:8080/uploads/test%00.txt" \
  --data-binary "hello" --path-as-is -v
```

Expected:

```http
HTTP/1.1 400 Bad Request
```

```html
<html><body><h1>400 Bad Request: Invalid file name</h1></body></html>
```

---

### 3.8 Upload to a path that is a directory

```bash
mkdir -p www/uploads/mydir
curl -X POST http://localhost:8080/uploads/mydir \
  --data-binary "hello" -v
```

Expected:

```http
HTTP/1.1 409 Conflict
```

```html
<html><body><h1>409 Conflict: A directory with this name already exists</h1></body></html>
```

Cleanup:

```bash
rmdir www/uploads/mydir
```

---

### 3.9 Overwrite an existing file

```bash
curl -X POST http://localhost:8080/uploads/test.txt \
  --data-binary "first version" -v

curl -X POST http://localhost:8080/uploads/test.txt \
  --data-binary "second version" -v
```

Both expected to return:

```http
HTTP/1.1 201 Created
```

Verify the file contains the second version:

```bash
cat www/uploads/test.txt
```

Expected output:

```txt
second version
```

---

### 3.10 Binary file upload

```bash
curl -X POST http://localhost:8080/uploads/image.bin \
  --data-binary @/bin/ls -v
```

Expected:

```http
HTTP/1.1 201 Created
```

Verify the uploaded file has the same size as the original:

```bash
wc -c www/uploads/image.bin
wc -c /bin/ls
```

Both values must match. Then confirm the content is byte-for-byte identical, not just the same size:

```bash
diff /bin/ls www/uploads/image.bin && echo "IDENTICAL"
```

Expected output:

```txt
IDENTICAL
```

`diff` exits silently with status 0 when files are identical; any byte difference (e.g. body corruption during write) would print the mismatch.

---

### 3.11 Body too large (exceeds client_max_body_size)

This is the key case proving large uploads are bounded and rejected cleanly, not buffered until the server runs out of memory.

Make sure the upload route (or server block) has a small limit, e.g.:

```conf
client_max_body_size 1m;
```

Generate a file larger than the limit and upload it:

```bash
head -c 5M /dev/urandom > big.bin
curl -X POST http://localhost:8080/uploads/big.bin \
  --data-binary @big.bin -v
```

Expected:

```http
HTTP/1.1 413 Payload Too Large
```

The rejection must happen during request inspection (before the full body is buffered), and no file should be written:

```bash
ls www/uploads/big.bin
```

Expected:

```txt
ls: cannot access 'www/uploads/big.bin': No such file or directory
```

Cleanup:

```bash
rm -f big.bin
```

---

### 3.12 Upload to route path without a trailing filename

A POST on the route prefix itself (no trailing `/`) uses the last path segment as the filename, so the file is literally named after the route.

```bash
curl -X POST http://localhost:8080/uploads \
  --data-binary "edge case" -v
```

Expected:

```http
HTTP/1.1 201 Created
```

Verify a file named `uploads` was created inside the store:

```bash
cat www/uploads/uploads
```

Expected output:

```txt
edge case
```

Cleanup:

```bash
rm -f www/uploads/uploads
```

## 4. DELETE tests

### 4.1 Successful deletion

First upload a file:

```bash
curl -X POST http://localhost:8080/uploads/todelete.txt \
  --data-binary "bye" -v
```

Then delete it:

```bash
curl -X DELETE http://localhost:8080/uploads/todelete.txt -v
```

Expected:

```http
HTTP/1.1 200 OK
```

```html
<html><body><h1>File deleted successfully</h1></body></html>
```

Verify the file no longer exists:

```bash
ls www/uploads/todelete.txt
```

Expected:

```txt
ls: cannot access 'www/uploads/todelete.txt': No such file or directory
```

---

### 4.2 Delete a non-existent file

```bash
curl -X DELETE http://localhost:8080/uploads/doesnotexist.txt -v
```

Expected:

```http
HTTP/1.1 404 Not Found
```

```html
<html><body><h1>404 Not Found: Resource does not exist</h1></body></html>
```

---

### 4.3 Double deletion

```bash
curl -X POST http://localhost:8080/uploads/double.txt \
  --data-binary "hello" -v

curl -X DELETE http://localhost:8080/uploads/double.txt -v
curl -X DELETE http://localhost:8080/uploads/double.txt -v
```

Expected responses:

```http
HTTP/1.1 201 Created
HTTP/1.1 200 OK
HTTP/1.1 404 Not Found
```

---

### 4.4 Delete a directory

```bash
mkdir -p www/uploads/mydir
curl -X DELETE http://localhost:8080/uploads/mydir -v
```

Expected:

```http
HTTP/1.1 403 Forbidden
```

```html
<html><body><h1>403 Forbidden: Cannot delete a directory</h1></body></html>
```

Cleanup:

```bash
rmdir www/uploads/mydir
```

---

### 4.5 Empty filename (path ends with `/`)

```bash
curl -X DELETE http://localhost:8080/uploads/ -v
```

Expected:

```http
HTTP/1.1 403 Forbidden
```

```html
<html><body><h1>403 Forbidden: Invalid path sequence</h1></body></html>
```

---

### 4.6 Path traversal with `..` (plain)

```bash
curl -X DELETE "http://localhost:8080/uploads/../etc/passwd" \
  --path-as-is -v
```

Expected:

```http
HTTP/1.1 403 Forbidden
```

```html
<html><body><h1>403 Forbidden: Invalid path sequence</h1></body></html>
```

---

### 4.7 Path traversal with URL encoding

```bash
curl -X DELETE "http://localhost:8080/uploads/..%2Fetc%2Fpasswd" \
  --path-as-is -v
```

Expected:

```http
HTTP/1.1 403 Forbidden
```

```html
<html><body><h1>403 Forbidden: Invalid path sequence</h1></body></html>
```

---

### 4.8 Null byte in path

```bash
curl -X DELETE "http://localhost:8080/uploads/test%00.txt" \
  --path-as-is -v
```

Expected:

```http
HTTP/1.1 403 Forbidden
```

```html
<html><body><h1>403 Forbidden: Invalid path sequence</h1></body></html>
```

---

### 4.9 Method not allowed on route

```bash
curl -X DELETE http://localhost:8080/static/test.txt -v
```

Expected:

```http
HTTP/1.1 405 Method Not Allowed
Allow: GET
```

---

## 5. Quick regression checklist

Before opening or merging the PR, run:

```bash
# POST
curl -X POST http://localhost:8080/uploads/test.txt --data-binary "hello world" -v
curl -X POST http://localhost:8080/uploads/ --data-binary "hello" -v
curl -X POST http://localhost:8080/uploads/test.txt --data-binary "" -v
curl -X POST "http://localhost:8080/uploads/..%2Fetc%2Fpasswd" --data-binary "hack" --path-as-is -v
curl -X POST http://localhost:8080/uploads/big.bin --data-binary @big.bin -v
curl -X POST http://localhost:8080/uploads --data-binary "edge case" -v

# DELETE
curl -X DELETE http://localhost:8080/uploads/test.txt -v
curl -X POST http://localhost:8080/static/test.txt --data-binary "hello" -v
curl -X POST http://localhost:8080/test.txt --data-binary "hello" -v
curl -X DELETE http://localhost:8080/uploads/doesnotexist.txt -v
curl -X DELETE http://localhost:8080/uploads/ -v
curl -X DELETE "http://localhost:8080/uploads/..%2Fetc%2Fpasswd" --path-as-is -v
```

Expected summary:

| Request | Expected status | Purpose |
|---|---|---|
| `POST /uploads/test.txt` with body | `201 Created` | Normal upload |
| `POST /static/test.txt` with body | `405 Method Not Allowed` | POST not in route methods |
| `POST /test.txt` with body | `403 Forbidden` | upload_enable off |
| `POST /uploads/` with body | `400 Bad Request` | Empty filename |
| `POST /uploads/test.txt` empty body | `400 Bad Request` | Empty body |
| `POST /uploads/..%2Fetc%2Fpasswd` | `400 Bad Request` | Traversal blocked |
| `DELETE /uploads/test.txt` (exists) | `200 OK` | Normal deletion |
| `DELETE /uploads/doesnotexist.txt` | `404 Not Found` | File not found |
| `DELETE /uploads/` | `403 Forbidden` | Empty filename |
| `DELETE /uploads/..%2Fetc%2Fpasswd` | `403 Forbidden` | Traversal blocked |
| `POST /uploads/big.bin` oversized | `413 Payload Too Large` | Size limit enforced |
| `POST /uploads` (no filename) | `201 Created` | Last path segment as name |
