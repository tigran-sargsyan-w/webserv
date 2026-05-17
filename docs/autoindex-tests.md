# Autoindex Test Cases

This document contains manual test cases for the `autoindex-base` branch of the `webserv` project.

The goal is to verify that static file serving and autoindex directory handling work correctly with the current `configs/default.conf` configuration.

---

## 1. Preparation

### Start the server

From the project root:

```bash
make re
./webserv configs/default.conf
```

The server should start on:

```txt
127.0.0.1:8080
```

Expected server output should include something similar to:

```txt
Listening on 127.0.0.1:8080
WebServ run called!
```

---

## 2. Prepare test directories and files

Run these commands from the project root:

```bash
mkdir -p www/static/listing-test
printf "hello\n" > www/static/listing-test/a.txt
printf "world\n" > www/static/listing-test/b.txt

mkdir -p www/static/with-index
printf "<h1>Index works</h1>" > www/static/with-index/index.html

mkdir -p www/uploads
```

Expected structure:

```txt
www/
├── static/
│   ├── listing-test/
│   │   ├── a.txt
│   │   └── b.txt
│   └── with-index/
│       └── index.html
└── uploads/
```

---

## 3. Test: root index page

### Command

```bash
curl -i http://localhost:8080/
```

### Expected result

```http
HTTP/1.1 200 OK
```

The response body should contain the main index page, for example:

```html
<h1>Hello from WebServ!</h1>
```

### Purpose

Checks that normal static file serving still works after the autoindex changes.

---

## 4. Test: missing file

### Command

```bash
curl -i http://localhost:8080/no-such-file
```

### Expected result

```http
HTTP/1.1 404 Not Found
```

### Purpose

Checks that a missing static resource returns `404 Not Found`, not autoindex and not redirect.

---

## 5. Test: autoindex enabled on `/static/`

### Command

```bash
curl -i http://localhost:8080/static/
```

### Expected result

```http
HTTP/1.1 200 OK
Content-Type: text/html
```

The response body should contain an HTML directory listing:

```html
<h1>Index of /static/</h1>
```

It should contain links to directories with a trailing slash:

```html
<a href="/static/listing-test/">listing-test/</a>
<a href="/static/with-index/">with-index/</a>
```

It should not contain:

```txt
.
..
```

### Purpose

Checks that when a directory exists, has no index file, and `autoindex on` is configured, the server generates an HTML listing.

---

## 6. Test: autoindex listing for nested directory

### Command

```bash
curl -i http://localhost:8080/static/listing-test/
```

### Expected result

```http
HTTP/1.1 200 OK
Content-Type: text/html
```

The body should contain:

```html
<h1>Index of /static/listing-test/</h1>
<a href="/static/listing-test/a.txt">a.txt</a>
<a href="/static/listing-test/b.txt">b.txt</a>
```

The response must not contain:

```txt
..
```

### Purpose

Checks that autoindex works for nested directories and that `.` / `..` entries are hidden.

---

## 7. Test: directory with index file

### Command

```bash
curl -i http://localhost:8080/static/with-index/
```

### Expected result

```http
HTTP/1.1 200 OK
```

The response body should be:

```html
<h1>Index works</h1>
```

The response should not be an autoindex listing.

### Purpose

Checks that `index.html` has priority over autoindex. If an index file exists inside the requested directory, the server must serve the index file instead of generating a directory listing.

---

## 8. Test: static file inside autoindex directory

### Command

```bash
curl -i http://localhost:8080/static/listing-test/a.txt
```

### Expected result

```http
HTTP/1.1 200 OK
```

The response body should be:

```txt
hello
```

### Purpose

Checks that normal file serving inside a route using `autoindex on` still works correctly.

---

## 9. Test: autoindex disabled on `/uploads/`

### Command

```bash
curl -i http://localhost:8080/uploads/
```

### Expected result

```http
HTTP/1.1 403 Forbidden
```

### Purpose

Checks that when a directory exists but `autoindex off` is configured, the server refuses to list the directory.

---

## 10. Test: missing directory inside static route

### Command

```bash
curl -i http://localhost:8080/static/not-found/
```

### Expected result

```http
HTTP/1.1 404 Not Found
```

### Purpose

Checks that a missing directory returns `404 Not Found`.

---

## 11. Test: missing file inside static route

### Command

```bash
curl -i http://localhost:8080/static/not-found.txt
```

### Expected result

```http
HTTP/1.1 404 Not Found
```

### Purpose

Checks that a missing file inside a valid route returns `404 Not Found`.

---

## 12. Test: directory without trailing slash

### Command

```bash
curl -i http://localhost:8080/static/listing-test
```

### Expected result

Current accepted behavior:

```http
HTTP/1.1 200 OK
```

The response should contain an autoindex listing for:

```html
<h1>Index of /static/listing-test</h1>
```

Links inside the listing should still be valid, for example:

```html
<a href="/static/listing-test/a.txt">a.txt</a>
<a href="/static/listing-test/b.txt">b.txt</a>
```

### Purpose

Checks that a directory path without a trailing slash does not break autoindex generation.

### Note

A more polished behavior would be to return:

```http
HTTP/1.1 301 Moved Permanently
Location: /static/listing-test/
```

But this is optional for the current autoindex task.

---

## 13. Quick regression checklist

Before opening or merging the PR, run:

```bash
curl -i http://localhost:8080/
curl -i http://localhost:8080/no-such-file
curl -i http://localhost:8080/static/
curl -i http://localhost:8080/static/listing-test/
curl -i http://localhost:8080/static/with-index/
curl -i http://localhost:8080/static/listing-test/a.txt
curl -i http://localhost:8080/uploads/
curl -i http://localhost:8080/static/not-found/
curl -i http://localhost:8080/static/not-found.txt
curl -i http://localhost:8080/static/listing-test
```

Expected summary:

| Request                      | Expected status | Purpose                          |
| ---------------------------- | --------------: | -------------------------------- |
| `/`                          |        `200 OK` | Root index works                 |
| `/no-such-file`              | `404 Not Found` | Missing resource                 |
| `/static/`                   |        `200 OK` | Autoindex enabled                |
| `/static/listing-test/`      |        `200 OK` | Nested autoindex                 |
| `/static/with-index/`        |        `200 OK` | Index file priority              |
| `/static/listing-test/a.txt` |        `200 OK` | Static file serving              |
| `/uploads/`                  | `403 Forbidden` | Autoindex disabled               |
| `/static/not-found/`         | `404 Not Found` | Missing directory                |
| `/static/not-found.txt`      | `404 Not Found` | Missing file                     |
| `/static/listing-test`       |        `200 OK` | Directory without trailing slash |

---

## 14. Current accepted limitations

These are not blockers for the current autoindex task, but can be improved later:

* Directory listing order is not sorted.
* File names are not HTML-escaped.
* Static files currently use a simple `Content-Type`, often `text/html`.
* Directory without trailing slash returns listing directly instead of redirecting to `/dir/`.
* Path traversal protection should be handled as a separate security improvement.
