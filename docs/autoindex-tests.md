# Autoindex Test Cases

This document contains manual test cases for the `autoindex-base` branch of the `webserv` project.

The goal is to verify that static file serving and autoindex directory handling work correctly with the current `configs/default.conf` configuration.

Covered features:

* static file serving;
* directory index resolution;
* `autoindex on` and `autoindex off` behavior;
* sorted autoindex output;
* HTML escaping in generated listings;
* URL encoding in autoindex links;
* URL decoding for static file requests;
* MIME types for static files;
* fallback MIME type for unknown file extensions.

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

mkdir -p www/static/sort-test/assets
mkdir -p www/static/sort-test/css
mkdir -p www/static/sort-test/scripts
printf "z\n" > www/static/sort-test/z.txt
printf "a\n" > www/static/sort-test/a.txt
printf "m\n" > www/static/sort-test/m.txt

mkdir -p www/static/escape-test
printf "amp\n" > 'www/static/escape-test/a&b.txt'
printf "tag\n" > 'www/static/escape-test/<tag>.txt'
printf "quote\n" > 'www/static/escape-test/"quote".txt'
printf "single\n" > "www/static/escape-test/'single'.txt"

mkdir -p www/static/url-test
printf "space\n" > 'www/static/url-test/my file.txt'
printf "hash\n" > 'www/static/url-test/hash#file.txt'
printf "query\n" > 'www/static/url-test/query?file.txt'
printf "percent\n" > 'www/static/url-test/percent%file.txt'
printf "amp\n" > 'www/static/url-test/a&b.txt'

mkdir -p www/static/mime-test
printf "<h1>Hello</h1>\n" > www/static/mime-test/index.html
printf "body { color: red; }\n" > www/static/mime-test/style.css
printf "console.log('hello');\n" > www/static/mime-test/script.js
printf '{"ok": true}\n' > www/static/mime-test/data.json
printf "plain text\n" > www/static/mime-test/file.txt
printf "a,b,c\n" > www/static/mime-test/test.csv
printf "<root></root>\n" > www/static/mime-test/test.xml
printf "fake wasm\n" > www/static/mime-test/test.wasm
printf "fake mp3\n" > www/static/mime-test/test.mp3
printf "fake wav\n" > www/static/mime-test/test.wav
printf "fake mp4\n" > www/static/mime-test/test.mp4
printf "fake webm\n" > www/static/mime-test/test.webm
printf "fake woff\n" > www/static/mime-test/test.woff
printf "fake woff2\n" > www/static/mime-test/test.woff2
printf "fake ttf\n" > www/static/mime-test/test.ttf
printf "unknown content\n" > www/static/mime-test/unknown.abc

