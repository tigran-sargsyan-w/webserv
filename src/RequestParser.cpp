#include "RequestParser.hpp"
#include "Request.hpp"
#include "utils.hpp"
#include <sstream>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <iostream>

static int parseRequestLine(std::string &requestLine, Request &request) {
  if (requestLine.empty())
    return (1);
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

  struct stat st;
  stat(path.c_str(), &st);
 if (!(S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)))
 {
   std::cout << "Invalid path in request!\n";
   return (1);
  }
  return (0);
}

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

int RequestParser::parse(const std::string &rawRequest, Request& req) {

  std::istringstream ss(rawRequest);
  std::string requestLine;
  std::getline(ss, requestLine);
  if (requestLine.empty())
    return 1;
  if (requestLine[requestLine.length() - 1] == '\r')
    requestLine.erase(requestLine.length() - 1);
  if (parseRequestLine(requestLine, req))
  {
    return 1;
  }

  std::string headerLine;
  while (std::getline(ss, headerLine) && headerLine != "\r\n" &&
         !headerLine.empty()) {
    parseHeader(headerLine, req);
  }
  // TODO: parse body if Content-Length is present
  // TODO: check is request vaild and if it's finished
        if (req.getMethod().empty()) {
          std::cout << "Failed to parse request\n";
          return 1;
        }
          if (req.getMethod() != "GET") {
            std::cout << "Unsupported HTTP method: " << req.getMethod() << "\n";
          return 0;
          }
  return 0;
}
