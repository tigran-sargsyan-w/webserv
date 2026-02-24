#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html")
print("")
print("<!DOCTYPE html>")
print("<html><head><title>CGI Hello</title></head><body>")
print("<h1>Hello from CGI!</h1>")
print("<h2>Environment Variables:</h2><ul>")
for key, val in sorted(os.environ.items()):
    print("<li><b>{}</b> = {}</li>".format(key, val))
print("</ul>")

method = os.environ.get("REQUEST_METHOD", "")
if method == "POST":
    content_length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
    body = sys.stdin.read(content_length) if content_length > 0 else ""
    print("<h2>POST Body:</h2><pre>{}</pre>".format(body))

print("</body></html>")
