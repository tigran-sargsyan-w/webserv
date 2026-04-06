#include "Response.hpp"
#include "utils.hpp"
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

Response::Response()
{

}

Response::~Response()
{

}

std::string Response::getReasonPhrase() const
{

  switch (_statusCode)
  {
    case 200: return "OK";
    case 404: return "Not Found";
    default: return "Unknown";
  }
}

std::string Response::toString() const
{
    std::string response = "HTTP/1.1 "  + intToString(_statusCode) + " " + getReasonPhrase() + "\r\n";

    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it)
    {
        response += it->first + ": " + it->second + "\r\n";
    }
    response += "\r\n" + _body;
    return response;
}

void Response::setBodyFromFile(const std::string& path)
{
  std::ifstream file(path.c_str());

  if (!file)
  {
    std::cerr << "File not found!\n";
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  _body = ss.str();
}

