#include "RequestParser.hpp"
#include "Request.hpp"
#include <sstream>
#include <string>
#include <cstring>

static void parseRequestLine(std::string &requestLine, Request &request) {
  if (requestLine.empty())
    return;
  if (requestLine[requestLine.length() - 1] == '\r')
    requestLine.erase(requestLine.length() - 1);

  std::istringstream lineStream(requestLine);
  std::string method;
  std::string path;
  std::string version;
  lineStream >> method >> path >> version;

  request.setMethod(method);
  request.setPath(path);
  request.setVersion(version);
}


#include <cstdio>
#include <unistd.h>

static void parseHeader(std::string &header, Request &request) {
  std::string key;
  std::string value;

  int colonIndex = header.find(":");
  key = header.substr(0, colonIndex);
  value = header.substr(colonIndex + 1);
  if (value[value.length() - 1] == '\r')
    value.erase(value.length() - 1);
  request.addHeader(key, value);
}

Request RequestParser::parse(const std::string &rawRequest) {
  Request request;

  std::istringstream ss(rawRequest);
  std::string requestLine;
  std::getline(ss, requestLine);
  if (requestLine.empty())
    return request;
  if (requestLine[requestLine.length() - 1] == '\r')
    requestLine.erase(requestLine.length() - 1);
  parseRequestLine(requestLine, request);

  std::string headerLine;
  while (std::getline(ss, headerLine) && headerLine != "\r\n" &&
         !headerLine.empty()) {
    parseHeader(headerLine, request);
  }
  // TODO: parse body if Content-Length is present
  return request;
}
