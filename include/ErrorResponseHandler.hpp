#ifndef ERRORRESPONSEHANDLER_HPP
#define ERRORRESPONSEHANDLER_HPP

#include "Config.hpp"
#include "Response.hpp"

#include <string>

class ErrorResponseHandler
{
public:
    static Response build(int statusCode, const std::string &message, const ServerConfig &server);

private:
    ErrorResponseHandler();
    ErrorResponseHandler(const ErrorResponseHandler &other);
    ErrorResponseHandler &operator=(const ErrorResponseHandler &other);
    ~ErrorResponseHandler();
};

#endif