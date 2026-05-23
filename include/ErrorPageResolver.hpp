#ifndef ERRORPAGERESOLVER_HPP
#define ERRORPAGERESOLVER_HPP

#include "Config.hpp"

#include <string>

class ErrorPageResolver
{
public:
    static bool resolveBody(int statusCode, const ServerConfig &server, std::string &body);

private:
    ErrorPageResolver();
    ErrorPageResolver(const ErrorPageResolver &other);
    ErrorPageResolver &operator=(const ErrorPageResolver &other);
    ~ErrorPageResolver();
};

#endif