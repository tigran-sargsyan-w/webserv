# webserv

A non-blocking HTTP/1.1 server written in C++98, inspired by the 42 School project. Supports static files, CGI scripts, file uploads, multiple virtual servers, and more — all using a single `poll()` event loop.

## Features

- **Non-blocking I/O** — single `poll()` loop handles all sockets (listen + clients + CGI pipes)
- **HTTP/1.1** — GET, POST, DELETE; keep-alive; chunked transfer decoding
- **Static file serving** — MIME types, autoindex directory listing, custom index files
- **File uploads** — POST to a configured upload directory
- **CGI** — fork/execve for `.py`, `.pl`, `.sh` scripts with full CGI/1.1 environment variables
- **NGINX-like config** — `server {}` and `location {}` blocks, redirects, per-route limits
- **Multiple virtual servers** — different ports/interfaces, independent content roots
- **Error pages** — built-in defaults + custom pages from config
- **Connection timeouts** — no hanging connections; CGI processes killed after 30 s

## Build

```bash
make          # build webserv binary
make clean    # remove object files
make fclean   # remove objects + binary
make re       # fclean + all
```

Requirements: `g++` with C++98 support (`-Wall -Wextra -Werror`). No external libraries.

## Run

```bash
./webserv [config_file]
```

If no config file is given, `./default.conf` is used.

```bash
./webserv config/default.conf
```

## Config Example

```nginx
server {
    listen 8080;
    server_name localhost;
    client_max_body_size 10M;
    error_page 404 /errors/404.html;

    location / {
        root ./www;
        index index.html;
        autoindex off;
        allowed_methods GET POST;
    }

    location /upload {
        allowed_methods POST;
        upload_dir ./uploads;
        client_max_body_size 100M;
    }

    location /cgi-bin {
        root ./cgi-bin;
        cgi_extension .py;
        allowed_methods GET POST;
    }

    location /files {
        root ./uploads;
        allowed_methods GET DELETE;
        autoindex on;
    }
}
```

### Config Directives

| Directive | Scope | Description |
|-----------|-------|-------------|
| `listen` | server | `port` or `host:port` |
| `server_name` | server | Virtual host name |
| `client_max_body_size` | server/location | Max body (e.g. `10M`, `1G`, `512k`) |
| `error_page` | server | `code /path` |
| `location` | server | Route prefix block |
| `root` | location | Filesystem root for this route |
| `index` | location | Default file (default: `index.html`) |
| `autoindex` | location | `on`/`off` directory listing |
| `allowed_methods` | location | Space-separated: `GET POST DELETE` |
| `return` | location | Redirect: `301 /new/url` |
| `upload_dir` | location | Directory to save uploaded files |
| `cgi_extension` | location | File extension to execute as CGI (e.g. `.py`) |

## Testing

```bash
# Run the built-in test suite
bash tests/test.sh

# Manual telnet test
printf "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 8080

# Upload a file
curl -X POST -F "file=@/path/to/file.txt" http://localhost:8080/upload

# CGI
curl http://localhost:8080/cgi-bin/hello.py

# Delete a file
curl -X DELETE http://localhost:8080/files/myfile.txt
```

## Directory Structure

```
webserv/
├── include/          # Header files
├── src/              # Source files
├── config/           # Sample configuration files
├── www/              # Sample web content
│   └── errors/       # Default error pages
├── cgi-bin/          # CGI scripts
├── uploads/          # Default upload directory
└── tests/            # Test scripts
```

## Architecture

| Component | Description |
|-----------|-------------|
| `EventLoop` | Single `poll()` managing listen sockets, clients, and CGI pipes |
| `ConfigParser` | NGINX-like tokenizer + recursive-descent parser |
| `HttpRequest` | Incremental state-machine parser (request line → headers → body) |
| `Router` | Best-prefix location matching; static files, autoindex, uploads, CGI |
| `CgiHandler` | fork/execve with non-blocking pipe I/O and 30 s timeout |
| `Connection` | Per-connection state: `READING` → `WRITING` → `CLOSED` |

## Resources

- [RFC 7230 – HTTP/1.1 Message Syntax](https://datatracker.ietf.org/doc/html/rfc7230)
- [RFC 3875 – CGI/1.1](https://datatracker.ietf.org/doc/html/rfc3875)
- [NGINX configuration reference](https://nginx.org/en/docs/)
- [`poll(2)` man page](https://man7.org/linux/man-pages/man2/poll.2.html)

## AI Assistance

GitHub Copilot was used to scaffold the initial implementation, generate boilerplate (MIME type tables, error page HTML, CGI environment variable setup), and suggest fixes during code review. All generated code was reviewed, tested, and adjusted by the project team.
