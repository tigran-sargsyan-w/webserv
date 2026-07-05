# Cookies and sessions bonus demo

This branch adds a minimal server-side session demo for the `Support cookies and session management` bonus.

The bonus is enabled per location through config directives:

```conf
location / {
    methods GET;
    root ./www;
    index index.html;
    autoindex off;
    upload_enable off;
    session_enable on;
    session_path /session;
}
```

`session_path` defines the session demo endpoint. The logout endpoint is derived from it by appending `/logout`.

For example:

- `session_path /session;` gives:
  - `GET /session`
  - `GET /session/logout`
- `session_path /account/session;` gives:
  - `GET /account/session`
  - `GET /account/session/logout`

## HTML templates

The session pages are not hardcoded in C++.

`SessionHandler` reads templates from the configured route root:

- `session.html`
- `session-logout.html`

For the default config, the files are:

- `www/session.html`
- `www/session-logout.html`

For the demo config, the files are:

- `www-demo/session.html`
- `www-demo/session-logout.html`

Supported placeholders:

- `{{STATUS}}`
- `{{SESSION_ID}}`
- `{{VISIT_COUNT}}`
- `{{CREATED_AT}}`
- `{{LAST_SEEN}}`
- `{{SESSION_PATH}}`
- `{{LOGOUT_PATH}}`
- `{{MESSAGE}}`

## Endpoints with the default config

- `GET /session`
  - creates a new session when the request has no valid `sid` cookie;
  - restores the existing session when the request sends a valid `sid` cookie;
  - increments a per-session visit counter;
  - sends `Set-Cookie: sid=...; Path=/; Max-Age=1800; HttpOnly; SameSite=Lax`.

- `GET /session/logout`
  - removes the current server-side session if it exists;
  - sends an expired `sid` cookie with `Max-Age=0`.

Sessions are stored in memory and expire after 30 minutes of inactivity.

## Manual test

Run the server first:

```sh
make
./webserv configs/default.conf
```

Then test the session lifecycle:

```sh
# First request: creates a new session and stores the cookie locally.
curl -i -c /tmp/webserv-session.cookies http://127.0.0.1:8080/session

# Second request: sends the saved cookie back and should reuse the same session.
curl -i -b /tmp/webserv-session.cookies -c /tmp/webserv-session.cookies http://127.0.0.1:8080/session

# Logout: deletes the server-side session and expires the browser cookie.
curl -i -b /tmp/webserv-session.cookies -c /tmp/webserv-session.cookies http://127.0.0.1:8080/session/logout

# New request after logout: creates a fresh session again.
curl -i -b /tmp/webserv-session.cookies -c /tmp/webserv-session.cookies http://127.0.0.1:8080/session
```

Expected behavior:

- the first `/session` response says `new session created`;
- the second `/session` response says `existing session restored from Cookie header`;
- `Visit count` increases when the same cookie is reused;
- `/session/logout` expires the cookie;
- a request after logout starts a new session.

## Notes

This is intentionally not a production-like authentication system. It is a compact in-memory demo suitable for showing cookie parsing, `Set-Cookie`, session creation, session lookup, session expiration, and logout during evaluation.
