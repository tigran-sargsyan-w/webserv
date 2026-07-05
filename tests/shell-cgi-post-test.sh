#!/bin/sh

set -eu

HOST=${HOST:-127.0.0.1}
PORT=${PORT:-8080}
BODY='name=tigran&mode=shell-post'
URL="http://$HOST:$PORT/cgi-bin/hello.sh?mode=post"
TMP_FILE=$(mktemp)

cleanup() {
    rm -f "$TMP_FILE"
}
trap cleanup EXIT

curl -sS -i \
    -X POST \
    -H 'Content-Type: application/x-www-form-urlencoded' \
    --data "$BODY" \
    "$URL" > "$TMP_FILE"

grep -q 'HTTP/1.1 200 OK' "$TMP_FILE"
grep -q 'HELLO_FROM_SHELL_CGI' "$TMP_FILE"
grep -q 'REQUEST_METHOD=POST' "$TMP_FILE"
grep -q 'QUERY_STRING=mode=post' "$TMP_FILE"
grep -q 'CONTENT_TYPE=application/x-www-form-urlencoded' "$TMP_FILE"
grep -q "CONTENT_LENGTH=${#BODY}" "$TMP_FILE"
grep -q 'BODY:' "$TMP_FILE"
grep -q "$BODY" "$TMP_FILE"

echo 'PASS shell CGI POST test'
