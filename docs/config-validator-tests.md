# Config Validator Test Cases

This document contains manual test cases for the configuration parser and validator of the `webserv` project.

The goal is to verify that invalid configuration files are rejected early, before the server starts listening for client connections.

The validator should prevent ambiguous, unsafe, or unsupported configuration states.

Covered features:

* required `server` block validation;
* `listen` host and port validation;
* required server `root`;
* required `client_max_body_size`;
* `client_timeout` range validation;
* `error_page` status code and path validation;
* `location` path validation;
* duplicate `location` detection;
* accepted HTTP methods validation;
* `autoindex` value validation;
* upload configuration validation;
* `upload_store` filesystem validation at startup (exists, directory, writable);
* redirect configuration validation;
* CGI extension and executable validation;
* duplicate CGI extension detection;
* duplicate `listen` host/port detection;
* duplicate `listen` + `server_name` detection;
* required `location` block per server;
* stricter path validation;
* double-slash `location` path rejection;
* absolute CGI executable path validation.

---

## 1. Why this matters

The subject requires the server to be configured through a configuration file.

The configuration file must be able to define:

* interface and port pairs;
* default error pages;
* maximum client body size;
* route-level rules;
* accepted HTTP methods;
* redirects;
* root directories;
* autoindex behavior;
* upload authorization and storage location;
* CGI execution based on file extension.

Invalid configuration should fail during startup instead of producing undefined runtime behavior.

---

## 2. Preparation

### Build the project

From the project root:
```bash
make re
```

Expected result:
```txt
webserv is built successfully
```

---

## 3. How to run the tests

To run a test:
```bash
./webserv configs/invalid/<config-name>.conf
```

For invalid configs, the server should not start.

The expected output should contain either:
```txt
Config parse error
```

or:
```txt
Config validation error
```

depending on whether the error is detected by the parser or by the validator.

---

## 4. Test: valid minimal config

### Command
```bash
./webserv configs/invalid/valid_minimal.conf
```

### Expected result

The server should start successfully.

Expected output should include something similar to:
```txt
Listening on 127.0.0.1:8080
WebServ run called!
```

Stop the server with:
```txt
Ctrl+C
```

### Purpose

Checks that a valid minimal configuration is accepted.

---

## 5. Test: empty config file

### Command
```bash
./webserv configs/invalid/empty.conf
```

### Expected result

The server should not start.

Expected error should mention that at least one server block is required, for example:
```txt
Config validation error: at least one server block is required
```

### Purpose

Checks that an empty config is rejected.

---

## 6. Test: unknown top-level directive

### Command
```bash
./webserv configs/invalid/unknown_top_level.conf
```

### Expected result

The server should not start.

Expected error should mention that only `server` blocks are allowed at top level.

### Purpose

Checks that the parser rejects unsupported top-level blocks.

---

## 7. Test: missing server root

### Command
```bash
./webserv configs/invalid/missing_server_root.conf
```

### Expected result

The server should not start.

Expected error should mention that the server requires `root`, for example:
```txt
Config validation error: server 0 requires root
```

### Purpose

Checks that the main server root is mandatory.

---

## 8. Test: missing client_max_body_size

### Command
```bash
./webserv configs/invalid/missing_client_max_body_size.conf
```

### Expected result

The server should not start.

Expected error should mention `client_max_body_size`, for example:
```txt
Config validation error: server 0 requires client_max_body_size greater than 0
```

### Purpose

Checks that the max client body size is explicitly configured.

---

## 9. Test: invalid client_max_body_size value

### Command
```bash
./webserv configs/invalid/invalid_client_max_body_size.conf
```

### Expected result

The server should not start.

Expected error should mention an invalid size value.

### Purpose

Checks that non-numeric body size values are rejected.

---

## 9b. Test: invalid client_timeout value (zero)

### Command
```bash
./webserv configs/invalid/client-timeout-zero.conf
```

### Expected result
The server should not start.
Expected error should mention `client_timeout`, for example:
```bash
Config validation error: server 0 requires client_timeout greater than 0
```

### Purpose
Checks that non-positive client timeout values are rejected.

---

