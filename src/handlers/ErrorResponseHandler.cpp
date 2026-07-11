#include "ErrorResponseHandler.hpp"
#include "ErrorPageResolver.hpp"
#include "ErrorResponseBuilder.hpp"

#include <string>

/**
 * @brief Creates an empty error response handler.
 */
ErrorResponseHandler::ErrorResponseHandler() {}

/**
 * @brief Copies an error response handler.
 * @param other - Handler to copy.
 */
ErrorResponseHandler::ErrorResponseHandler(const ErrorResponseHandler &other)
{
    (void)other;
}

/**
 * @brief Assigns an error response handler.
 * @param other - Handler to assign from.
 * @return Updated handler.
 */
ErrorResponseHandler &ErrorResponseHandler::operator=(const ErrorResponseHandler &other)
{
    (void)other;
    return (*this);
}

/**
 * @brief Destroys the handler.
 */
ErrorResponseHandler::~ErrorResponseHandler() {}

/**
 * @brief Builds an error response.
 * Tries a custom error page first, then falls back to a default body.
 * @param statusCode - HTTP status code.
 * @param message - Human-readable message.
 * @param server - Server configuration.
 * @return Complete HTTP error response.
 */
Response ErrorResponseHandler::build(int statusCode, const std::string &message, const ServerConfig &server)
{
    std::string body;

    if (!ErrorPageResolver::resolveBody(statusCode, server, body))
        body = ErrorResponseBuilder::buildDefaultBody(statusCode, message);
    return (ErrorResponseBuilder::buildFromBody(statusCode, body));
}