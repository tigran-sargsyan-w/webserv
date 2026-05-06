# webserv
This project aims to create your own HTTP server. You will be able to test it with a real web browser. HTTP is one of the most used protocols on the internet. Knowing its intricacies will be useful, even if web development is not on your carreer path.


## CGI Standard Meta-Variables Tests

This section contains manual tests used to verify CGI standard meta-variables.

The tests assume that the server is running on:

```bash
http://localhost:8080
```

and that the CGI test scripts/files are available at:

```bash
/cgi-bin/env.py
/cgi-bin/time.py
/cgi-bin/forbidden.txt
```

`env.py` is used to verify CGI environment variables.

`time.py` is used to verify that another valid CGI script can also be executed from the same CGI location.

`forbidden.txt` is used to verify that files with non-configured CGI extensions are not executed as CGI.

---

### 1. Test CGI environment with query string

```bash
curl -i "http://localhost:8080/cgi-bin/env.py?x=42"
```

Expected important values:

```txt
REQUEST_METHOD=GET
SCRIPT_NAME=/cgi-bin/env.py
PATH_INFO=
PATH_TRANSLATED=
QUERY_STRING=x=42
REMOTE_ADDR=127.0.0.1
GATEWAY_INTERFACE=CGI/1.1
SERVER_NAME=default_server
SERVER_PORT=8080
SERVER_PROTOCOL=HTTP/1.1
SERVER_SOFTWARE=webserv/1.0
```

This test checks that:

* the CGI script is detected correctly;
* the query string is extracted correctly;
* `SCRIPT_NAME` does not include the query string;
* `REMOTE_ADDR` is passed to the CGI process;
* standard server variables are present.

---

### 2. Test CGI environment without query string

```bash
curl -i "http://localhost:8080/cgi-bin/env.py"
```

Expected important values:

```txt
REQUEST_METHOD=GET
SCRIPT_NAME=/cgi-bin/env.py
PATH_INFO=
PATH_TRANSLATED=
QUERY_STRING=
REMOTE_ADDR=127.0.0.1
```

This test checks that `QUERY_STRING` exists but is empty when no query string is provided.

---

### 3. Test CGI `PATH_INFO`

```bash
curl -i "http://localhost:8080/cgi-bin/env.py/extra/path?x=42"
```

Expected important values:

```txt
REQUEST_METHOD=GET
SCRIPT_NAME=/cgi-bin/env.py
PATH_INFO=/extra/path
PATH_TRANSLATED=./www/cgi-bin/extra/path
QUERY_STRING=x=42
REMOTE_ADDR=127.0.0.1
```

This test checks that the server correctly splits the request URI into:

```txt
SCRIPT_NAME=/cgi-bin/env.py
PATH_INFO=/extra/path
```

and builds the corresponding translated path:

```txt
PATH_TRANSLATED=./www/cgi-bin/extra/path
```

---

### 4. Test another valid CGI script

```bash
curl -i "http://localhost:8080/cgi-bin/time.py"
```

Expected behavior:

```txt
The server should execute time.py as CGI if .py is configured as a CGI extension.
```

This test checks that CGI execution is not hardcoded only for `env.py` and that any configured `.py` script inside `/cgi-bin` can be executed.

---

### 5. Test another valid CGI script with query string

```bash
curl -i "http://localhost:8080/cgi-bin/time.py?format=unix"
```

Expected important values if `time.py` prints CGI variables or uses the query string internally:

```txt
SCRIPT_NAME=/cgi-bin/time.py
QUERY_STRING=format=unix
REQUEST_METHOD=GET
```

This test checks that query string handling works for different CGI scripts, not only for `env.py`.

---

### 6. Test non-CGI file inside CGI location

```bash
curl -i "http://localhost:8080/cgi-bin/forbidden.txt"
```

Expected behavior:

```txt
The server must not execute forbidden.txt as CGI if only .py is configured.
```

Depending on the current route policy, the response may be:

```txt
403 Forbidden
```

This test checks that the server does not execute files with non-configured extensions as CGI, even if they are inside a CGI-enabled location.

---

### 7. Test CGI extension boundary

```bash
curl -i "http://localhost:8080/cgi-bin/env.pybackup"
```

Expected behavior:

```txt
The request must not be treated as a valid .py CGI script.
```

The server should not execute CGI just because `.py` appears inside the filename.

This test checks that:

```txt
/cgi-bin/env.py
```

is treated as CGI, but:

```txt
/cgi-bin/env.pybackup
```

is not.

---

### 8. Test unknown CGI extension

```bash
curl -i "http://localhost:8080/cgi-bin/env.php"
```

Expected behavior:

```txt
The request must not be executed as CGI if only .py is configured.
```