## 9c. Test: client_timeout above maximum

### Command
```bash
./webserv configs/invalid/client-timeout-too-large.conf
```

### Expected result
The server should not start.
Expected error should mention `client_timeout`, for example:
```bash
Config validation error: server 0 requires client_timeout at most 3600
```

### Purpose
Checks that excessively large timeout values are rejected before poll timeout conversion.

---

## 10. Test: invalid listen port

### Command
```bash
./webserv configs/invalid/invalid_listen_port.conf
```

### Expected result

The server should not start.

Expected error should mention an invalid listen port, for example:
```txt
Config validation error: server 0 has invalid listen port
```

### Purpose

Checks that ports outside the valid range are rejected.

---

## 11. Test: invalid listen host

### Command
```bash
./webserv configs/invalid/invalid_listen_host.conf
```

### Expected result

The server should not start.

Expected error should mention an invalid or empty listen host.

### Purpose

Checks that `listen` cannot contain an empty host before `:`.

---

## 12. Test: invalid location path

### Command
```bash
./webserv configs/invalid/invalid_location_path.conf
```

### Expected result

The server should not start.

Expected error should mention that the location path is invalid.

### Purpose

Checks that location paths must start with `/`.

---

## 13. Test: duplicate location path

### Command
```bash
./webserv configs/invalid/duplicate_location.conf
```

### Expected result

The server should not start.

Expected error should mention the duplicate location, for example:
```txt
Config validation error: server 0 has duplicate location: /
```

### Purpose

Checks that one server block cannot contain two routes with the same path.


---

## 14. Test: duplicate server listen and server_name

### Command
```bash
./webserv configs/invalid/duplicate_server_same_name.conf
```

### Expected result

The server should not start.

Expected error should mention a duplicate server block, for example:
```txt
Config validation error: duplicate server block for 127.0.0.1:8080 with server_name 'duplicate_test'
```

### Purpose

Checks that two server blocks cannot use the same `listen` host, port and `server_name`.

---

## 14b. Test: duplicate listen on same host and port

### Command
```bash
./webserv configs/invalid/duplicate_listen_same_port.conf
```

### Expected result

The server should not start.

Expected error should mention a duplicate listen, for example:
```bash
Config validation error: duplicate listen for 127.0.0.1:19091
```

### Purpose

Checks that two server blocks cannot bind to the same `listen` address, even when `server_name` differs. This matches the eval requirement: setting up the same port multiple times in one config must not work.

---

## 15. Test: server without location

### Command
```bash
./webserv configs/invalid/server_without_location.conf
```

### Expected result

The server should not start.

Expected error should mention that the server requires at least one location, for example:
```txt
Config validation error: server 0 requires at least one location
```

### Purpose

Checks that a server block is not accepted without any route definition.

---

## 16. Test: double-slash location path

### Command
```bash
./webserv configs/invalid/double_slash_location.conf
```

### Expected result

The server should not start.

Expected error should mention an invalid location path.

### Purpose

Checks that location paths must not start with `//`.

---

## 17. Test: empty root value

### Command
```bash
./webserv configs/invalid/empty_root_value.conf
```

### Expected result

The server should not start.

Expected error should mention a missing or invalid root value.

### Purpose

Checks that empty path values are rejected.

---

## 18. Test: empty error_page path

### Command
```bash
./webserv configs/invalid/empty_error_page_path.conf
```

### Expected result

The server should not start.

Expected error should mention a missing or invalid error page path.

### Purpose

Checks that empty error page paths are rejected.

---

## 19. Test: null byte in root path

### Command
```bash
./webserv configs/invalid/null_byte_root.conf
```

### Expected result

The server should not start.

The error can come from either the parser or the validator.

### Purpose

Checks that paths containing a null byte are rejected.

---

## 20. Test: CGI executable with relative path

### Command
```bash
./webserv configs/invalid/invalid_cgi_relative_executable.conf
```

### Expected result

The server should not start.

Expected error should mention an invalid CGI executable path.

### Purpose

Checks that CGI executables must be configured with an absolute path.

---

## 21. Test: CGI executable path ending with slash

