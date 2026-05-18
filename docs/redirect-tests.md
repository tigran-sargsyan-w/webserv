# Redirect Test Cases

This document contains manual test cases for the redirect support implemented in the `webserv` project.

The goal is to verify that the `return <redirect_status_code> <target>;` directive works correctly for location blocks.

---

## 1. Supported redirect behavior

The current implementation supports config-based internal redirects using this format:

```conf
return <redirect_status_code> <target>;
```

Supported redirect status codes:

```txt
301, 302, 303, 307, 308
```

Supported redirect target format:

```txt
/path
```

The target must:

* not be empty;
* start with `/`;
* not start with `//`.

Examples of valid redirect directives:

```conf
return 301 /;
return 302 /login;
return 303 /result;
return 307 /temporary;
return 308 /new-location;
```

Examples of invalid redirect directives:

```conf
return 404 /;
return 200 /;
return 500 /;
return 301 //;
return 301 //example.com;
return 301 example.com;
return 301 https://example.com;
```

---

## 2. Preparation

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

## 3. Test: default redirect route

The default config contains a route similar to:

```conf
location /redirect-me {
    methods GET;
    return 301 /;
}
```

### Command

```bash
curl -i http://localhost:8080/redirect-me
```

### Expected result

```http
HTTP/1.1 301 Moved Permanently
Location: /
Content-Type: text/html
Content-Length: ...
Connection: close
```

The response body may contain a simple HTML message, for example:

```html
<html><body><h1>301 Redirect</h1><p>Redirecting to /</p></body></html>
```

### Purpose

Checks that a location with `return 301 /;` returns an HTTP redirect response and does not try to serve a file, run CGI, or generate autoindex.

---

## 4. Test: follow redirect with curl

### Command

```bash
curl -i -L http://localhost:8080/redirect-me
```

Or, for more verbose output:

```bash
curl -v -L http://localhost:8080/redirect-me
```

### Expected result

Curl should first receive:

```http
HTTP/1.1 301 Moved Permanently
Location: /
```

Then it should automatically make a second request to:

```txt
/
```

The final response should be the root page:

```http
HTTP/1.1 200 OK
```

### Purpose

Checks that the redirect response is valid enough for a real HTTP client to follow it.

---

## 5. Test: missing resource must not redirect

### Command

```bash
curl -i http://localhost:8080/something-that-does-not-exist
```

### Expected result

```http
HTTP/1.1 404 Not Found
```

### Purpose

Checks the important distinction between:

```txt
missing resource -> 404 Not Found
configured redirect route -> 3xx redirect
```

A missing path must not automatically redirect.

---

## 6. Test: supported redirect status codes

Create a temporary test config, for example:

```bash
cp configs/default.conf configs/redirect_test.conf
```

Add these routes inside the server block:

```conf
location /r301 {
    methods GET;
    return 301 /;
}

location /r302 {
    methods GET;
    return 302 /;
}

location /r303 {
    methods GET;
    return 303 /;
}

location /r307 {
    methods GET;
    return 307 /;
}

location /r308 {
    methods GET;
    return 308 /;
}
```

Start the server with:

```bash
./webserv configs/redirect_test.conf
```

### Commands

```bash
curl -i http://localhost:8080/r301
curl -i http://localhost:8080/r302
curl -i http://localhost:8080/r303
curl -i http://localhost:8080/r307
curl -i http://localhost:8080/r308
```

### Expected status lines

```http
HTTP/1.1 301 Moved Permanently
HTTP/1.1 302 Found
HTTP/1.1 303 See Other
HTTP/1.1 307 Temporary Redirect
HTTP/1.1 308 Permanent Redirect
```

Each response should contain:

```http
Location: /
```

### Purpose

Checks that all supported redirect codes are accepted by the config validator and serialized with the correct HTTP reason phrase.

---

## 7. Test: invalid redirect status code

Create a temporary invalid config, for example:

```conf
location /bad-code {
    methods GET;
    return 404 /;
}
```

Other invalid examples:

```conf
return 200 /;
return 300 /;
return 309 /;
return 500 /;
```

### Command

```bash
./webserv configs/bad_redirect_code.conf
```

### Expected result

The server should not start.

Expected error should mention that the redirect status code is invalid, for example:

```txt
Config validation error: location /bad-code has invalid redirect status code
```

### Purpose

