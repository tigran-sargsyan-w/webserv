#!/bin/sh

echo "Content-Type: text/plain"
echo ""
echo "HELLO_FROM_SHELL_CGI"
echo "REQUEST_METHOD=$REQUEST_METHOD"
echo "QUERY_STRING=$QUERY_STRING"