mkdir -p www/uploads
```

---

## 3. Test: root index page

```bash
curl -i http://localhost:8080/
```

Expected:

```http
HTTP/1.1 200 OK
```

Purpose: checks that normal static file serving still works.

---

## 4. Test: missing file

```bash
curl -i http://localhost:8080/no-such-file
```

Expected:

```http
HTTP/1.1 404 Not Found
```

Purpose: missing static resource should return `404 Not Found`.

---

## 5. Test: autoindex enabled on `/static/`

```bash
curl -i http://localhost:8080/static/
```

Expected:

```http
HTTP/1.1 200 OK
Content-Type: text/html
```

Body should contain:

```html
<h1>Index of /static/</h1>
```

It should contain directory links with trailing slash:

```html
<a href="/static/listing-test/">listing-test/</a>
<a href="/static/with-index/">with-index/</a>
```

It should not contain:

```txt
.
..
```

---

## 6. Test: autoindex listing for nested directory

```bash
curl -i http://localhost:8080/static/listing-test/
```

Expected:

```http
HTTP/1.1 200 OK
Content-Type: text/html
```

Body should contain:

```html
<h1>Index of /static/listing-test/</h1>
<a href="/static/listing-test/a.txt">a.txt</a>
<a href="/static/listing-test/b.txt">b.txt</a>
```

Response must not contain:

```txt
..
```

---

## 7. Test: directory with index file

```bash
curl -i http://localhost:8080/static/with-index/
```

Expected:

```http
HTTP/1.1 200 OK
```

Body:

```html
<h1>Index works</h1>
```

Purpose: `index.html` has priority over autoindex.

---

## 8. Test: static file inside autoindex directory

```bash
curl -i http://localhost:8080/static/listing-test/a.txt
```

Expected:

```http
HTTP/1.1 200 OK
Content-Type: text/plain
```

Body:

```txt
hello
```

---

## 9. Test: autoindex disabled on `/uploads/`

```bash
curl -i http://localhost:8080/uploads/
```

Expected:

```http
HTTP/1.1 403 Forbidden
```

Purpose: existing directory with `autoindex off` must not be listed.

---

## 10. Test: missing directory inside static route

```bash
curl -i http://localhost:8080/static/not-found/
```

Expected:

```http
HTTP/1.1 404 Not Found
```

---

## 11. Test: missing file inside static route

```bash
curl -i http://localhost:8080/static/not-found.txt
```

Expected:

```http
HTTP/1.1 404 Not Found
```

---

## 12. Test: sorted autoindex output

```bash
curl -s http://localhost:8080/static/sort-test/
```

Expected order:

```txt
assets/
css/
scripts/
a.txt
m.txt
z.txt
```

Quick check:

```bash
curl -s http://localhost:8080/static/sort-test/ | grep -oE 'assets/|css/|scripts/|a\.txt|m\.txt|z\.txt'
```

Expected output:

```txt
assets/
css/
scripts/
a.txt
m.txt
z.txt
```

Purpose: directories first, then files; both groups sorted by name.

---

## 13. Test: HTML escaping in autoindex output

```bash
curl -s http://localhost:8080/static/escape-test/
```

Expected escaped fragments:

```html
a&amp;b.txt
&lt;tag&gt;.txt
&quot;quote&quot;.txt
&#39;single&#39;.txt
```

Quick check:

```bash
curl -s http://localhost:8080/static/escape-test/ | grep -E '&amp;|&lt;|&gt;|&quot;|&#39;'
```

Purpose: file names must not be inserted into HTML as raw unsafe text.

---

## 14. Test: URL encoding in autoindex links

```bash
curl -s http://localhost:8080/static/url-test/
```

Expected `href` fragments:

```html
href="/static/url-test/a%26b.txt"
href="/static/url-test/hash%23file.txt"
href="/static/url-test/my%20file.txt"
href="/static/url-test/percent%25file.txt"
href="/static/url-test/query%3Ffile.txt"
```

Visible text should stay readable:

```html
a&amp;b.txt
hash#file.txt
my file.txt
percent%file.txt
query?file.txt
```

Quick check:

```bash
curl -s http://localhost:8080/static/url-test/ | grep -oE '%20|%23|%3F|%25|%26'
```

Expected encoded fragments:

```txt
%26
%23
%20
%25
%3F
```

---

## 15. Test: URL decoding when opening encoded autoindex links

```bash
curl -i 'http://localhost:8080/static/url-test/a%26b.txt'
curl -i 'http://localhost:8080/static/url-test/hash%23file.txt'
curl -i 'http://localhost:8080/static/url-test/my%20file.txt'
curl -i 'http://localhost:8080/static/url-test/percent%25file.txt'
curl -i 'http://localhost:8080/static/url-test/query%3Ffile.txt'
```

Expected for each request:

```http
HTTP/1.1 200 OK
```

Expected bodies:

```txt
a&b.txt          -> amp
hash#file.txt    -> hash
my file.txt      -> space
percent%file.txt -> percent
query?file.txt   -> query
```

Purpose: encoded links generated by autoindex must resolve to the correct file on disk.

---

## 16. Test: query string should not break static path resolution

```bash
curl -i 'http://localhost:8080/static/url-test/my%20file.txt?hello=world'
```

Expected:

```http
HTTP/1.1 200 OK
```

Body:

```txt
space
```

Purpose: raw query strings are removed before filesystem resolution, while encoded characters in the path are still decoded correctly.

---

## 17. Test: MIME types for static files

Use normal `GET` requests with `curl -i`.

Do not use `curl -I` for this test unless `HEAD` is implemented.  
`curl -I` sends a `HEAD` request, not a normal `GET` request.

```bash
curl -i http://localhost:8080/static/mime-test/index.html
curl -i http://localhost:8080/static/mime-test/style.css
curl -i http://localhost:8080/static/mime-test/script.js
curl -i http://localhost:8080/static/mime-test/data.json
curl -i http://localhost:8080/static/mime-test/file.txt
curl -i http://localhost:8080/static/mime-test/test.csv
curl -i http://localhost:8080/static/mime-test/test.xml
curl -i http://localhost:8080/static/mime-test/test.wasm
curl -i http://localhost:8080/static/mime-test/test.mp3
curl -i http://localhost:8080/static/mime-test/test.wav
curl -i http://localhost:8080/static/mime-test/test.mp4
curl -i http://localhost:8080/static/mime-test/test.webm
curl -i http://localhost:8080/static/mime-test/test.woff
curl -i http://localhost:8080/static/mime-test/test.woff2
curl -i http://localhost:8080/static/mime-test/test.ttf
curl -i http://localhost:8080/static/mime-test/unknown.abc
```

Expected:

| File          | Expected `Content-Type`    |
| ------------- | -------------------------- |
| `index.html`  | `text/html`                |
| `style.css`   | `text/css`                 |
| `script.js`   | `application/javascript`   |
| `data.json`   | `application/json`         |
| `file.txt`    | `text/plain`               |
| `test.csv`    | `text/csv`                 |
| `test.xml`    | `application/xml`          |
| `test.wasm`   | `application/wasm`         |
| `test.mp3`    | `audio/mpeg`               |
| `test.wav`    | `audio/wav`                |
| `test.mp4`    | `video/mp4`                |
| `test.webm`   | `video/webm`               |
| `test.woff`   | `font/woff`                |
| `test.woff2`  | `font/woff2`               |
| `test.ttf`    | `font/ttf`                 |
| `unknown.abc` | `application/octet-stream` |

Example expected response for `unknown.abc`:

```http
HTTP/1.1 200 OK
Connection: close
Content-Type: application/octet-stream
```

Purpose:

* verifies MIME mapping for common web assets;
* verifies additional useful static file types;
* verifies fallback behavior for unknown extensions;
* verifies that MIME detection is integrated into normal static file responses.

---

## 18. Test: path traversal protection

Use `--path-as-is` so that `curl` does not normalize `../` before sending the request to the server.

```bash
curl --path-as-is -i http://localhost:8080/static/../index.html
curl --path-as-is -i http://localhost:8080/static/../../../../etc/passwd
curl --path-as-is -i http://localhost:8080/static/%2E%2E/index.html
curl --path-as-is -i http://localhost:8080/static/%2E%2E/%2E%2E/etc/passwd
```

Expected for each request:

```http
HTTP/1.1 403 Forbidden
```

Purpose: raw `../` and encoded `%2E%2E` path traversal attempts must be rejected before building the filesystem path.

Regression check: file names that contain dots but are not a `..` path segment should still work.

```bash
printf "dots\n" > www/static/listing-test/file..name.txt
curl -i http://localhost:8080/static/listing-test/file..name.txt
```

Expected:

```http
HTTP/1.1 200 OK
```

---

## 19. Test: directory without trailing slash

```bash
curl -i http://localhost:8080/static/listing-test
```

Expected current behavior:

```http
HTTP/1.1 200 OK
```

Body should contain:

```html
<h1>Index of /static/listing-test</h1>
```

Links inside the listing should still be valid:

```html
<a href="/static/listing-test/a.txt">a.txt</a>
<a href="/static/listing-test/b.txt">b.txt</a>
```

Note: a more polished behavior would be:

```http
HTTP/1.1 301 Moved Permanently
Location: /static/listing-test/
```

This is the next planned improvement.

---

## 20. Quick regression checklist

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
curl --path-as-is -i http://localhost:8080/static/../index.html
curl --path-as-is -i http://localhost:8080/static/%2E%2E/index.html
curl -s http://localhost:8080/static/sort-test/
curl -s http://localhost:8080/static/escape-test/
curl -s http://localhost:8080/static/url-test/
curl -i 'http://localhost:8080/static/url-test/query%3Ffile.txt'
curl -i http://localhost:8080/static/mime-test/index.html
curl -i http://localhost:8080/static/mime-test/style.css
curl -i http://localhost:8080/static/mime-test/script.js
curl -i http://localhost:8080/static/mime-test/data.json
curl -i http://localhost:8080/static/mime-test/file.txt
curl -i http://localhost:8080/static/mime-test/test.csv
curl -i http://localhost:8080/static/mime-test/test.xml
curl -i http://localhost:8080/static/mime-test/test.wasm
curl -i http://localhost:8080/static/mime-test/test.mp3
curl -i http://localhost:8080/static/mime-test/test.wav
curl -i http://localhost:8080/static/mime-test/test.mp4
curl -i http://localhost:8080/static/mime-test/test.webm
curl -i http://localhost:8080/static/mime-test/test.woff
curl -i http://localhost:8080/static/mime-test/test.woff2
curl -i http://localhost:8080/static/mime-test/test.ttf
curl -i http://localhost:8080/static/mime-test/unknown.abc
```

