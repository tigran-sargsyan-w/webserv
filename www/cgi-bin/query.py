#!/usr/bin/env python3

import os

query = os.environ.get("QUERY_STRING", "")

print("Content-Type: text/html")
print()
print("<html>")
print("<body>")
print("<h1>Query string test</h1>")
print("<p>QUERY_STRING = {}</p>".format(query))
print("</body>")
print("</html>")