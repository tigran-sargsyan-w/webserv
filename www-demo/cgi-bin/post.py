import html
import os
import sys
from urllib.parse import parse_qs


def esc(value):
    return html.escape(value or "")

length = os.environ.get("CONTENT_LENGTH", "0")
try:
    size = int(length)
except ValueError:
    size = 0
body = sys.stdin.read(size) if size > 0 else ""
params = parse_qs(body)
payload = params.get("payload", params.get("message", [""]))[0]
name = params.get("name", [""])[0]

print("Content-Type: text/html")
print()
print("<!DOCTYPE html>")
print("<html lang=\"en\">")
print("<head>")
print("<meta charset=\"utf-8\">")
print("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">")
print("<title>CGI POST demo · webserv</title>")
print("<link rel=\"stylesheet\" href=\"/css/style.css\">")
print("</head>")
print("<body>")
print("<header class=\"nav\"><div class=\"nav-inner\">")
print("<a href=\"/index.html\" class=\"brand\"><span class=\"brand-dot\"></span><span><span class=\"brand-prompt\">~/</span>webserv</span></a>")
print("<nav class=\"nav-links\"><a href=\"/cgi.html\">Back to CGI</a><a href=\"/forms.html\">Forms</a><a href=\"/index.html\">Home</a></nav>")
print("</div></header>")
print("<main class=\"fade-in\">")
print("<section class=\"container\">")
print("<span class=\"eyebrow\">// Python CGI POST</span>")
print("<h1>CGI POST is working</h1>")
print("<p class=\"lead\" style=\"color:var(--muted)\">The request body was read from CGI stdin.</p>")
print("</section>")
print("<section class=\"section container\"><div class=\"grid\">")
print("<div class=\"card\"><h3>REQUEST_METHOD</h3><p><code>%s</code></p></div>" % esc(os.environ.get("REQUEST_METHOD", "")))
print("<div class=\"card\"><h3>CONTENT_LENGTH</h3><p><code>%s</code></p></div>" % esc(length))
print("<div class=\"card\"><h3>CONTENT_TYPE</h3><p><code>%s</code></p></div>" % esc(os.environ.get("CONTENT_TYPE", "")))
if name:
    print("<div class=\"card\"><h3>name</h3><p><code>%s</code></p></div>" % esc(name))
if payload:
    print("<div class=\"card\"><h3>payload</h3><p><code>%s</code></p></div>" % esc(payload))
print("<div class=\"card\"><h3>raw body</h3><p><code>%s</code></p></div>" % esc(body))
print("</div></section>")
print("</main>")
print("<footer><div class=\"container\"><span>webserv</span><span class=\"sep\">/</span><span>CGI POST demo</span></div></footer>")
print("</body></html>")
