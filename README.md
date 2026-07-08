*This project has been created as part of the 42 curriculum by tsargsya, dsemenov, cafabre*

# WebServ

## Description

This project aims to create your own HTTP server. You will be able to test it with a real web browser. HTTP is one of the most used protocols on the internet. Knowing its intricacies will be useful, even if web development is not on your career path.

## Instructions

### Prerequisites

- C++98 compiler
- A POSIX-compatible system (Linux/macOS)
- Optional: Python 3 to run the provided regression tests

### Build and run

```sh
make
./webserv
```

By default the server uses `configs/default.conf`.

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

## Resources

- https://hackmd.io/@fttranscendance/H1mLWxbr_