#!/bin/bash
# Basic test script for webserv

HOST="localhost"
PORT="8080"
BASE="http://${HOST}:${PORT}"
PASS=0
FAIL=0

check() {
    local desc="$1"
    local expected="$2"
    local actual="$3"
    if echo "$actual" | grep -q "$expected"; then
        echo "[PASS] $desc"
        PASS=$((PASS+1))
    else
        echo "[FAIL] $desc  (expected: $expected)"
        echo "       got: $actual"
        FAIL=$((FAIL+1))
    fi
}

echo "=== Testing webserv at $BASE ==="

# GET /
check "GET / returns 200" "200" "$(curl -s -o /dev/null -w '%{http_code}' $BASE/)"

# GET non-existent returns 404
check "GET /nonexistent returns 404" "404" \
    "$(curl -s -o /dev/null -w '%{http_code}' $BASE/nonexistent)"

# POST to /upload
check "POST /upload returns 201" "201" \
    "$(curl -s -o /dev/null -w '%{http_code}' -X POST --data 'hello' $BASE/upload)"

# DELETE on / returns 405
check "DELETE / returns 405" "405" \
    "$(curl -s -o /dev/null -w '%{http_code}' -X DELETE $BASE/)"

# GET /files/ autoindex
check "GET /files/ returns 200" "200" \
    "$(curl -s -o /dev/null -w '%{http_code}' $BASE/files/)"

echo ""
echo "Results: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ] && exit 0 || exit 1
