#include "HttpMethod.hpp"

HttpMethod parseHttpMethod(const std::string &method)
{
    if (method == "GET")
        return (HTTP_GET);
    if (method == "POST")
        return (HTTP_POST);
    if (method == "DELETE")
        return (HTTP_DELETE);
    return (HTTP_UNKNOWN);
}

std::string httpMethodToString(HttpMethod method)
{
    if (method == HTTP_GET)
        return ("GET");
    if (method == HTTP_POST)
        return ("POST");
    if (method == HTTP_DELETE)
        return ("DELETE");
    return ("UNKNOWN");
}