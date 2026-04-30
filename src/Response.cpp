#include "Response.hpp"
#include "utils.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

Response::Response() {}

Response::~Response() {}

std::string Response::getReasonPhrase() const {

  switch (this->statusCode) {
  case 200:
    return "OK";
  case 404:
    return "Not Found";
  case 403:
        return ("Forbidden");
  default:
    return "Unknown";
  }
}

std::string Response::toString() const {
  std::string response =
      "HTTP/1.1 " + intToString(this->statusCode) + " " + getReasonPhrase() + "\r\n";

  for (std::map<std::string, std::string>::const_iterator it = this->headers.begin();
       it != this->headers.end(); ++it) {
    response += it->first + ": " + it->second + "\r\n";
  }
  response += "\r\n" + this->body;
  return response;
}

void Response::setBodyFromFile(const std::string &path) {
  std::ifstream file(path.c_str());

  if (!file) {
    std::cerr << "File not found!\n";
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  this->body = ss.str();
}