Expected summary:

| Request                             | Expected status | Purpose                                                     |
| ----------------------------------- | --------------: | ----------------------------------------------------------- |
| `/`                                 |        `200 OK` | Root index works                                            |
| `/no-such-file`                     | `404 Not Found` | Missing resource                                            |
| `/static/`                          |        `200 OK` | Autoindex enabled                                           |
| `/static/listing-test/`             |        `200 OK` | Nested autoindex                                            |
| `/static/with-index/`               |        `200 OK` | Index file priority                                         |
| `/static/listing-test/a.txt`        |        `200 OK` | Static file serving                                         |
| `/uploads/`                         | `403 Forbidden` | Autoindex disabled                                          |
| `/static/not-found/`                | `404 Not Found` | Missing directory                                           |
| `/static/not-found.txt`             | `404 Not Found` | Missing file                                                |
| `/static/listing-test`              |        `200 OK` | Directory without trailing slash, current accepted behavior |
| `/static/../index.html`             | `403 Forbidden` | Raw path traversal blocked                                  |
| `/static/%2E%2E/index.html`         | `403 Forbidden` | Encoded path traversal blocked                              |
| `/static/sort-test/`                |        `200 OK` | Sorted listing                                              |
| `/static/escape-test/`              |        `200 OK` | HTML escaping                                               |
| `/static/url-test/`                 |        `200 OK` | URL encoded links                                           |
| `/static/url-test/query%3Ffile.txt` |        `200 OK` | URL decoding                                                |
| `/static/mime-test/index.html`      |        `200 OK` | MIME type: `text/html`                                      |
| `/static/mime-test/style.css`       |        `200 OK` | MIME type: `text/css`                                       |
| `/static/mime-test/script.js`       |        `200 OK` | MIME type: `application/javascript`                         |
| `/static/mime-test/data.json`       |        `200 OK` | MIME type: `application/json`                               |
| `/static/mime-test/file.txt`        |        `200 OK` | MIME type: `text/plain`                                     |
| `/static/mime-test/test.csv`        |        `200 OK` | MIME type: `text/csv`                                       |
| `/static/mime-test/test.xml`        |        `200 OK` | MIME type: `application/xml`                                |
| `/static/mime-test/test.wasm`       |        `200 OK` | MIME type: `application/wasm`                               |
| `/static/mime-test/test.mp3`        |        `200 OK` | MIME type: `audio/mpeg`                                     |
| `/static/mime-test/test.wav`        |        `200 OK` | MIME type: `audio/wav`                                      |
| `/static/mime-test/test.mp4`        |        `200 OK` | MIME type: `video/mp4`                                      |
| `/static/mime-test/test.webm`       |        `200 OK` | MIME type: `video/webm`                                     |
| `/static/mime-test/test.woff`       |        `200 OK` | MIME type: `font/woff`                                      |
| `/static/mime-test/test.woff2`      |        `200 OK` | MIME type: `font/woff2`                                     |
| `/static/mime-test/test.ttf`        |        `200 OK` | MIME type: `font/ttf`                                       |
| `/static/mime-test/unknown.abc`     |        `200 OK` | MIME fallback: `application/octet-stream`                   |

---

## 21. Current accepted limitations

These are not blockers for the current autoindex task, but can be improved later:

* Directory without trailing slash returns listing directly instead of redirecting to `/dir/`.
* Basic `../` path traversal protection is implemented for raw and encoded paths. Symlink escape protection with `realpath()` is not covered yet.
* `HEAD` requests are not covered by these tests unless the server implements `HEAD`.
* Autoindex output is intentionally simple HTML and does not show file size or modification date.