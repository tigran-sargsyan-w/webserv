#include "ErrorPageResolver.hpp"

#include <fstream>
#include <map>
#include <sstream>
#include <string>

ErrorPageResolver::ErrorPageResolver() {}

ErrorPageResolver::ErrorPageResolver(const ErrorPageResolver &other)
{
    (void)other;
}

ErrorPageResolver &ErrorPageResolver::operator=(const ErrorPageResolver &other)
{
    (void)other;
    return (*this);
}

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

bool ErrorPageResolver::resolveBody(int statusCode, const ServerConfig &server, std::string &body)
{
    std::map<int, std::string>::const_iterator it;

    it = server.errorPages.find(statusCode);
    if (it == server.errorPages.end())
        return (false);
    return (readFileContent(it->second, body));
}