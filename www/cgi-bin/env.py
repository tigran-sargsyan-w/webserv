#!/usr/bin/env python3
import os
import sys

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

http_keys = sorted(key for key in os.environ if key.startswith("HTTP_"))

for key in http_keys:
    print(f"{key}={os.environ.get(key, '<missing>')}")

print()
print("BODY:")
print(sys.stdin.read())