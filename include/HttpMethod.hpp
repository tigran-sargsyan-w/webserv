#ifndef HTTPMETHOD_HPP
#define HTTPMETHOD_HPP

#include <string>

enum HttpMethod
{
    HTTP_GET,
    HTTP_POST,
    HTTP_DELETE,
    HTTP_UNKNOWN
};

HttpMethod parseHttpMethod(const std::string &method);
std::string httpMethodToString(HttpMethod method);

#endif