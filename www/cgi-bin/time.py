#!/usr/bin/env python3

from datetime import datetime

now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

print("Content-Type: text/html")
print()
print("<html>")
print("<body>")
print("<h1>Current server time</h1>")
print("<p>{}</p>".format(now))
print("</body>")
print("</html>")