# Error Pages Testing

This document describes how to manually test custom and fallback error pages in the `webserv` project.

## Goal

These tests verify that the server correctly handles configured `error_page` directives and returns the expected HTTP error responses.

The tests cover:

* custom `404 Not Found` page;
* custom `405 Method Not Allowed` page;
* custom `403 Forbidden` page;
* fallback to default error page when the configured file is missing;
* required response headers;
* preservation of the `Allow` header for `405`.

## Prerequisites

Build the project:

```bash
make re
```

Start the server:

```bash
./webserv configs/default.conf
```

The config should contain custom error pages, for example:

```conf
error_page 403 ./www/errors/403.html;
error_page 404 ./www/errors/404.html;
error_page 405 ./www/errors/405.html;
error_page 500 ./www/errors/500.html;
```

Example custom error page files:

```html
<html><body><h1>Custom 403 Forbidden</h1></body></html>
```

```html
<html><body><h1>Custom 404 Not Found</h1></body></html>
```

```html
<html><body><h1>Custom 405 Method Not Allowed</h1></body></html>
```

## Quick Regression Checklist

Run these commands after starting the server:

```bash
curl -i http://127.0.0.1:8080/not-existing
curl -i -X POST http://127.0.0.1:8080/static/
curl -i -X POST http://127.0.0.1:8080/some-route
curl -i -X DELETE http://127.0.0.1:8080/not-existing-file
```

Expected high-level result:

```txt
404 request  -> custom 404 page
POST static  -> 405 + Allow header + custom 405 page
POST invalid/upload-disabled route -> custom 403 page
DELETE invalid path -> custom 403 page or custom 404 page depending on matched route
```

## Test 1 — Custom 404 Page

Request a missing resource:

```bash
curl -i http://127.0.0.1:8080/not-existing
```

Expected response:

```http
HTTP/1.1 404 Not Found
Connection: close
Content-Length: 56
Content-Type: text/html

<html><body><h1>Custom 404 Not Found</h1></body></html>
```

This confirms that the server uses the configured custom `404` page instead of generating only a default error body.

## Test 2 — Custom 405 Page

Send a method that is not allowed for the target route:

```bash
curl -i -X POST http://127.0.0.1:8080/static/
```

Expected response:

```http
HTTP/1.1 405 Method Not Allowed
Allow: GET
Connection: close
Content-Length: 65
Content-Type: text/html

<html><body><h1>Custom 405 Method Not Allowed</h1></body></html>
```

This confirms that:

* the server returns `405 Method Not Allowed`;
* the custom `405` error page is used;
* the `Allow` header is still present.

The `Allow` header is important because `405` responses should tell the client which methods are allowed for the route.

## Test 3 — Custom 403 Page

Send a request that should be forbidden.

Example:

```bash
curl -i -X POST http://127.0.0.1:8080/some-route
```

Expected response:

```http
HTTP/1.1 403 Forbidden
Connection: close
Content-Length: 56
Content-Type: text/html

<html><body><h1>Custom 403 Forbidden</h1></body></html>
```

This confirms that custom error pages are also used for non-static errors, for example upload-disabled or forbidden route cases.

## Test 4 — DELETE Error Page

Send a DELETE request to an invalid or missing target:

```bash
curl -i -X DELETE http://127.0.0.1:8080/not-existing-file
```

Possible expected response:

```http
HTTP/1.1 403 Forbidden
Connection: close
Content-Length: 56
Content-Type: text/html

<html><body><h1>Custom 403 Forbidden</h1></body></html>
```

Depending on the matched route and path validation, the response may also be:

```http
HTTP/1.1 404 Not Found
Connection: close
Content-Type: text/html
```

The important point is that the error response should use the configured custom page when one exists.

## Test 5 — Fallback When Custom Error Page Is Missing

Temporarily break one configured error page path:

```conf
error_page 404 ./www/errors/missing404.html;
```

Restart the server:

```bash
./webserv configs/default.conf
```

Request a missing resource:

```bash
curl -i http://127.0.0.1:8080/not-existing
```

Expected response:

```http
HTTP/1.1 404 Not Found
Connection: close
Content-Type: text/html
```

Expected default fallback body:

```html
<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>
```

This confirms that the server does not crash when the configured error page file is missing and correctly falls back to a built-in default error page.

After the test, restore the valid path:

```conf
error_page 404 ./www/errors/404.html;
```

## Expected Headers

Every error response should include:

```http
Connection: close
Content-Length: <body size>
Content-Type: text/html
```

For `405 Method Not Allowed`, the response should also include:

```http
Allow: GET
```

The exact `Content-Length` depends on the actual content of the custom HTML file.

## Test Results Example

Example successful output:

```http
HTTP/1.1 404 Not Found
Connection: close
Content-Length: 56
Content-Type: text/html

<html><body><h1>Custom 404 Not Found</h1></body></html>
```

```http
HTTP/1.1 405 Method Not Allowed
Allow: GET
Connection: close
Content-Length: 65
Content-Type: text/html

<html><body><h1>Custom 405 Method Not Allowed</h1></body></html>
```

```http
HTTP/1.1 403 Forbidden
Connection: close
Content-Length: 56
Content-Type: text/html

<html><body><h1>Custom 403 Forbidden</h1></body></html>
```

## Quick Pass Criteria

The error pages feature can be considered working if:

* missing static files return the configured custom `404` page;
* method errors return the configured custom `405` page;
* `405` responses keep the `Allow` header;
* forbidden requests return the configured custom `403` page;
* missing custom error page files fall back to the default built-in HTML page;
* the server does not crash during any of these tests.
