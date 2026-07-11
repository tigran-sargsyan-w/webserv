#include "ErrorResponseBuilder.hpp"
#include "Utils.hpp"

#include <sstream>
#include <string>

/**
 * @brief Creates an empty error response builder.
 */
ErrorResponseBuilder::ErrorResponseBuilder () {}

/**
 * @brief Copies an error response builder.
 * @param other - Builder to copy.
 */
ErrorResponseBuilder::ErrorResponseBuilder (const ErrorResponseBuilder &other)
{
	(void)other;
}

/**
 * @brief Assigns an error response builder.
 * @param other - Builder to assign from.
 * @return Updated builder.
 */
ErrorResponseBuilder &ErrorResponseBuilder::operator= (const ErrorResponseBuilder &other)
{
	(void)other;
	return (*this);
}

/**
 * @brief Destroys the builder.
 */
ErrorResponseBuilder::~ErrorResponseBuilder () {}

/**
 * @brief Builds a default HTML error body.
 * @param statusCode - HTTP status code.
 * @param message - Human-readable message.
 * @return HTML body string.
 */
std::string ErrorResponseBuilder::buildDefaultBody(int statusCode, const std::string &message)
{
    std::ostringstream body;

    body << "<html>";
    body << "<head><title>";
    body << statusCode << " " << message;
    body << "</title></head>";
    body << "<body>";
    body << "<h1>" << statusCode << " " << message << "</h1>";
    body << "</body>";
    body << "</html>";
    return (body.str());
}

/**
 * @brief Builds a response from an HTML body.
 * @param statusCode - HTTP status code.
 * @param body - Response body.
 * @return Complete HTTP response.
 */
Response ErrorResponseBuilder::buildFromBody(int statusCode, const std::string &body)
{
    Response response;

    response.setStatusCode(statusCode);
    response.setBody(body);
    response.addHeader("Content-Type", "text/html");
    response.addHeader("Content-Length", intToString(body.length()));
    response.addHeader("Connection", "close");
    return (response);
}

/**
 * @brief Builds a complete error response.
 * @param statusCode - HTTP status code.
 * @param message - Human-readable message.
 * @return Complete HTTP error response.
 */
Response ErrorResponseBuilder::build(int statusCode, const std::string &message)
{
    return (buildFromBody(statusCode, buildDefaultBody(statusCode, message)));
}