### Command
```bash
./webserv configs/invalid/invalid_cgi_executable_directory.conf
```

### Expected result

The server should not start.

Expected error should mention an invalid CGI executable path.

### Purpose

Checks that a CGI executable path cannot point to a directory-like path.

---

## 22. Test: location without methods

### Command
```bash
./webserv configs/invalid/location_without_methods.conf
```

### Expected result

The server should not start.

Expected error should mention that the location has no allowed methods, for example:
```txt
Config validation error: location / has no allowed methods
```

### Purpose

Checks that every route explicitly defines accepted HTTP methods.

---

## 23. Test: unknown HTTP method

### Command
```bash
./webserv configs/invalid/unknown_method.conf
```

### Expected result

The server should not start.

Expected error should mention an unknown HTTP method.

### Purpose

Checks that only supported methods are accepted in the config.

Current mandatory methods are:
```txt
GET, POST, DELETE
```

---

## 24. Test: invalid autoindex value

### Command
```bash
./webserv configs/invalid/invalid_autoindex.conf
```

### Expected result

The server should not start.

Expected error should mention that autoindex must be `on` or `off`.

### Purpose

Checks that boolean-like config directives accept only supported values.

---

## 25. Test: invalid upload_enable value

### Command
```bash
./webserv configs/invalid/invalid_upload_enable.conf
```

### Expected result

The server should not start.

Expected error should mention that `upload_enable` must be `on` or `off`.

### Purpose

Checks that upload authorization is configured with the expected values.

---

## 26. Test: upload enabled without upload_store

### Command
```bash
./webserv configs/invalid/upload_enabled_without_store.conf
```

### Expected result

The server should not start.

Expected error should mention that `upload_store` is missing, for example:
```txt
Config validation error: location /uploads has upload_enable on but upload_store is missing
```

### Purpose

Checks that upload storage location is mandatory when uploads are enabled.

---

## 27. Test: upload_store while upload is disabled

### Command
```bash
./webserv configs/invalid/upload_store_with_upload_disabled.conf
```

### Expected result

The server should not start if the validator enforces strict upload configuration.

Expected error example:
```txt
Config validation error: location /uploads has upload_store but upload_enable is off
```

### Purpose

Checks that the configuration does not contain unused or contradictory upload settings.

### Current accepted limitation

If the project decides to allow `upload_store` while `upload_enable off`, this test can be moved to the limitation section or removed from the required checklist.

---

## 28. Test: upload_store filesystem validation at startup

### Preparation

Use `configs/default.conf`, which enables upload on `/uploads` with `upload_store ./www/uploads`.

Temporarily hide the upload directory:
```bash
mv www/uploads www/uploads.bak
```

### Command
```bash
./webserv configs/default.conf
```

### Expected result

The server must not start.

Expected error example:
```txt
Config validation error: location /uploads: upload_store ./www/uploads does not exist
```

### Cleanup
```bash
mv www/uploads.bak www/uploads
```

If the backup does not exist:
```bash
mkdir -p www/uploads
chmod u+rwx www/uploads
```

### Purpose

Checks that a missing `upload_store` path is rejected at startup instead of failing later during POST with `400 Bad Request`.

The validator must **not** create the directory automatically.

---

## 29. Test: upload_store is not a directory

### Preparation
```bash
touch www/fake_upload_store
```

### Command
```bash
./webserv configs/invalid/upload_store_is_file.conf
```

### Expected result

The server must not start.

Expected error example:
```txt
Config validation error: location /uploads has invalid upload_store directory
```

### Cleanup
```bash
rm -f www/fake_upload_store
```

### Purpose

Checks that `upload_store` cannot point to a regular file.

---

## 30. Test: upload_store is not writable

### Preparation
```bash
mkdir -p www/uploads_readonly
chmod a-w www/uploads_readonly
```

### Command
```bash
./webserv configs/invalid/upload_store_not_writable.conf
```

### Expected result

The server must not start.

Expected error example:
```txt
Config validation error: location /uploads upload_store directory is not writable
```

### Cleanup
```bash
chmod u+rwx www/uploads_readonly
```

### Purpose