This test checks that CGI execution depends on configured extensions.

---

### 9. Test POST CGI environment

```bash
curl -i -X POST \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "name=tigran" \
  "http://localhost:8080/cgi-bin/env.py"
```

Expected important values:

```txt
REQUEST_METHOD=POST
CONTENT_TYPE=application/x-www-form-urlencoded
CONTENT_LENGTH=11
SCRIPT_NAME=/cgi-bin/env.py
QUERY_STRING=
REMOTE_ADDR=127.0.0.1
```

This test checks that POST requests are parsed correctly and that `CONTENT_TYPE` and `CONTENT_LENGTH` are passed to CGI.

Important note:

```txt
At this stage, the request body may be stored in Request::body, but the CGI script will not be able to read it from stdin until CGI stdin pipe support is implemented.
```

---

### 10. Test POST with query string

```bash
curl -i -X POST \
  -H "Content-Type: application/x-www-form-urlencoded" \
  -d "name=tigran" \
  "http://localhost:8080/cgi-bin/env.py?mode=test"
```

Expected important values:

```txt
REQUEST_METHOD=POST
CONTENT_TYPE=application/x-www-form-urlencoded
CONTENT_LENGTH=11
QUERY_STRING=mode=test
SCRIPT_NAME=/cgi-bin/env.py
```

This test checks that POST body metadata and query string can exist together.

---

### 11. Test all standard CGI meta-variables are present

```bash
curl -i "http://localhost:8080/cgi-bin/env.py?x=42"
```

Expected variables:

```txt
AUTH_TYPE=
CONTENT_LENGTH=
CONTENT_TYPE=
GATEWAY_INTERFACE=CGI/1.1
PATH_INFO=
PATH_TRANSLATED=
QUERY_STRING=x=42
REMOTE_ADDR=127.0.0.1
REMOTE_HOST=
REMOTE_IDENT=
REMOTE_USER=
REQUEST_METHOD=GET
SCRIPT_NAME=/cgi-bin/env.py
SERVER_NAME=default_server
SERVER_PORT=8080
SERVER_PROTOCOL=HTTP/1.1
SERVER_SOFTWARE=webserv/1.0
```

Some variables are expected to be empty for now:

```txt
AUTH_TYPE
REMOTE_HOST
REMOTE_IDENT
REMOTE_USER
```

For a simple GET request, these are also expected to be empty:

```txt
CONTENT_LENGTH
CONTENT_TYPE
PATH_INFO
PATH_TRANSLATED
```

---

### 12. Optional debug mode

During development, CGI environment variables can be printed on the server side before `execve()`.

Example output:

```txt
===== CGI ENV DEBUG =====
AUTH_TYPE=
CONTENT_LENGTH=
CONTENT_TYPE=
GATEWAY_INTERFACE=CGI/1.1
PATH_INFO=/extra/path
PATH_TRANSLATED=./www/cgi-bin/extra/path
QUERY_STRING=x=42
REMOTE_ADDR=127.0.0.1
REMOTE_HOST=
REMOTE_IDENT=
REMOTE_USER=
REQUEST_METHOD=GET
SCRIPT_NAME=/cgi-bin/env.py
SERVER_NAME=default_server
SERVER_PORT=8080
SERVER_PROTOCOL=HTTP/1.1
SERVER_SOFTWARE=webserv/1.0
=========================
```

It is recommended to guard this debug output with a macro:

```cpp
#ifdef DEBUG_CGI
	debugPrintEnv(env);
#endif
```

Then debug mode can be enabled with:

```bash
make CXXFLAGS="-Wall -Wextra -Werror -std=c++98 -g -DDEBUG_CGI"
```

---

## Test CGI script

Example `env.py` script used for testing:

```python
#!/usr/bin/env python3
import os

print("Content-Type: text/plain")
print()

keys = [
    "AUTH_TYPE",
    "CONTENT_LENGTH",
    "CONTENT_TYPE",
    "GATEWAY_INTERFACE",
    "PATH_INFO",
    "PATH_TRANSLATED",
    "QUERY_STRING",
    "REMOTE_ADDR",
    "REMOTE_HOST",
    "REMOTE_IDENT",
    "REMOTE_USER",
    "REQUEST_METHOD",
    "SCRIPT_NAME",
    "SERVER_NAME",
    "SERVER_PORT",
    "SERVER_PROTOCOL",
    "SERVER_SOFTWARE",
]

for key in keys:
    print(f"{key}={os.environ.get(key, '<missing>')}")
```

Make it executable:

```bash
chmod +x ./www/cgi-bin/env.py
```

The value `<missing>` means that the variable was not passed to the CGI process at all.

An empty value like:

```txt
AUTH_TYPE=
```

means that the variable exists, but its value is empty.
