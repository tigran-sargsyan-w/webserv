# Webserv – A Non-Blocking HTTP Server in C++98 🌐⚙️

✅ **Status**: Completed  
🏫 **School**: 42 Lyon – Webserv  
🏅 **Score**: 125/100  
🧑‍💻 **Language**: C++98

> *A configurable HTTP server built from scratch around a single non-blocking `poll()` event loop.*

---

## 📚 Table of Contents

- [📝 Description](#-description)
- [✨ Highlights](#-highlights)
- [🌍 Supported HTTP Features](#-supported-http-features)
- [🏗️ Architecture](#️-architecture)
  - [Main event loop](#main-event-loop)
  - [Request lifecycle](#request-lifecycle)
  - [CGI lifecycle](#cgi-lifecycle)
- [🧩 Project Modules](#-project-modules)
- [⚙️ Configuration](#️-configuration)
  - [Server directives](#server-directives)
  - [Location directives](#location-directives)
  - [Configuration example](#configuration-example)
- [🚀 Instructions](#-instructions)
  - [Prerequisites](#prerequisites)
  - [Build](#build)
  - [Run](#run)
  - [Logging](#logging)
- [🖥️ Browser Demo](#️-browser-demo)
- [🧪 Testing](#-testing)
  - [Regression suite](#regression-suite)
  - [Manual examples](#manual-examples)
  - [What is covered](#what-is-covered)
- [📂 Repository Layout](#-repository-layout)
- [🛡️ Robustness and Security](#️-robustness-and-security)
- [🧠 Key Technical Lessons](#-key-technical-lessons)
- [📖 Resources](#-resources)

---

## 📝 Description

**Webserv** is a non-blocking HTTP server written entirely in **C++98**.

The project recreates the essential behaviour of a production web server without relying on an existing HTTP server implementation. It accepts TCP connections, parses HTTP requests, resolves configuration-driven routes, executes the appropriate handler, and sends complete HTTP responses back to clients.

The server is compatible with standard browsers and command-line clients such as `curl`. It can serve static websites, upload and delete files, generate directory listings, execute CGI scripts, redirect requests, return custom error pages, and maintain simple server-side sessions.

The implementation is driven by a single `poll()`-based event loop that monitors:

- listening sockets;
- connected clients;
- client reads and partial writes;
- CGI standard-input pipes;
- CGI standard-output pipes.

This architecture allows slow clients and slow CGI processes to coexist without blocking the rest of the server.

The original shorter submission-oriented README is preserved as [`README_Subject.md`](README_Subject.md).

---

## ✨ Highlights

- ✅ Written in **C++98** with `-Wall -Wextra -Werror`
- ✅ One central `poll()` event loop for network and CGI pipe I/O
- ✅ Non-blocking listening and client sockets
- ✅ Multiple `server` blocks and listening ports
- ✅ Incremental request parsing across multiple TCP packets
- ✅ `GET`, `POST`, and `DELETE`
- ✅ HTTP/1.0 and HTTP/1.1 request handling
- ✅ `Content-Length` and `Transfer-Encoding: chunked`
- ✅ Static files and MIME type detection
- ✅ Directory index resolution and optional autoindex
- ✅ Raw and `multipart/form-data` uploads
- ✅ CGI with configurable interpreters
- ✅ CGI query strings, request body, headers, and `PATH_INFO`
- ✅ Non-blocking CGI input/output and execution timeout
- ✅ HTTP redirects
- ✅ Custom and fallback error pages
- ✅ Request body size limits
- ✅ Client inactivity timeouts
- ✅ Cookies and server-side sessions
- ✅ Automated regression and stress testing

---

## 🌍 Supported HTTP Features

| Area | Supported behaviour |
|---|---|
| Methods | `GET`, `POST`, `DELETE` |
| Protocol versions | HTTP/1.0 and HTTP/1.1 requests |
| Static content | HTML, CSS, JavaScript, images, text, binary files, and other configured resources |
| Routing | Longest matching `location` prefix |
| Directories | Configurable index file and optional autoindex listing |
| Uploads | Raw request-body uploads and `multipart/form-data` |
| Deletion | Removal of uploaded resources through `DELETE` |
| Request bodies | `Content-Length` and chunked transfer decoding |
| CGI | Configurable extensions and interpreters, query strings, headers, stdin body, `PATH_INFO`, custom status output |
| Redirects | Configuration-driven HTTP redirects |
| Errors | Custom pages with built-in fallback responses |
| Limits | Maximum request-body size, header/URI validation, client timeout, CGI timeout |
| Sessions | Cookie parsing, server-side session creation, visit counting, and logout |
| Concurrency | Multiple simultaneous clients handled through `poll()` |

---

## 🏗️ Architecture

### Main event loop

The central `WebServ` object owns the main runtime components:

```text
WebServ
├── PollManager
├── ListenerSocketHandler
├── ConnectionManager
└── CgiManager
```

At runtime, the server repeatedly:

1. calculates the nearest client or CGI timeout;
2. calls `poll()` once for all registered descriptors;
3. checks expired CGI processes and inactive clients;
4. dispatches ready descriptors to the correct event handler;
5. updates the requested `POLLIN` / `POLLOUT` interests;
6. removes closed clients and completed CGI descriptors safely.

```text
                    ┌──────────────────────┐
                    │      poll() loop     │
                    └──────────┬───────────┘
                               │
             ┌─────────────────┼─────────────────┐
             │                 │                 │
       listener socket     client socket      CGI pipe
          POLLIN          POLLIN / POLLOUT   POLLIN / POLLOUT
             │                 │                 │
          accept()       recv() / send()     read() / write()
```

### Request lifecycle

```text
TCP connection
      │
      ▼
Client input buffer
      │
      ▼
RequestInspector
- detects complete headers
- validates framing
- determines body mode
      │
      ▼
RequestParser
- request line
- headers
- body
- chunked decoding
      │
      ▼
RequestDispatcher
      │
      ▼
Router
- removes query string
- selects longest matching location prefix
      │
      ├── RedirectHandler
      ├── SessionHandler
      ├── CgiRequestHandler
      ├── UploadHandler
      ├── DeleteHandler
      └── StaticFileHandler
      │
      ▼
Response serialization
      │
      ▼
Client output buffer
      │
      ▼
Partial non-blocking send()
```

The response is stored in the client output buffer and may require several `POLLOUT` events before it is completely transmitted.

### CGI lifecycle

CGI execution is isolated from the main server process through `fork()`, pipes, and `execve()`.

```text
HTTP request
     │
     ▼
Validate script and interpreter
     │
     ▼
Create stdin/stdout pipes
     │
     ▼
fork()
 ┌───┴───────────────────────────┐
 │ child                         │ parent
 │                               │
 │ dup2() pipes                  │ register pipe FDs in poll()
 │ build CGI environment         │ write request body incrementally
 │ chdir() to script directory   │ read CGI output incrementally
 │ execve() interpreter          │ monitor child and timeout
 └───────────────────────────────┘
                                     │
                                     ▼
                            Parse CGI headers/body
                                     │
                                     ▼
                              Build HTTP response
```

The CGI environment includes standard metadata such as:

- `REQUEST_METHOD`
- `QUERY_STRING`
- `CONTENT_LENGTH`
- `CONTENT_TYPE`
- `SCRIPT_NAME`
- `PATH_INFO`
- `PATH_TRANSLATED`
- `SERVER_NAME`
- `SERVER_PORT`
- `SERVER_PROTOCOL`
- `REMOTE_ADDR`
- converted `HTTP_*` request headers

If a CGI does not return a `Content-Length`, EOF marks the end of its output. Long-running CGI processes are terminated when their timeout is reached.

---

## 🧩 Project Modules

| Module | Responsibility |
|---|---|
| `core` | Application lifecycle, logging, and the main event loop |
| `config` | Lexing, parsing, validating, and storing configuration |
| `http` | Request inspection, parsing, chunk decoding, methods, responses, and MIME types |
| `network` | Listeners, clients, connections, poll dispatch, timeouts, and partial writes |
| `routing` | Route selection and request dispatch |
| `handlers` | Static resources, redirects, deletion, errors, templates, and autoindex responses |
| `storage` | Path resolution, multipart parsing, uploads, and file storage |
| `session` | Cookie parsing and server-side session management |
| `cgi` | CGI validation, startup, pipe I/O, completion, and timeout handling |
| `utils` | URI, filesystem path, and general helper functions |

---

## ⚙️ Configuration

The configuration format is inspired by NGINX and supports multiple `server` blocks containing multiple `location` blocks.

Invalid syntax, unknown directives, inconsistent values, inaccessible required paths, and conflicting configuration are rejected during startup.

### Server directives

| Directive | Purpose | Example |
|---|---|---|
| `listen` | Interface and port to bind | `listen 127.0.0.1:8080;` |
| `server_name` | Logical name used by the server and CGI metadata | `server_name webserv_demo;` |
| `root` | Default document root | `root ./www;` |
| `index` | Default file for directory requests | `index index.html;` |
| `client_max_body_size` | Maximum accepted request body in bytes | `client_max_body_size 1048576;` |
| `client_timeout` | Client inactivity timeout in seconds | `client_timeout 30;` |
| `error_page` | Custom page for a status code | `error_page 404 ./www/errors/404.html;` |

### Location directives

| Directive | Purpose | Example |
|---|---|---|
| `methods` | Accepted HTTP methods | `methods GET POST DELETE;` |
| `root` | Filesystem root for this route | `root ./www/uploads;` |
| `index` | Route-specific directory index | `index index.html;` |
| `autoindex` | Enable or disable directory listing | `autoindex on;` |
| `upload_enable` | Allow uploads on the route | `upload_enable on;` |
| `upload_store` | Directory where uploaded files are stored | `upload_store ./www/uploads;` |
| `return` | Return a redirect response | `return 301 /index.html;` |
| `cgi` | Map an extension to an interpreter | `cgi .py /usr/bin/python3;` |
| `session_enable` | Enable the built-in session endpoint | `session_enable on;` |
| `session_path` | Path used by the session handler | `session_path /session;` |

### Configuration example

```nginx
server {
    listen 127.0.0.1:8080;
    server_name webserv_demo;

    root ./www-demo;
    index index.html;
    client_max_body_size 10485760;
    client_timeout 30;

    error_page 404 ./www-demo/errors/404.html;
    error_page 413 ./www-demo/errors/413.html;
    error_page 500 ./www-demo/errors/500.html;
    error_page 504 ./www-demo/errors/504.html;

    location / {
        methods GET;
        root ./www-demo;
        index index.html;
        autoindex off;
        upload_enable off;
        session_enable on;
        session_path /session;
    }

    location /uploads {
        methods GET POST DELETE;
        root ./www-demo/uploads;
        autoindex on;
        upload_enable on;
        upload_store ./www-demo/uploads;
    }

    location /cgi-bin {
        methods GET POST;
        root ./www-demo/cgi-bin;
        autoindex off;
        upload_enable off;
        cgi .py /usr/bin/python3;
        cgi .sh /bin/sh;
    }

    location /redirect-me {
        methods GET;
        return 301 /index.html;
    }
}
```

Route matching uses the **longest valid location prefix**. For example, `/uploads/file.txt` matches `/uploads` instead of `/`.

---

## 🚀 Instructions

### Prerequisites

Required:

- a POSIX-compatible system such as Linux or macOS;
- a C++ compiler with C++98 support;
- `make`.

Used by the supplied demo and tests:

- Python 3;
- `/bin/sh`.

Optional:

- `php-cgi`, when using the PHP mapping included in `configs/default.conf`;
- `curl`, `telnet`, or `netcat` for manual testing.

### Build

```sh
make
```

The build uses:

```text
-Wall -Wextra -Werror -std=c++98
```

Rebuild everything:

```sh
make re
```

Remove object files:

```sh
make clean
```

Remove object files and the executable:

```sh
make fclean
```

### Run

With the default configuration:

```sh
./webserv
```

This loads:

```text
configs/default.conf
```

With a custom configuration:

```sh
./webserv path/to/config.conf
```

Example:

```sh
./webserv configs/demo.conf
```

Then open:

```text
http://127.0.0.1:8080
```

Stop the server with `Ctrl+C`.

### Logging

Logging verbosity is selected at compile time:

```sh
make re LOG_LEVEL=1   # errors only, default
make re LOG_LEVEL=2   # errors and informational messages
make re LOG_LEVEL=3   # errors, information, and debug traces
```

---

## 🖥️ Browser Demo

A complete demonstration website is available in `www-demo/`.

Start it with:

```sh
make
./webserv configs/demo.conf
```

The demo exposes examples for:

- static pages;
- directory listings;
- raw file uploads;
- browser `multipart/form-data` uploads;
- uploaded-file deletion;
- Python and shell CGI;
- redirects;
- custom error pages;
- cookies and server-side sessions.

Useful routes:

| Route | Demonstrates |
|---|---|
| `/` | Main static demo website |
| `/files/` | Autoindex directory listing |
| `/public/` | Public static directory |
| `/uploads/` | Upload listing and file operations |
| `/cgi-bin/` | CGI scripts |
| `/redirect-me` | `301` redirect |
| `/session` | Session creation and visit counter |
| `/session/logout` | Session destruction and cookie expiration |

---

## 🧪 Testing

### Regression suite

The repository includes a Python regression tester using only the Python standard library.

It creates an isolated runtime directory, generates its own websites and configuration, starts the server, runs deterministic tests, and shuts the server down automatically.

Build and run the complete check:

```sh
python3 tests/regression_tester.py check --build
```

Also verify that the Makefile does not relink unnecessarily:

```sh
python3 tests/regression_tester.py check --build --check-relink
```

Run the extended parallel stress scenario:

```sh
python3 tests/regression_tester.py check --build --stress
```

The extended mode sends **500 parallel requests** and performs a final health check afterward.

The tester can also record and compare behaviour snapshots:

```sh
python3 tests/regression_tester.py record \
    --build \
    --baseline tests/baselines/before_refactor.json

python3 tests/regression_tester.py compare \
    --build \
    --baseline tests/baselines/before_refactor.json
```

### Manual examples

#### Static file

```sh
curl -i http://127.0.0.1:8080/
```

#### Directory listing

```sh
curl -i http://127.0.0.1:8080/files/
```

#### Raw upload

```sh
curl -i \
    -X POST \
    -H "Content-Type: application/octet-stream" \
    --data-binary @README.md \
    http://127.0.0.1:8080/uploads/readme-copy.md
```

#### Multipart upload

```sh
curl -i \
    -F "file=@README.md" \
    http://127.0.0.1:8080/uploads/
```

#### Retrieve an uploaded file

```sh
curl -i http://127.0.0.1:8080/uploads/readme-copy.md
```

#### Delete an uploaded file

```sh
curl -i \
    -X DELETE \
    http://127.0.0.1:8080/uploads/readme-copy.md
```

#### CGI with query parameters

```sh
curl -i "http://127.0.0.1:8080/cgi-bin/hello.py?name=42"
```

#### CGI POST body

```sh
curl -i \
    -X POST \
    -H "Content-Type: text/plain" \
    --data "Hello from curl" \
    http://127.0.0.1:8080/cgi-bin/post.py
```

#### Redirect

```sh
curl -i http://127.0.0.1:8080/redirect-me
```

### What is covered

The automated and documented tests cover:

- multiple listening ports;
- static files and large binary responses;
- index resolution and autoindex;
- custom error pages;
- allowed and unsupported methods;
- malformed request lines and headers;
- duplicate or invalid `Content-Length`;
- split requests arriving in several packets;
- body-size enforcement;
- valid and invalid chunked bodies;
- chunk extensions and trailers;
- Python and shell CGI;
- CGI environment variables;
- CGI stdin and `PATH_INFO`;
- CGI custom status responses;
- CGI without explicit headers;
- CGI timeout handling;
- raw upload, retrieval, and deletion;
- encoded path traversal attempts;
- partial writes to slow readers;
- clients disconnecting during a response;
- slow incomplete clients;
- slow CGI processes that must not block normal requests;
- parallel request stress;
- final server health after all tests.

Topic-specific manual test documentation is available in `docs/`.

---

## 📂 Repository Layout

```text
.
├── Makefile
├── README.md
├── README_Subject.md
├── configs/
│   ├── default.conf
│   └── demo.conf
├── docs/
│   ├── autoindex-tests.md
│   ├── cgi-tests.md
│   ├── config-validator-tests.md
│   ├── error-pages-tests.md
│   ├── multiple-cgi-types-tests.md
│   ├── non-blocking-cgi_tests.md
│   ├── partial-write-tests.md
│   ├── post-delete-tests.md
│   ├── redirect-tests.md
│   ├── request-body-tests.md
│   ├── request-timeout-tests.md
│   └── session-bonus-tests.md
├── include/
│   ├── core/
│   ├── config/
│   ├── http/
│   ├── network/
│   ├── routing/
│   ├── handlers/
│   ├── storage/
│   ├── session/
│   ├── cgi/
│   └── utils/
├── src/
│   ├── main.cpp
│   ├── core/
│   ├── config/
│   ├── http/
│   ├── network/
│   ├── routing/
│   ├── handlers/
│   ├── storage/
│   ├── session/
│   ├── cgi/
│   └── utils/
├── tests/
│   ├── regression_tester.py
│   └── baselines/
├── www/
└── www-demo/
```

---

## 🛡️ Robustness and Security

The implementation includes explicit handling for common failure and abuse cases:

- malformed HTTP request lines;
- invalid or conflicting body framing;
- unsupported methods;
- missing resources;
- forbidden directory access;
- network and broadcast-style partial input scenarios;
- request bodies larger than the configured maximum;
- URI and header limits;
- decoded path traversal attempts such as `%2e%2e`;
- clients closing a connection during transmission;
- partial socket writes;
- inactive clients;
- missing or unsupported CGI scripts;
- malformed CGI responses;
- CGI processes that exceed their timeout;
- process cleanup and pipe cleanup after CGI completion.

`SIGPIPE` is ignored so that writing to a disconnected client does not terminate the server process.

---

## 🧠 Key Technical Lessons

This project brings together several low-level systems concepts:

- how TCP is a byte stream rather than a message protocol;
- why one HTTP request may arrive in many `recv()` calls;
- how to detect complete headers and bodies without blocking;
- how `poll()` readiness drives a state machine;
- why a complete response may require many `send()` calls;
- how HTTP body framing works with `Content-Length` and chunked encoding;
- how filesystem routing differs from URL routing;
- how CGI connects HTTP, environment variables, pipes, and processes;
- how to keep child-process I/O non-blocking;
- how configuration parsing benefits from separate lexer, parser, and validator stages;
- how regression tests protect behaviour during architectural refactoring.

---

## 📖 Resources

### HTTP and web servers

- [RFC 9110 – HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110)
- [RFC 9112 – HTTP/1.1](https://www.rfc-editor.org/rfc/rfc9112)
- [MDN – HTTP overview](https://developer.mozilla.org/en-US/docs/Web/HTTP/Overview)
- [MDN – HTTP methods](https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods)
- [MDN – HTTP response status codes](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status)
- [NGINX documentation](https://nginx.org/en/docs/)

### Networking and system calls

- [`poll(2)` – Linux manual page](https://man7.org/linux/man-pages/man2/poll.2.html)
- [`socket(2)` – Linux manual page](https://man7.org/linux/man-pages/man2/socket.2.html)
- [`accept(2)` – Linux manual page](https://man7.org/linux/man-pages/man2/accept.2.html)
- [`recv(2)` – Linux manual page](https://man7.org/linux/man-pages/man2/recv.2.html)
- [`send(2)` – Linux manual page](https://man7.org/linux/man-pages/man2/send.2.html)

### CGI

- [CGI 1.1 specification – RFC 3875](https://www.rfc-editor.org/rfc/rfc3875)
- [Common Gateway Interface overview](https://en.wikipedia.org/wiki/Common_Gateway_Interface)

### Additional project references

- [Summary of topics covered in Webserv](https://hackmd.io/@fttranscendance/H1mLWxbr_)
- [Creating an HTTP Server from Scratch](https://medium.com/@sakhawy/creating-an-http-server-from-scratch-ed41ef83314b)

---

Built as part of the **42 curriculum**.