Checks that the process has write permission on the configured upload directory before the server starts listening.

---

## 31. Test: valid upload_store allows server startup

### Preparation

Relative paths such as `./www/uploads` are resolved from the **project root** when `./webserv` is launched.
```bash
mkdir -p www/uploads
chmod u+rwx www/uploads
```

### Command
```bash
./webserv configs/default.conf
```

### Expected result

The server starts and prints a listening message, for example:
```txt
Listening on 127.0.0.1:8080
```

Stop the server with `Ctrl+C`, or keep it running for the upload check below.

### Upload regression

With the server running:
```bash
curl -i -X POST http://127.0.0.1:8080/uploads/small.txt \
  --data-binary @www/static/client-max-body-size-tests/small.txt
```

Expected:
```http
HTTP/1.1 201 Created
```

Verify on disk:
```bash
cat www/uploads/small.txt
```

Cleanup (optional):
```bash
rm -f www/uploads/small.txt
```

For more POST and DELETE cases, see `docs/post-delete-tests.md`.

### Purpose

Checks that a valid writable `upload_store` does not break startup or normal upload behavior.

---

## 32. Test: invalid redirect status code

### Command
```bash
./webserv configs/invalid/invalid_redirect_code.conf
```

### Expected result

The server should not start.

Expected error should mention an invalid redirect status code, for example:
```txt
Config validation error: location /bad-redirect has invalid redirect status code
```

### Purpose

Checks that `return` accepts only supported redirect codes.

Supported redirect codes:
```txt
301, 302, 303, 307, 308
```

---

## 33. Test: invalid redirect target

### Command
```bash
./webserv configs/invalid/invalid_redirect_target.conf
```

### Expected result

The server should not start.

Expected error should mention an invalid redirect target.

### Purpose

Checks that redirect targets are restricted to internal paths such as:
```txt
/
```

or:
```txt
/some-path
```

---

## 34. Test: invalid error_page status code

### Command
```bash
./webserv configs/invalid/invalid_error_page_code.conf
```

### Expected result

The server should not start.

Expected error should mention an invalid `error_page` code, for example:
```txt
Config validation error: server 0 has invalid error_page code: 301
```

### Purpose

Checks that `error_page` is used for error status codes only.

Accepted range:
```txt
400-599
```

---

## 35. Test: valid error_page status codes

### Command
```bash
./webserv configs/invalid/valid_error_pages.conf
```

### Expected result

The server should start successfully.

Stop the server with:
```txt
Ctrl+C
```

### Purpose

Checks that valid HTTP error status codes are accepted.

---

## 36. Test: invalid CGI extension without dot

### Command
```bash
./webserv configs/invalid/invalid_cgi_extension_without_dot.conf
```

### Expected result

The server should not start.

Expected error should mention an invalid CGI extension.

### Purpose

Checks that CGI extensions must use the expected extension format:
```txt
.py
.php
```

---

## 37. Test: invalid CGI extension with only dot

### Command
```bash
./webserv configs/invalid/invalid_cgi_extension_only_dot.conf
```

### Expected result

The server should not start.

Expected error should mention an invalid CGI extension.

### Purpose

Checks that `.` alone is not accepted as a CGI extension.

---

## 38. Test: duplicate CGI extension

### Command
```bash
./webserv configs/invalid/duplicate_cgi_extension.conf
```

### Expected result

The server should not start.

Expected error should mention the duplicate CGI extension, for example:
```txt
Config validation error: location /cgi-bin has duplicate CGI extension: .py
```

### Purpose

Checks that a single location cannot define two executables for the same CGI extension.

---

## 39. Test: valid CGI configuration

### Command
```bash
./webserv configs/invalid/valid_cgi.conf
```

### Expected result

The server should start successfully.

Stop the server with:
```txt
Ctrl+C
```

### Purpose

Checks that multiple different CGI extensions can be configured in the same location.

---

## 40. Test: missing semicolon

### Command
```bash
./webserv configs/invalid/missing_semicolon.conf
```

### Expected result

The server should not start.

Expected error should mention a missing semicolon, for example:
```txt
Config parse error
```

### Purpose

