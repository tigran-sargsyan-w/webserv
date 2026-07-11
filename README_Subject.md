*This project has been created as part of the 42 curriculum by cafabre, dsemenov, tsargsya.*

# WebServ

## Description

WebServ is a non-blocking HTTP/1.1 server written in C++98 for the 42 curriculum. It parses HTTP requests, routes them through nginx-inspired configuration blocks, and handles static files, uploads, redirects, CGI scripts, and optional server-side sessions.

The server runs a single `poll()` event loop with non-blocking client sockets. Configuration uses custom `server` and `location` blocks to define listening addresses, allowed methods, document roots, upload directories, CGI mappings, custom error pages, and request limits such as `client_max_body_size` and `client_timeout`. Invalid configuration is rejected at startup.

## Instructions

### Prerequisites

- C++98 compiler
- A POSIX-compatible system (Linux/macOS)
- Optional: Python 3 to run the provided regression tests

### Build

```sh
make
```

Rebuild from scratch:

```sh
make re
```

Remove build artifacts:

```sh
make fclean
```

### Run

```sh
./webserv configs/default.conf
```

By default, the server uses `configs/default.conf` when launched as `./webserv` without arguments.

Stop the server with `Ctrl+C`.

### Logging (optional)

Log verbosity can be set at compile time:

```sh
make re LOG_LEVEL=1   # errors only (default)
make re LOG_LEVEL=2   # errors + info
make re LOG_LEVEL=3   # errors + info + debug
```

## Browser demo

```sh
make
./webserv configs/demo.conf
```

The browser demo is served from `www-demo/` and is intended to exercise the same core scenarios as the tested default setup:

- static files with `GET`;
- directory listing with `autoindex on`;
- regular browser uploads with `multipart/form-data`;
- raw `POST /uploads/<filename>` uploads;
- `DELETE` on uploaded resources;
- CGI `GET` and `POST`;
- multiple CGI interpreter types when configured;
- redirects;
- custom error pages;
- cookies and server-side sessions.

Before using `www-demo` as the only web root, verify that every link in the demo maps to an existing route, script, or fixture file and that `configs/demo.conf` starts from a clean clone.

## Tests

### Recommended

Run the automated regression suite from the repository root:

```sh
python3 tests/regression_tester.py check
```

Optional extended stress run (500 parallel requests):

```sh
python3 tests/regression_tester.py check --stress
```

### By topic

Manual test cases are documented in the `docs/` folder:

| Topic | Documentation |
|-------|---------------|
| Autoindex | `docs/autoindex-tests.md` |
| CGI | `docs/cgi-tests.md` |
| Non-blocking CGI | `docs/non-blocking-cgi_tests.md` |
| Multiple CGI interpreters | `docs/multiple-cgi-types-tests.md` |
| POST / DELETE / uploads | `docs/post-delete-tests.md` |
| Request body handling | `docs/request-body-tests.md` |
| Client max body size | `docs/client-max-body-size-tests.md` |
| Request timeout | `docs/request-timeout-tests.md` |
| Partial writes | `docs/partial-write-tests.md` |
| Redirects | `docs/redirect-tests.md` |
| Custom error pages | `docs/error-pages-tests.md` |
| Config validation | `docs/config-validator-tests.md` |
| Sessions (bonus) | `docs/session-bonus-tests.md` |

## Resources

### References
- [NGINX documentation](https://nginx.org/en/docs/)
- [MDN - HTTP Methods](https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods)
- [MDN - HTTP Status Codes](https://developer.mozilla.org/en-US/docs/Web/HTTP/Status)
- [IBM - Common Gateway Interface (CGI)](https://www.ibm.com/docs/en/i/7.5.0?topic=functionality-cgi)

Additional learning resources:

- [Summary of topics covered in WebServ](https://hackmd.io/@fttranscendance/H1mLWxbr_)
- [Creating an HTTP Server from Scratch](https://medium.com/@sakhawy/creating-an-http-server-from-scratch-ed41ef83314b)
- [Polling vs Streaming](https://www.svix.com/resources/faq/polling-vs-streaming/)

### AI usage

**AI was used only as a productivity tool. It helped for :**

- `docs/` : writing manual test documentation from the existing codebase
- `configs/` : generating valid and invalid configuration files from predefined requirements
- `tests/` : generate additional test cases based on existing ones
- outside the project : summarize and regroup documentation about specific topics
- everywhere : detect unused or dead code, review structure and wording

All AI-assisted output was reviewed, adapted, and validated by the team before being merged.
