#!/usr/bin/env python3
import os

print("Content-Type: text/plain")
print()

keys = [
    "SCRIPT_FILENAME",
    "DOCUMENT_ROOT",
    "REQUEST_URI",
    "REQUEST_SCHEME",
    "HTTPS",
    "SERVER_ADMIN",
    "REDIRECT_STATUS",
    "FCGI_ROLE",
    "PHP_SELF",
    "PATH",
    "PWD",
    "REQUEST_TIME",
    "REQUEST_TIME_FLOAT",
]

for key in keys:
    print(f"{key}={os.environ.get(key, '')}")