#include "HttpMethod.hpp"

/**
 * @brief Convert a method string to its enum value.
 */
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

/**
 * @brief Convert an HTTP method enum to its string form.
 */
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