#include "Response.hpp"
#include <fstream>
#include <sstream>

Response::Response()
{

}

Response::~Response()
{

}

std::string Response::toString() const
{
    std::string response = "HTTP/1.1 "  + _statusCode + "\r\n";

    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it)
    {
        response += it->first + ": " + it->second + "\r\n";
    }
    response += "\r\n" + _body;
    return response;
}


void Response::setBodyFromFile(const std::string& path)
{
  std::ifstream file(path);

  std::ostringstream ss;
  ss << file.rdbuf();
  std::string res = ss.str();
  _body = res;
}

