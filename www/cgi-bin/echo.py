#!/usr/bin/env python3
import sys
import os

body = sys.stdin.read()

print("Content-Type: text/plain")
print()
print("METHOD=" + os.environ.get("REQUEST_METHOD", ""))
print("CONTENT_LENGTH=" + os.environ.get("CONTENT_LENGTH", ""))
print("BODY=" + body)