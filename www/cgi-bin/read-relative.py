#!/usr/bin/env python3
import os

print("Content-Type: text/plain")
print()

print("cwd=" + os.getcwd())

try:
    with open("relative-data.txt", "r") as f:
        print("relative_file=" + f.read().strip())
except Exception as e:
    print("relative_file_error=" + str(e))