Checks that syntax errors are caught by the parser before validation.

---

## 41. Test: missing closing brace

### Command
```bash
./webserv configs/invalid/missing_closing_brace.conf
```

### Expected result

The server should not start.

Expected error should mention that a closing brace is expected.

### Purpose

Checks that incomplete config blocks are rejected.

---

## 42. Quick regression checklist

Before opening or merging the PR, run these checks manually from the project root.

First rebuild the project:
```bash
make re
```

Then run each config file:
```bash
./webserv configs/invalid/valid_minimal.conf
./webserv configs/invalid/empty.conf
./webserv configs/invalid/unknown_top_level.conf
./webserv configs/invalid/missing_server_root.conf
./webserv configs/invalid/missing_client_max_body_size.conf
./webserv configs/invalid/invalid_client_max_body_size.conf
./webserv configs/invalid/invalid_listen_port.conf
./webserv configs/invalid/invalid_listen_host.conf
./webserv configs/invalid/invalid_location_path.conf
./webserv configs/invalid/duplicate_location.conf
./webserv configs/invalid/location_without_methods.conf
./webserv configs/invalid/unknown_method.conf
./webserv configs/invalid/invalid_autoindex.conf
./webserv configs/invalid/invalid_upload_enable.conf
./webserv configs/invalid/upload_enabled_without_store.conf
./webserv configs/invalid/upload_store_with_upload_disabled.conf
./webserv configs/invalid/upload_store_is_file.conf
./webserv configs/invalid/upload_store_not_writable.conf
./webserv configs/invalid/invalid_redirect_code.conf
./webserv configs/invalid/invalid_redirect_target.conf
./webserv configs/invalid/invalid_error_page_code.conf
./webserv configs/invalid/valid_error_pages.conf
./webserv configs/invalid/invalid_cgi_extension_without_dot.conf
./webserv configs/invalid/invalid_cgi_extension_only_dot.conf
./webserv configs/invalid/duplicate_cgi_extension.conf
./webserv configs/invalid/valid_cgi.conf
./webserv configs/invalid/missing_semicolon.conf
./webserv configs/invalid/missing_closing_brace.conf
./webserv configs/invalid/duplicate_server_same_name.conf
./webserv configs/invalid/duplicate_listen_same_port.conf
./webserv configs/invalid/server_without_location.conf
./webserv configs/invalid/double_slash_location.conf
./webserv configs/invalid/empty_root_value.conf
./webserv configs/invalid/empty_error_page_path.conf
./webserv configs/invalid/null_byte_root.conf
./webserv configs/invalid/client-timeout-zero.conf
./webserv configs/invalid/client-timeout-too-large.conf
```

The following configs are valid and should start the server:
```txt
valid_minimal.conf
valid_error_pages.conf
valid_cgi.conf
```

Stop the server manually with:
```txt
Ctrl+C
```

All other configs should fail during parsing or validation.

Expected summary:

