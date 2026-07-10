#include "ErrorResponseHandler.hpp"
#include "ErrorPageResolver.hpp"
#include "ErrorResponseBuilder.hpp"

#include <string>

ErrorResponseHandler::ErrorResponseHandler() {}

ErrorResponseHandler::ErrorResponseHandler(const ErrorResponseHandler &other)
{
    (void)other;
}

ErrorResponseHandler &ErrorResponseHandler::operator=(const ErrorResponseHandler &other)
{
    (void)other;
    return (*this);
}

ErrorResponseHandler::~ErrorResponseHandler() {}

Response ErrorResponseHandler::build(int statusCode, const std::string &message, const ServerConfig &server)
{
    std::string body;

    if (!ErrorPageResolver::resolveBody(statusCode, server, body))
        body = ErrorResponseBuilder::buildDefaultBody(statusCode, message);
    return (ErrorResponseBuilder::buildFromBody(statusCode, body));
}