#include "ErrorPageResolver.hpp"

#include <fstream>
#include <map>
#include <sstream>
#include <string>

/**
 * @brief Creates an empty error page resolver.
 */
ErrorPageResolver::ErrorPageResolver() {}

/**
 * @brief Copies an error page resolver.
 * @param other - Resolver to copy.
 */
ErrorPageResolver::ErrorPageResolver(const ErrorPageResolver &other)
{
    (void)other;
}

/**
 * @brief Assigns an error page resolver.
 * @param other - Resolver to assign from.
 * @return Updated resolver.
 */
ErrorPageResolver &ErrorPageResolver::operator=(const ErrorPageResolver &other)
{
    (void)other;
    return (*this);
}

/**
 * @brief Destroys the resolver.
 */
ErrorPageResolver::~ErrorPageResolver() {}

static bool readFileContent(const std::string &path, std::string &body)
{
    std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
    std::ostringstream stream;

    if (!file.is_open())
        return (false);
    stream << file.rdbuf();
    if (file.bad())
        return (false);
    body = stream.str();
    return (true);
}

/**
 * @brief Loads a custom error page body.
 * @param statusCode - HTTP status code to resolve.
 * @param server - Server configuration.
 * @param body - Output body content.
 * @return True when the body was loaded.
 */
bool ErrorPageResolver::resolveBody(int statusCode, const ServerConfig &server, std::string &body)
{
    std::map<int, std::string>::const_iterator it;

    it = server.errorPages.find(statusCode);
    if (it == server.errorPages.end())
        return (false);
    return (readFileContent(it->second, body));
}