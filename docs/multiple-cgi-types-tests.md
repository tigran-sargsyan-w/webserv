# Multiple CGI Types Tests

This document contains manual tests for the `Bonus multiple cgi types` pull request.

The goal is to verify that the same CGI location can execute more than one configured CGI type, for example:

```nginx
location /cgi-bin {
    methods GET POST;
    root ./www/cgi-bin;
    autoindex off;
    cgi .py /usr/bin/python3;
    cgi .sh /bin/sh;
    cgi .php /usr/bin/php-cgi;
}
```

The important point is that CGI execution is selected from the request script extension and the configured interpreter, not hardcoded to Python only.

---

## 1. Test setup

Start the server from the repository root:

```bash
make
./webserv configs/default.conf
```

The tests assume that the server listens on:

```txt
http://127.0.0.1:8080
```

The shell CGI script used by these tests is:

```txt
www/cgi-bin/hello.sh
```

It should print the request method, query string, content headers, and the request body received on stdin.

---

## 2. Shell CGI GET test

Run:

```bash
curl -i "http://127.0.0.1:8080/cgi-bin/hello.sh?mode=multi"
```

Expected response:

```http
HTTP/1.1 200 OK
Content-Type: text/plain
```

Expected body fragments:

```txt
HELLO_FROM_SHELL_CGI
REQUEST_METHOD=GET
QUERY_STRING=mode=multi
CONTENT_TYPE=
CONTENT_LENGTH=
BODY:
```

This validates that:

* `.sh` is recognized as a CGI extension;
* `/bin/sh` is used as the CGI interpreter;
* the query string is passed to the shell CGI environment;
* an empty GET body does not block the CGI process.

---

## 3. Shell CGI POST test

Run:

```bash
curl -i \
  -X POST \
  -H "Content-Type: application/x-www-form-urlencoded" \
  --data "name=tigran&mode=shell-post" \
  "http://127.0.0.1:8080/cgi-bin/hello.sh?mode=post"
```

Expected response:

```http
HTTP/1.1 200 OK
Content-Type: text/plain
```

Expected body fragments:

```txt
HELLO_FROM_SHELL_CGI
REQUEST_METHOD=POST
QUERY_STRING=mode=post
CONTENT_TYPE=application/x-www-form-urlencoded
CONTENT_LENGTH=27
BODY:
name=tigran&mode=shell-post
```

This validates that:

* `.sh` CGI works with `POST`, not only with `GET`;
* the request body is forwarded to the CGI process through stdin;
* `CONTENT_TYPE` is passed to the CGI environment;
* `CONTENT_LENGTH` matches the POST body size;
* query string and POST body can be used together.

---

## 4. Python CGI regression check

Run:

```bash
curl -i "http://127.0.0.1:8080/cgi-bin/hello.py?mode=multi"
```

Expected response:

```http
HTTP/1.1 200 OK
```

Expected body contains the Python CGI output, for example:

```html
<h1>Hello from CGI</h1>
```

This validates that adding `.sh` support did not break the existing `.py` CGI type.

---

## 5. Automated helper script

The same POST behavior can also be checked with:

```bash
sh tests/shell-cgi-post-test.sh
```

Expected output:

```txt
PASS shell CGI POST test
```

This script is only a convenience wrapper around the manual POST `curl` test above.

---

## 6. Full regression

Before merging the PR, run:

```bash
python3 tests/regression_tester.py check --build --check-relink
```

Expected result:

```txt
all tests pass
```

Together, the manual GET/POST checks and the regression tester prove that multiple CGI types are supported and that the existing CGI behavior remains stable.
