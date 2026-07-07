import time

# Must be longer than CGI_TIMEOUT_SECONDS currently configured in the server.
time.sleep(15)

print("Content-Type: text/html")
print()
print("<!DOCTYPE html>")
print("<html lang=\"en\"><head><meta charset=\"utf-8\"><title>Slow CGI</title></head>")
print("<body><h1>This should normally timeout before being displayed.</h1></body></html>")
