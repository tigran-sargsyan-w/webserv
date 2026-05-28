#!/usr/bin/env python3
import time
import os

print("Content-Type: text/plain")
print()
print("CGI started, pid =", os.getpid(), flush=True)

time.sleep(50)

print("CGI finished")
