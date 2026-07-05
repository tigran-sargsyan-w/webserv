# Cookies and sessions bonus demo

This branch adds a minimal server-side session demo for the `Support cookies and session management` bonus.

## Endpoints

- `GET /session`
  - creates a new session when the request has no valid `sid` cookie;
  - restores the existing session when the request sends a valid `sid` cookie;
  - increments a per-session visit counter;
  - sends `Set-Cookie: sid=...; Path=/; Max-Age=1800; HttpOnly; SameSite=Lax`.

- `GET /logout`
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
curl -i -b /tmp/webserv-session.cookies -c /tmp/webserv-session.cookies http://127.0.0.1:8080/logout

# New request after logout: creates a fresh session again.
curl -i -b /tmp/webserv-session.cookies -c /tmp/webserv-session.cookies http://127.0.0.1:8080/session
```

Expected behavior:

- the first `/session` response says `new session created`;
- the second `/session` response says `existing session restored from Cookie header`;
- `Visit count` increases when the same cookie is reused;
- `/logout` expires the cookie;
- a request after logout starts a new session.

## Notes

This is intentionally not a production-like authentication system. It is a compact in-memory demo suitable for showing cookie parsing, `Set-Cookie`, session creation, session lookup, session expiration, and logout during evaluation.