Checks that `return` is restricted to real redirect status codes:

```txt
301, 302, 303, 307, 308
```

---

## 8. Test: invalid redirect target

Create a temporary invalid config with one of these invalid targets:

```conf
location /bad-target-one {
    methods GET;
    return 301 //;
}

location /bad-target-two {
    methods GET;
    return 301 //example.com;
}

location /bad-target-three {
    methods GET;
    return 301 example.com;
}

location /bad-target-four {
    methods GET;
    return 301 https://example.com;
}
```

### Command

```bash
./webserv configs/bad_redirect_target.conf
```

### Expected result

The server should not start.

Expected error should mention that the redirect target is invalid, for example:

```txt
Config validation error: location /bad-target-one has invalid redirect target
```

### Purpose

Checks that redirect targets are limited to internal paths and that suspicious targets like `//example.com` are rejected.

---

## 9. Test: valid redirect targets

Create a temporary config with routes such as:

```conf
location /to-root {
    methods GET;
    return 301 /;
}

location /to-login {
    methods GET;
    return 302 /login;
}

location /to-static {
    methods GET;
    return 308 /static/;
}
```

### Commands

```bash
curl -i http://localhost:8080/to-root
curl -i http://localhost:8080/to-login
curl -i http://localhost:8080/to-static
```

### Expected results

```http
HTTP/1.1 301 Moved Permanently
Location: /
```

```http
HTTP/1.1 302 Found
Location: /login
```

```http
HTTP/1.1 308 Permanent Redirect
Location: /static/
```

### Purpose

Checks that valid internal redirect targets are accepted and returned in the `Location` header.

---

## 10. Test: redirect should happen before static file handling

For the default route:

```conf
location /redirect-me {
    methods GET;
    return 301 /;
}
```

Make sure there is no file required at:

```txt
www/redirect-me
```

### Command

```bash
curl -i http://localhost:8080/redirect-me
```

### Expected result

```http
HTTP/1.1 301 Moved Permanently
Location: /
```

The response must not be:

```http
HTTP/1.1 404 Not Found
```

### Purpose

Checks that redirect is handled as a route rule, not as a file lookup.

---

## 11. Current method-related limitation

Current accepted behavior:

```bash
curl -i -X POST http://localhost:8080/redirect-me
```

May still return a redirect response even if the route has:

```conf
methods GET;
```

This is accepted for the redirect task because full route method enforcement is a separate task.

Final desired behavior after method enforcement:

```txt
1. Check whether the method is allowed by route.methods.
2. If not allowed, return 405 Method Not Allowed.
3. If allowed and route.hasReturn is true, return redirect.
```

---

## 12. Quick regression checklist

Before opening or merging the PR, run:

```bash
curl -i http://localhost:8080/redirect-me
curl -i -L http://localhost:8080/redirect-me
curl -i http://localhost:8080/something-that-does-not-exist
```

Expected summary:

| Request                          |         Expected status | Purpose                            |
| -------------------------------- | ----------------------: | ---------------------------------- |
| `/redirect-me`                   | `301 Moved Permanently` | Configured redirect                |
| `/redirect-me` with `-L`         |          final `200 OK` | Client can follow redirect         |
| `/something-that-does-not-exist` |         `404 Not Found` | Missing resource must not redirect |

For full redirect-code coverage, also test:

```bash
curl -i http://localhost:8080/r301
curl -i http://localhost:8080/r302
curl -i http://localhost:8080/r303
curl -i http://localhost:8080/r307
curl -i http://localhost:8080/r308
```

Expected summary:

| Route   |          Expected status | Expected Location |
| ------- | -----------------------: | ----------------- |
| `/r301` |  `301 Moved Permanently` | `/`               |
| `/r302` |              `302 Found` | `/`               |
| `/r303` |          `303 See Other` | `/`               |
| `/r307` | `307 Temporary Redirect` | `/`               |
| `/r308` | `308 Permanent Redirect` | `/`               |

---

## 13. Current accepted limitations

These are not blockers for the current redirect task, but can be improved later:

* `return` is implemented only as redirect, not as a generic Nginx-like `return` directive.
* External redirects such as `https://example.com` are not supported.
* Protocol-relative redirects such as `//example.com` are rejected.
* Route method enforcement is handled separately.
* Redirect target normalization is minimal and only supports internal paths starting with one `/`.
