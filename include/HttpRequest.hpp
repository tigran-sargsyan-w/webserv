#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>
#include "HttpMethod.hpp"

struct HttpRequest
{
    HttpMethod method;
    std::string methodString;
    std::string target;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;

    HttpRequest() : method(HTTP_UNKNOWN) {}
};

#endif