| Config file                              | Expected result | Purpose                             |
| ---------------------------------------- | --------------: | ----------------------------------- |
| `valid_minimal.conf`                     |          starts | valid baseline                      |
| `empty.conf`                             |           fails | no server block                     |
| `unknown_top_level.conf`                 |           fails | unsupported top-level block         |
| `missing_server_root.conf`               |           fails | required server root                |
| `missing_client_max_body_size.conf`      |           fails | required body size limit            |
| `invalid_client_max_body_size.conf`      |           fails | invalid body size value             |
| `client-timeout-zero.conf`               |           fails | client_timeout must be over 0       |
| `client-timeout-too-large.conf`          |           fails | client_timeout must be at most 3600 |
| `invalid_listen_port.conf`               |           fails | invalid port range                  |
| `invalid_listen_host.conf`               |           fails | invalid listen host                 |
| `invalid_location_path.conf`             |           fails | location must start with `/`        |
| `duplicate_location.conf`                |           fails | duplicate route path                |
| `location_without_methods.conf`          |           fails | route methods required              |
| `unknown_method.conf`                    |           fails | unsupported method                  |
| `invalid_autoindex.conf`                 |           fails | autoindex must be `on` or `off`     |
| `invalid_upload_enable.conf`             |           fails | upload_enable must be `on` or `off` |
| `upload_enabled_without_store.conf`      |           fails | upload_store required               |
| `upload_store_with_upload_disabled.conf` |           fails | contradictory upload config         |
| `default.conf` with missing `www/uploads`|           fails | `upload_store` does not exist       |
| `upload_store_is_file.conf`              |           fails | `upload_store` not a directory      |
| `upload_store_not_writable.conf`         |           fails | `upload_store` not writable         |
| `default.conf` with valid `www/uploads`  |          starts | valid upload directory              |
| `invalid_redirect_code.conf`             |           fails | invalid redirect code               |
| `invalid_redirect_target.conf`           |           fails | invalid redirect target             |
| `invalid_error_page_code.conf`           |           fails | invalid error_page code             |
| `valid_error_pages.conf`                 |          starts | valid error pages                   |
| `invalid_cgi_extension_without_dot.conf` |           fails | invalid CGI extension               |
| `invalid_cgi_extension_only_dot.conf`    |           fails | invalid CGI extension               |
| `duplicate_cgi_extension.conf`           |           fails | duplicate CGI extension             |
| `valid_cgi.conf`                         |          starts | valid CGI config                    |
| `missing_semicolon.conf`                 |           fails | parser syntax error                 |
| `missing_closing_brace.conf`             |           fails | parser syntax error                 |
| `duplicate_server_same_name.conf`        |           fails | duplicate listen and server_name    |
| duplicate_listen_same_port.conf          |           fails | duplicate listen host/port          |
| `server_without_location.conf`           |           fails | server route block required         |
| `double_slash_location.conf`             |           fails | location must not start with `//`   |
| `empty_root_value.conf`                  |           fails | empty path value                    |
| `empty_error_page_path.conf`             |           fails | empty error_page path               |
| `null_byte_root.conf`                    |           fails | null byte in path                   |
| `invalid_cgi_relative_executable.conf`   |           fails | CGI executable must be absolute     |
| `invalid_cgi_executable_directory.conf`  |           fails | CGI executable cannot end with `/`  |
| `invalid_client_max_body_size.conf`      |           fails | invalid body size value             |

---

## 43. Automated quick regression command

The valid configs start the server and normally keep running.

For this reason, the command below uses `timeout`.

Exit code meaning:
```txt
124 = the server started and was stopped by timeout
non-zero and not 124 = parsing or validation failed
0 = suspicious, because the server should not exit immediately after startup
```

Run this from the project root:
```bash

make re && \
for file in configs/invalid/*.conf; do
    name=$(basename "$file")

    case "$name" in
        valid_minimal.conf|valid_error_pages.conf|valid_cgi.conf)
            expected="starts"
            ;;
        *)
            expected="fails"
            ;;
    esac

    timeout 1s ./webserv "$file" > /tmp/webserv_config_test.out 2>&1
    code=$?

    if [ "$expected" = "starts" ]; then
        if [ "$code" -eq 124 ]; then
            echo "✅ PASS: $name started successfully"
        else
            echo "❌ FAIL: $name should start but exited"
            cat /tmp/webserv_config_test.out
            echo
        fi
    else
        if [ "$code" -ne 0 ] && [ "$code" -ne 124 ]; then
            echo "✅ PASS: $name failed as expected"
        else
            echo "❌ FAIL: $name should fail but started or exited successfully"
            cat /tmp/webserv_config_test.out
            echo
        fi
    fi
done
```

Expected output should contain only `PASS` lines.

Example:
```txt
✅ PASS: duplicate_cgi_extension.conf failed as expected
✅ PASS: duplicate_location.conf failed as expected
✅ PASS: empty.conf failed as expected
✅ PASS: invalid_autoindex.conf failed as expected
✅ PASS: valid_cgi.conf started successfully
✅ PASS: valid_error_pages.conf started successfully
✅ PASS: valid_minimal.conf started successfully
```

If a config prints `FAIL`, this means one of two things:

* the validator does not catch this invalid case yet;
* the test expectation needs to be adjusted because the project intentionally accepts that config.

---